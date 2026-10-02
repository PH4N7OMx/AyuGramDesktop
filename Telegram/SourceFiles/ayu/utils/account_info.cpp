#include "ayu/utils/account_info.h"
#include "ayu/utils/registration_model.h"

#include "core/core_settings.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "settings.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QTimer>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace Ayu::AccountInfo {
namespace {

constexpr auto kMaxRecords = 10000;
constexpr auto kMaxFileSize = 2 * 1024 * 1024;

struct Record {
	QDate registration;
	QString country;
	qint64 updated = 0;
};
struct Cache {
	std::map<uint64, Record> records;
	bool dirty = false;
	bool scheduled = false;
};

std::map<uint64, Cache> &Caches() {
	static auto result = std::map<uint64, Cache>();
	return result;
}

QString Directory() {
	return cWorkingDir() + u"tdata/ayu/account_info/"_q;
}
QString Path(uint64 account) {
	return Directory() + QString::number(account) + u".json"_q;
}

QDate ValidMonth(QDate date) {
	const auto first = QDate(2013, 8, 1);
	const auto now = QDate::currentDate();
	const auto latest = QDate(now.year(), now.month(), 1);
	return (date.isValid() && date.day() == 1 && date >= first && date <= latest)
		? date : QDate();
}

QString ValidCountry(QString country) {
	country = country.toUpper();
	return (country.size() == 2
		&& country.at(0) >= QChar('A') && country.at(0) <= QChar('Z')
		&& country.at(1) >= QChar('A') && country.at(1) <= QChar('Z'))
		? country : QString();
}

Cache &GetCache(uint64 account) {
	auto [it, added] = Caches().try_emplace(account);
	auto &cache = it->second;
	if (!added) {
		return cache;
	}
	auto file = QFile(Path(account));
	if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxFileSize) {
		return cache;
	}
	const auto document = QJsonDocument::fromJson(file.readAll());
	const auto root = document.object();
	if (root.value(u"version"_q).toInt() != 1) {
		return cache;
	}
	const auto records = root.value(u"records"_q).toObject();
	for (auto i = records.begin(); i != records.end(); ++i) {
		auto ok = false;
		const auto id = i.key().toULongLong(&ok);
		if (!ok || !id || cache.records.size() >= kMaxRecords) {
			continue;
		}
		const auto data = i.value().toObject();
		auto record = Record{
			ValidMonth(QDate::fromString(data.value(u"month"_q).toString(), Qt::ISODate)),
			ValidCountry(data.value(u"country"_q).toString()),
			data.value(u"updated"_q).toString().toLongLong(),
		};
		if (record.registration.isValid() || !record.country.isEmpty()) {
			cache.records.emplace(id, std::move(record));
		}
	}
	return cache;
}

void Save(uint64 account) {
	auto &cache = Caches().at(account);
	if (!cache.dirty) {
		return;
	}
	auto records = QJsonObject();
	for (const auto &[id, record] : cache.records) {
		records.insert(QString::number(id), QJsonObject{
			{ u"month"_q, record.registration.toString(Qt::ISODate) },
			{ u"country"_q, record.country },
			{ u"updated"_q, QString::number(record.updated) },
		});
	}
	if (!QDir().mkpath(Directory())) {
		return;
	}
	auto file = QSaveFile(Path(account));
	const auto data = QJsonDocument(QJsonObject{
		{ u"version"_q, 1 }, { u"records"_q, records },
	}).toJson(QJsonDocument::Compact);
	if (file.open(QIODevice::WriteOnly)
		&& file.write(data) == data.size()
		&& file.commit()) {
		cache.dirty = false;
	}
}

void ScheduleSave(uint64 account, Cache &cache) {
	[[maybe_unused]] static const auto connected = [] {
		QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
			QCoreApplication::instance(), [] {
				for (const auto &[account, cache] : Caches()) {
					if (cache.dirty) Save(account);
				}
			});
		return true;
	}();
	if (!cache.dirty || cache.scheduled) return;
	cache.scheduled = true;
	QTimer::singleShot(1000, QCoreApplication::instance(), [account] {
		Caches().at(account).scheduled = false;
		Save(account);
	});
}

} // namespace

bool Observe(not_null<UserData*> user) {
	if (user->isBot()) {
		return false;
	}
	const auto date = ValidMonth(QDate(user->registrationYear(), user->registrationMonth(), 1));
	const auto country = ValidCountry(user->phoneCountryCode());
	if (!date.isValid() && country.isEmpty()) {
		return false;
	}
	const auto account = user->session().userId().bare;
	auto &cache = GetCache(account);
	auto &record = cache.records[peerToUser(user->id).bare];
	const auto changed = (date.isValid() && record.registration != date)
		|| (!country.isEmpty() && record.country != country);
	if (changed) {
		if (date.isValid()) record.registration = date;
		if (!country.isEmpty()) record.country = country;
		record.updated = QDateTime::currentSecsSinceEpoch();
		cache.dirty = true;
		if (cache.records.size() > kMaxRecords) {
			const auto oldest = std::min_element(cache.records.begin(), cache.records.end(),
				[](const auto &a, const auto &b) { return a.second.updated < b.second.updated; });
			cache.records.erase(oldest);
		}
	}
	ScheduleSave(account, cache);
	return changed;
}

QDate KnownRegistration(not_null<UserData*> user) {
	const auto current = ValidMonth(QDate(user->registrationYear(), user->registrationMonth(), 1));
	if (current.isValid()) {
		return current;
	}
	const auto &records = GetCache(user->session().userId().bare).records;
	const auto i = records.find(peerToUser(user->id).bare);
	return (i != records.end()) ? i->second.registration : QDate();
}

QString PhoneCountry(not_null<UserData*> user) {
	const auto current = ValidCountry(user->phoneCountryCode());
	if (!current.isEmpty()) {
		return current;
	}
	const auto &records = GetCache(user->session().userId().bare).records;
	const auto i = records.find(peerToUser(user->id).bare);
	return (i != records.end()) ? i->second.country : QString();
}

QDate EstimateRegistration(not_null<UserData*> user) {
	const auto id = peerToUser(user->id).bare;
	const auto &records = GetCache(user->session().userId().bare).records;
	auto samples = std::vector<RegistrationModel::Sample>();
	for (const auto &[sampleId, record] : records) {
		if (record.registration.isValid()) {
			samples.push_back({ sampleId, record.registration.addDays(14).toJulianDay() });
		}
	}
	const auto prediction = RegistrationModel::Estimate(id, std::move(samples),
		QDate(2013, 8, 1).toJulianDay(), QDate::currentDate().toJulianDay());
	if (!prediction) return {};
	const auto date = QDate::fromJulianDay(*prediction);
	return ValidMonth(QDate(date.year(), date.month(), 1));
}

} // namespace Ayu::AccountInfo
