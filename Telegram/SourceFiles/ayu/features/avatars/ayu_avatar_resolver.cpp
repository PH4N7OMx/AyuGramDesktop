// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/avatars/ayu_avatar_resolver.h"

#include "ayu/ayu_settings.h"
#include "base/random.h"
#include "core/core_settings.h"
#include "data/data_changes.h"
#include "data/data_peer_id.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "settings.h"
#include "ui/image/image_location.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <QtGui/QImage>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace {

constexpr auto kMaxActiveRequests = 2;
constexpr auto kNegativeCacheTtl = crl::time(2 * 3600 * 1000);
constexpr auto kNetworkErrorTtl = crl::time(15 * 60 * 1000);
constexpr auto kRequestTimeoutMs = 5000;
constexpr auto kUserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36";

using UpdateFlag = Data::PeerUpdate::Flag;

bool IsValidAvatarUrl(const QString &url) {
	if (!url.startsWith(u"https://"_q, Qt::CaseInsensitive)) {
		return false;
	}
	const auto isCdn = url.contains(u"telesco.pe"_q, Qt::CaseInsensitive)
		|| url.contains(u"telegram.org/file/"_q, Qt::CaseInsensitive);
	if (!isCdn) {
		return false;
	}
	if (url.endsWith(u".svg"_q, Qt::CaseInsensitive)
		|| url.contains(u"website_icon"_q, Qt::CaseInsensitive)
		|| url.contains(u"t_logo"_q, Qt::CaseInsensitive)) {
		return false;
	}
	return true;
}

QString ExtractAvatarUrl(const QString &html) {
	static const auto kMetaRegex1 = QRegularExpression(
		u"<meta\\s+property=[\"']og:image[\"']\\s+content=[\"']([^\"']+)[\"']"_q,
		QRegularExpression::CaseInsensitiveOption);
	static const auto kMetaRegex2 = QRegularExpression(
		u"<meta\\s+content=[\"']([^\"']+)[\"']\\s+property=[\"']og:image[\"']"_q,
		QRegularExpression::CaseInsensitiveOption);
	static const auto kImgRegex = QRegularExpression(
		u"<img[^>]+class=[\"'][^\"']*tgme_page_photo_image[^\"']*[\"'][^>]+src=[\"']([^\"']+)[\"']"_q,
		QRegularExpression::CaseInsensitiveOption);

	const auto checkMatch = [](const QRegularExpressionMatch &match) -> QString {
		if (match.hasMatch()) {
			auto url = match.captured(1);
			url.replace(u"&amp;"_q, u"&"_q);
			if (IsValidAvatarUrl(url)) {
				return url;
			}
		}
		return {};
	};

	if (const auto url = checkMatch(kMetaRegex1.match(html)); !url.isEmpty()) {
		return url;
	}
	if (const auto url = checkMatch(kMetaRegex2.match(html)); !url.isEmpty()) {
		return url;
	}
	if (const auto url = checkMatch(kImgRegex.match(html)); !url.isEmpty()) {
		return url;
	}
	return {};
}

} // namespace

AyuAvatarResolver &AyuAvatarResolver::Instance() {
	static AyuAvatarResolver instance;
	return instance;
}

AyuAvatarResolver::AyuAvatarResolver() = default;

void AyuAvatarResolver::resolve(not_null<UserData*> user) {
	if (!AyuSettings::getInstance().loadBlockedAvatars()) {
		return;
	}
	if (user->hasUserpic()) {
		return;
	}
	const auto username = user->username();
	if (username.isEmpty()) {
		return;
	}
	const auto usernameLower = username.toLower();

	const auto negIt = _negativeCache.find(usernameLower);
	if (negIt != _negativeCache.end()) {
		if (negIt->second + kNegativeCacheTtl > crl::now()) {
			return;
		}
		_negativeCache.erase(negIt);
	}

	const auto diskDir = cWorkingDir() + u"tdata/ayu/avatars/"_q;
	const auto diskFilePath = diskDir + usernameLower + u".jpg"_q;
	const auto fileInfo = QFileInfo(diskFilePath);
	if (fileInfo.exists() && fileInfo.size() > 0) {
		const auto image = QImage(diskFilePath);
		if (!image.isNull()) {
			applyUserpic(user, image);
			return;
		}
		QFile::remove(diskFilePath);
	}

	if (_inProgress.contains(usernameLower)) {
		return;
	}

	_inProgress.insert(usernameLower);
	_queue.push_back(ResolveTask{
		.userId = peerToUser(user->id),
		.session = base::make_weak(&user->session()),
		.username = username,
	});
	processQueue();
}

