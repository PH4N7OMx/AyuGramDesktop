// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/deleted_gifts/deleted_gifts.h"

#include "apiwrap.h"
#include "data/data_document.h"
#include "data/data_session.h"
#include "main/main_session.h"

namespace Ayu::DeletedGifts {

const std::vector<DeletedGiftDef> &GetDefinitions() {
	static const std::vector<DeletedGiftDef> kList = {
		{ 5956217000635139069ULL, 50, 0, QString::fromUtf8("Новогодний мишка") },
		{ 5922558454332916696ULL, 50, 1, QString::fromUtf8("Елочка") },
		{ 5800655655995968830ULL, 50, 2, QString::fromUtf8("Мишка на 14 февраля") },
		{ 5866352046986232958ULL, 50, 3, QString::fromUtf8("Мишка на 8 марта") },
		{ 5801108895304779062ULL, 50, 4, QString::fromUtf8("Валентинка на 14 февраля") },
		{ 5893356958802511476ULL, 50, 5, QString::fromUtf8("Мишка лепрекон") },
		{ 5935895822435615975ULL, 50, 6, QString::fromUtf8("Мишка на 1 апреля") },
		{ 5969796561943660080ULL, 50, 7, QString::fromUtf8("Мишка на пасху") },
		{ 6026193266406327981ULL, 50, 8, QString::fromUtf8("Мишка строитель") },
		{ 5974210632977745012ULL, 50, 9, QString::fromUtf8("Мишка на чемпионате") },
		{ 6046178578163303744ULL, 50, 10, QString::fromUtf8("Мишка террорист") },
	};
	return kList;
}

Manager &Manager::instance() {
	static Manager instance;
	return instance;
}

rpl::producer<> Manager::sessionUpdated(not_null<Main::Session*> session) {
	return _sessions[session].updated.events();
}

void Manager::load(not_null<Main::Session*> session) {
	auto &state = _sessions[session];
	if (state.requestId || !state.documents.empty()) {
		return;
	}

	session->lifetime().add([this, session] {
		_sessions.remove(session);
	});

	state.requestId = session->api().request(MTPmessages_GetStickerSet(
		MTP_inputStickerSetShortName(MTP_string("DeletedGiftsStickers")),
		MTP_int(0)
	)).done([this, session](const MTPmessages_StickerSet &result) {
		auto it = _sessions.find(session);
		if (it == _sessions.end()) {
			return;
		}
		auto &state = it->second;
		state.requestId = 0;
		result.match([&](const MTPDmessages_stickerSet &data) {
			state.packId = data.vset().data().vid().v;
			state.accessHash = data.vset().data().vaccess_hash().v;
			state.documents.clear();
			for (const auto &sticker : data.vdocuments().v) {
				const auto doc = session->data().processDocument(sticker);
				if (doc->sticker()) {
					state.documents.push_back(doc);
				}
			}
			state.updated.fire({});
		}, [](const auto &) {});
	}).fail([this, session] {
		auto it = _sessions.find(session);
		if (it != _sessions.end()) {
			it->second.requestId = 0;
		}
	}).send();
}

DocumentData *Manager::lookupSticker(
		not_null<Main::Session*> session,
		int index,
		DocumentData *fallback) const {
	const auto it = _sessions.find(session);
	if (it == _sessions.end() || it->second.documents.empty()) {
		return fallback;
	}
	const auto &docs = it->second.documents;
	int actualIndex = index;
	if (docs.size() >= 12) {
		actualIndex = index + 1;
	}
	if (actualIndex >= 0 && actualIndex < int(docs.size())) {
		return docs[actualIndex];
	} else if (!docs.empty()) {
		return docs.front();
	}
	return fallback;
}

std::vector<Data::StarGift> Manager::injectGifts(
		not_null<Main::Session*> session,
		const std::vector<Data::StarGift> &originalGifts) {
	load(session);

	base::flat_set<uint64> existingIds;
	DocumentData *donorDoc = nullptr;
	std::shared_ptr<Data::StarGiftBackground> donorBg = nullptr;
	for (const auto &gift : originalGifts) {
		existingIds.insert(gift.id);
		if (!donorDoc && gift.document) {
			donorDoc = gift.document.get();
			donorBg = gift.background;
		}
	}
	if (!donorDoc) {
		return originalGifts;
	}

	auto result = originalGifts;
	int insertPos = 0;
	for (const auto &gift : result) {
		if (!gift.unique && !gift.limitedCount && !gift.soldOut) {
			++insertPos;
		} else {
			break;
		}
	}
	if (insertPos == 0) {
		insertPos = std::min(int(result.size()), 11);
	}

	const auto &defs = GetDefinitions();
	int added = 0;
	for (const auto &def : defs) {
		if (existingIds.contains(def.id)) {
			continue;
		}
		const auto doc = lookupSticker(session, def.stickerIndex, donorDoc);
		if (!doc) {
			continue;
		}
		Data::StarGift gift{
			.id = def.id,
			.background = donorBg,
			.stars = def.stars,
			.starsConverted = (def.stars * 85) / 100,
			.document = not_null{ doc },
			.resellTitle = def.title,
		};
		result.insert(result.begin() + std::min(insertPos + added, int(result.size())), std::move(gift));
		++added;
	}
	return result;
}

} // namespace Ayu::DeletedGifts
