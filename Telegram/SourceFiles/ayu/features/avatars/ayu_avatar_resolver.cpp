// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/avatars/ayu_avatar_resolver.h"

#include "ayu/ayu_settings.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "core/core_settings.h"
#include "data/data_changes.h"
#include "data/data_peer_id.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "settings.h"
#include "ui/image/image_location.h"
#include "ui/image/image_location_factory.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <QtCore/QSaveFile>
#include <QtGui/QImageReader>
#include <QtGui/QImage>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace {

constexpr auto kMaxActiveRequests = 2;
constexpr auto kNegativeCacheTtl = crl::time(2 * 3600 * 1000);
constexpr auto kNetworkErrorTtl = crl::time(15 * 60 * 1000);
constexpr auto kHtmlRecheckInterval = crl::time(5 * 60 * 1000);
constexpr auto kRequestTimeoutMs = 5000;
constexpr auto kMaxHtmlBytes = 1024 * 1024;
constexpr auto kMaxImageBytes = 8 * 1024 * 1024;
constexpr auto kMaxImageDimension = 4096;
constexpr auto kMaxQueuedRequests = 256;
constexpr auto kUserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36";

using UpdateFlag = Data::PeerUpdate::Flag;

bool IsValidAvatarUrl(const QString &url) {
	if (!url.startsWith(u"https://"_q, Qt::CaseInsensitive)) {
		return false;
	}
	const auto parsed = QUrl(url);
	const auto host = parsed.host().toLower();
	const auto isCdn = host == u"telesco.pe"_q
		|| host.endsWith(u".telesco.pe"_q)
		|| (host == u"telegram.org"_q
			&& parsed.path().startsWith(u"/file/"_q));
	if (!isCdn || !parsed.userInfo().isEmpty()
		|| (parsed.port() != -1 && parsed.port() != 443)) {
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

QImage DecodeAvatar(const QByteArray &bytes) {
	if (bytes.isEmpty() || bytes.size() > kMaxImageBytes) {
		return {};
	}
	auto buffer = QBuffer();
	buffer.setData(bytes);
	buffer.open(QIODevice::ReadOnly);
	auto reader = QImageReader(&buffer);
	const auto size = reader.size();
	if (!size.isValid() || size.width() > kMaxImageDimension
		|| size.height() > kMaxImageDimension) {
		return {};
	}
	return reader.read();
}

QString UserpicKey(not_null<UserData*> user) {
	return QString::number(user->session().userId().bare)
		+ u"_"_q + QString::number(peerToUser(user->id).bare);
}

QString AvatarsDir() {
	return cWorkingDir() + u"tdata/ayu/avatars/"_q;
}

QString ImageFilePath(const QString &cacheKey) {
	return AvatarsDir() + cacheKey + u".jpg"_q;
}

QString UrlFilePath(const QString &cacheKey) {
	return AvatarsDir() + cacheKey + u".url"_q;
}

QString ReadCachedUrl(const QString &cacheKey) {
	auto file = QFile(UrlFilePath(cacheKey));
	if (!file.open(QIODevice::ReadOnly)) {
		return {};
	}
	return QString::fromUtf8(file.readAll()).trimmed();
}

void WriteCachedUrl(const QString &cacheKey, const QString &url) {
	QDir().mkpath(AvatarsDir());
	auto file = QSaveFile(UrlFilePath(cacheKey));
	if (file.open(QIODevice::WriteOnly)) {
		file.write(url.toUtf8());
		file.commit();
	}
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

	if (user->isSelf() || user->isBot() || user->isSupport() || user->isInaccessible()) {
		return;
	}

	if (!user->lastseen().isLongAgo()) {
		return;
	}

	const auto userId = peerToUser(user->id);
	const auto username = user->username();
	static const auto validUsername = QRegularExpression(u"^[A-Za-z0-9_]+$"_q);
	if (!validUsername.match(username).hasMatch()) {
		return;
	}
	const auto cacheKey = QString::number(user->session().userId().bare)
		+ u"_"_q + QString::number(userId.bare)
		+ u"_"_q + username.toLower();

	if (user->hasUserpic()) {
		const auto it = _appliedPhotoIds.find(UserpicKey(user));
		if (it == _appliedPhotoIds.end() || it->second != user->userpicPhotoId()) {
			return;
		}
	}

	if (!user->hasUserpic()) {
		applyFromDiskCache(user, cacheKey);
	}

	const auto negIt = _negativeCache.find(cacheKey);
	if (negIt != _negativeCache.end()) {
		if (negIt->second > crl::now()) {
			return;
		}
		_negativeCache.erase(negIt);
	}

	const auto checkIt = _lastHtmlCheck.find(cacheKey);
	if (checkIt != _lastHtmlCheck.end()
		&& checkIt->second + kHtmlRecheckInterval > crl::now()) {
		if (!user->hasUserpic()) {
			applyFromDiskCache(user, cacheKey);
		}
		return;
	}

	if (_inProgress.contains(cacheKey)) {
		return;
	}

	if (_queue.size() >= kMaxQueuedRequests) {
		return;
	}
	_inProgress.insert(cacheKey);
	_queue.push_back(ResolveTask{
		.userId = userId,
		.cacheKey = cacheKey,
		.session = base::make_weak(&user->session()),
		.username = username,
	});
	processQueue();
}

bool AyuAvatarResolver::applyFromDiskCache(not_null<UserData*> user, const QString &cacheKey) {
	const auto imgPath = ImageFilePath(cacheKey);
	const auto fi = QFileInfo(imgPath);
	if (!fi.exists() || fi.size() <= 0 || fi.size() > kMaxImageBytes) {
		return false;
	}
	auto file = QFile(imgPath);
	if (file.open(QIODevice::ReadOnly)) {
		const auto bytes = file.readAll();
		const auto image = DecodeAvatar(bytes);
		if (!image.isNull()) {
			applyUserpic(user, image, bytes);
			return true;
		}
	}
	return false;
}

void AyuAvatarResolver::processQueue() {
	while (_activeRequests < kMaxActiveRequests && !_queue.empty()) {
		auto task = std::move(_queue.front());
		_queue.pop_front();

		if (!currentUser(task)) {
			_inProgress.erase(task.cacheKey);
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
	const auto limit = (url.host() == u"t.me"_q) ? kMaxHtmlBytes : kMaxImageBytes;
	connect(reply, &QIODevice::readyRead, reply, [=] {
		if (reply->bytesAvailable() > limit) {
			reply->abort();
		}
	});

	QTimer::singleShot(kRequestTimeoutMs, reply, [reply] {
		if (reply && reply->isRunning()) {
			reply->abort();
		}
	});

	connect(reply, &QNetworkReply::finished, this, [this, reply, task = std::move(task)]() mutable {
		reply->deleteLater();
		if (!currentUser(task)) {
			onRequestDone(task.cacheKey);
			return;
		}

		if (reply->error() != QNetworkReply::NoError) {
			_negativeCache[task.cacheKey] = crl::now() + kNetworkErrorTtl;
			onRequestDone(task.cacheKey);
			return;
		}

		const auto html = QString::fromUtf8(reply->readAll());
		const auto avatarUrl = ExtractAvatarUrl(html);

		_lastHtmlCheck[task.cacheKey] = crl::now();

		if (avatarUrl.isEmpty()) {
			_negativeCache[task.cacheKey] = crl::now() + kNegativeCacheTtl;
			QFile::remove(ImageFilePath(task.cacheKey));
			QFile::remove(UrlFilePath(task.cacheKey));
			if (const auto session = task.session.get()) {
				const auto user = session->data().user(task.userId);
				const auto it = _appliedPhotoIds.find(UserpicKey(user));
				if (it != _appliedPhotoIds.end() && it->second == user->userpicPhotoId()) {
					user->setUserpic(PhotoId(), ImageLocation(), false);
					user->session().changes().peerUpdated(user, UpdateFlag::Photo);
				}
			}
			_appliedPhotoIds.erase(UserpicKey(currentUser(task)));
			onRequestDone(task.cacheKey);
			return;
		}

		const auto cachedUrl = ReadCachedUrl(task.cacheKey);
		if (cachedUrl == avatarUrl && QFileInfo(ImageFilePath(task.cacheKey)).size() > 0) {
			if (const auto session = task.session.get()) {
				const auto user = session->data().user(task.userId);
				if (!user->hasUserpic() && !applyFromDiskCache(user, task.cacheKey)) {
					fetchImage(std::move(task), avatarUrl);
					return;
				}
			}
			onRequestDone(task.cacheKey);
			return;
		}

		fetchImage(std::move(task), avatarUrl);
	});
}

void AyuAvatarResolver::fetchImage(ResolveTask task, const QString &avatarUrl) {
	const auto url = QUrl(avatarUrl);
	auto request = QNetworkRequest(url);
	request.setRawHeader("User-Agent", kUserAgent);

	const auto reply = _networkManager.get(request);
	const auto limit = (url.host() == u"t.me"_q) ? kMaxHtmlBytes : kMaxImageBytes;
	connect(reply, &QIODevice::readyRead, reply, [=] {
		if (reply->bytesAvailable() > limit) {
			reply->abort();
		}
	});

	QTimer::singleShot(kRequestTimeoutMs, reply, [reply] {
		if (reply && reply->isRunning()) {
			reply->abort();
		}
	});

	connect(reply, &QNetworkReply::finished, this, [this, reply, task = std::move(task), avatarUrl]() mutable {
		reply->deleteLater();
		if (!currentUser(task)) {
			onRequestDone(task.cacheKey);
			return;
		}

		if (reply->error() != QNetworkReply::NoError) {
			_negativeCache[task.cacheKey] = crl::now() + kNetworkErrorTtl;
			onRequestDone(task.cacheKey);
			return;
		}

		const auto bytes = reply->readAll();
		if (bytes.isEmpty()) {
			_negativeCache[task.cacheKey] = crl::now() + kNegativeCacheTtl;
			onRequestDone(task.cacheKey);
			return;
		}

		const auto image = DecodeAvatar(bytes);
		if (image.isNull()) {
			_negativeCache[task.cacheKey] = crl::now() + kNegativeCacheTtl;
			onRequestDone(task.cacheKey);
			return;
		}

		auto jpegBytes = bytes;
		if (jpegBytes.isEmpty() || !jpegBytes.startsWith("\xFF\xD8")) {
			jpegBytes.clear();
			auto buffer = QBuffer(&jpegBytes);
			buffer.open(QIODevice::WriteOnly);
			if (!image.save(&buffer, "JPG", 87)) {
				_negativeCache[task.cacheKey] = crl::now() + kNetworkErrorTtl;
				onRequestDone(task.cacheKey);
				return;
			}
		}

		QDir().mkpath(AvatarsDir());
		auto imgFile = QSaveFile(ImageFilePath(task.cacheKey));
		if (imgFile.open(QIODevice::WriteOnly)
			&& imgFile.write(jpegBytes) == jpegBytes.size()
			&& imgFile.commit()) {
			WriteCachedUrl(task.cacheKey, avatarUrl);
		}

		if (const auto session = task.session.get()) {
			const auto user = session->data().user(task.userId);
			const auto it = _appliedPhotoIds.find(UserpicKey(user));
			const auto isOurUserpic = (it != _appliedPhotoIds.end() && it->second == user->userpicPhotoId());
			if (!user->hasUserpic() || isOurUserpic) {
				applyUserpic(user, image, jpegBytes);
			}
		}

		onRequestDone(task.cacheKey);
	});
}

UserData *AyuAvatarResolver::currentUser(const ResolveTask &task) const {
	const auto session = task.session.get();
	if (!session || !AyuSettings::getInstance().loadBlockedAvatars()) {
		return nullptr;
	}
	const auto user = session->data().user(task.userId);
	return user->username() == task.username
		&& user->lastseen().isLongAgo()
		&& !user->isSelf() && !user->isBot()
		&& !user->isSupport() && !user->isInaccessible()
		? user.get() : nullptr;
}

void AyuAvatarResolver::onRequestDone(const QString &cacheKey) {
	_inProgress.erase(cacheKey);
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
	const auto photoId = base::RandomValue<PhotoId>();
	const auto imgWithLoc = Images::FromImageInMemory(image, "JPG", bytes);

	const auto photo = user->owner().photo(photoId);
	photo->peer = user;
	photo->setFields(base::unixtime::now(), false);

	const auto media = photo->createMediaView();
	photo->updateImages(
		QByteArray(),
		imgWithLoc,
		imgWithLoc,
		imgWithLoc,
		ImageWithLocation(),
		ImageWithLocation(),
		0);
	user->owner().keepAlive(media);

	_appliedPhotoIds[UserpicKey(user)] = photoId;

	user->setUserpic(photoId, imgWithLoc.location, false);
	user->session().changes().peerUpdated(user, UpdateFlag::Photo);
}