void AyuAvatarResolver::processQueue() {
	while (_activeRequests < kMaxActiveRequests && !_queue.empty()) {
		auto task = std::move(_queue.front());
		_queue.pop_front();

		if (!task.session.get()) {
			_inProgress.erase(task.username.toLower());
			continue;
		}

		++_activeRequests;
		fetchHtml(std::move(task));
	}
}

void AyuAvatarResolver::fetchHtml(ResolveTask task) {
	const auto url = QUrl(u"https://t.me/"_q + task.username);
	auto request = QNetworkRequest(url);
	request.setRawHeader("User-Agent", kUserAgent);
	request.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);

	const auto reply = _networkManager.get(request);

	QTimer::singleShot(kRequestTimeoutMs, reply, [reply] {
		if (reply && reply->isRunning()) {
			reply->abort();
		}
	});

	connect(reply, &QNetworkReply::finished, this, [this, reply, task = std::move(task)]() mutable {
		reply->deleteLater();
		const auto usernameLower = task.username.toLower();

		if (reply->error() != QNetworkReply::NoError) {
			_negativeCache[usernameLower] = crl::now() - kNegativeCacheTtl + kNetworkErrorTtl;
			onRequestDone(usernameLower);
			return;
		}

		const auto html = QString::fromUtf8(reply->readAll());
		const auto avatarUrl = ExtractAvatarUrl(html);
		if (avatarUrl.isEmpty()) {
			_negativeCache[usernameLower] = crl::now();
			onRequestDone(usernameLower);
			return;
		}

		fetchImage(std::move(task), avatarUrl);
	});
}

void AyuAvatarResolver::fetchImage(ResolveTask task, const QString &avatarUrl) {
	auto request = QNetworkRequest(QUrl(avatarUrl));
	request.setRawHeader("User-Agent", kUserAgent);

	const auto reply = _networkManager.get(request);

	QTimer::singleShot(kRequestTimeoutMs, reply, [reply] {
		if (reply && reply->isRunning()) {
			reply->abort();
		}
	});

	connect(reply, &QNetworkReply::finished, this, [this, reply, task = std::move(task)]() mutable {
		reply->deleteLater();
		const auto usernameLower = task.username.toLower();

		if (reply->error() != QNetworkReply::NoError) {
			_negativeCache[usernameLower] = crl::now() - kNegativeCacheTtl + kNetworkErrorTtl;
			onRequestDone(usernameLower);
			return;
		}

		const auto bytes = reply->readAll();
		if (bytes.isEmpty()) {
			_negativeCache[usernameLower] = crl::now();
			onRequestDone(usernameLower);
			return;
		}

		const auto image = QImage::fromData(bytes);
		if (image.isNull()) {
			_negativeCache[usernameLower] = crl::now();
			onRequestDone(usernameLower);
			return;
		}

		auto jpegBytes = bytes;
		if (jpegBytes.isEmpty() || !jpegBytes.startsWith("\xFF\xD8")) {
			jpegBytes.clear();
			auto buffer = QBuffer(&jpegBytes);
			buffer.open(QIODevice::WriteOnly);
			image.save(&buffer, "JPG", 87);
		}

		const auto diskDir = cWorkingDir() + u"tdata/ayu/avatars/"_q;
		QDir().mkpath(diskDir);
		const auto filePath = diskDir + usernameLower + u".jpg"_q;
		auto file = QFile(filePath);
		if (file.open(QIODevice::WriteOnly)) {
			file.write(jpegBytes);
			file.close();
		}

		if (const auto session = task.session.get()) {
			const auto user = session->data().user(task.userId);
			if (!user->hasUserpic()) {
				applyUserpic(user, image, jpegBytes);
			}
		}

		onRequestDone(usernameLower);
	});
}

void AyuAvatarResolver::onRequestDone(const QString &usernameLower) {
	_inProgress.erase(usernameLower);
	--_activeRequests;
	processQueue();
}

void AyuAvatarResolver::applyUserpic(
		not_null<UserData*> user,
		const QImage &image,
		QByteArray bytes) {
	if (bytes.isEmpty()) {
		auto buffer = QBuffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "JPG", 87);
	}
	user->setUserpic(
		base::RandomValue<PhotoId>(),
		ImageLocation(
			{ .data = InMemoryLocation{ .bytes = bytes } },
			image.width(),
			image.height()),
		false);
	user->session().changes().peerUpdated(user, UpdateFlag::Photo);
}
