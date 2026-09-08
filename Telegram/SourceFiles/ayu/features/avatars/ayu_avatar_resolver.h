// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "base/flat_map.h"
#include "base/flat_set.h"
#include "base/weak_ptr.h"
#include "crl/crl_time.h"
#include "data/data_types.h"

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkAccessManager>
#include <deque>

class QImage;
class UserData;

namespace Main {
class Session;
} // namespace Main

class AyuAvatarResolver final : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY(AyuAvatarResolver)

public:
	static AyuAvatarResolver &Instance();

	void resolve(not_null<UserData*> user);

private:
	struct ResolveTask {
		UserId userId = 0;
		base::weak_ptr<Main::Session> session;
		QString username;
	};

	AyuAvatarResolver();
	~AyuAvatarResolver() override = default;

	void processQueue();
	void fetchHtml(ResolveTask task);
	void fetchImage(ResolveTask task, const QString &avatarUrl);
	void onRequestDone(const QString &usernameLower);
	void applyUserpic(
		not_null<UserData*> user,
		const QImage &image,
		QByteArray bytes = {});

	QNetworkAccessManager _networkManager;
	std::deque<ResolveTask> _queue;
	base::flat_set<QString> _inProgress;
	base::flat_map<QString, crl::time> _negativeCache;
	int _activeRequests = 0;
};

namespace Ayu {
using AyuAvatarResolver = ::AyuAvatarResolver;
} // namespace Ayu
