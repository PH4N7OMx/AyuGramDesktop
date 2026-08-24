// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "data/data_star_gift.h"
#include "rpl/event_stream.h"
#include "rpl/producer.h"

namespace Main {
class Session;
} // namespace Main

namespace Ayu::DeletedGifts {

struct DeletedGiftDef {
	uint64 id = 0;
	int64 stars = 50;
	int stickerIndex = 0;
	QString title;
};

[[nodiscard]] const std::vector<DeletedGiftDef> &GetDefinitions();

class Manager final {
public:
	static Manager &instance();

	void load(not_null<Main::Session*> session);
	[[nodiscard]] rpl::producer<> sessionUpdated(not_null<Main::Session*> session);

	[[nodiscard]] DocumentData *lookupSticker(
		not_null<Main::Session*> session,
		int index,
		DocumentData *fallback) const;

	[[nodiscard]] std::vector<Data::StarGift> injectGifts(
		not_null<Main::Session*> session,
		const std::vector<Data::StarGift> &originalGifts);

private:
	Manager() = default;

	struct SessionState {
		uint64 packId = 0;
		uint64 accessHash = 0;
		std::vector<DocumentData*> documents;
		mtpRequestId requestId = 0;
		rpl::event_stream<> updated;
	};

	base::flat_map<not_null<Main::Session*>, SessionState> _sessions;
};

} // namespace Ayu::DeletedGifts
