// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/filters/filters_controller.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/filters/filters_cache_controller.h"
#include "ayu/features/filters/filters_utils.h"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "history/view/history_view_element.h"
#include "unicode/regex.h"

#include <memory>
#include <unordered_set>

namespace FiltersController {

std::unordered_set<long long> showingFilteredMessages;

bool filterBlocked(const not_null<HistoryItem*> item) {
	if (item->from() != item->history()->peer) {
		if (isBlocked(item)) {
			return true;
		}
	}
	if (const auto bot = item->viaBot()) {
		if (isBlocked(bot)) {
			return true;
		}
	}
	return false;
}

std::optional<bool> isFiltered(
		const QString &str,
		long long dialogId,
		const std::shared_ptr<const FiltersCacheController::Cache> &cache) {
	if (str.isEmpty()) {
		return std::nullopt;
	}

	const auto icuStr = icu::UnicodeString(reinterpret_cast<const UChar*>(str.constData()), str.length());

	const auto matches = [&](const ReversiblePattern &pattern)
	{
		UErrorCode status = U_ZERO_ERROR;

		const auto matcher = std::unique_ptr<icu::RegexMatcher>(pattern.pattern->matcher(icuStr, status));
		if (U_FAILURE(status) || !matcher) {
			LOG(("FILTER FAILED: %1").arg(u_errorName(status)));
			return false;
		}

		const auto match = matcher->find();
		const auto reversed = pattern.reversed;

		if ((!reversed && match) || (reversed && !match)) {
			return true;
		}
		return false;
	};

	if (const auto i = cache->patternsByDialogId.find(dialogId); i != cache->patternsByDialogId.end()) {
		for (const auto &pattern : i->second) {
			if (matches(pattern)) {
				return true;
			}
		}
	}

	const auto exclusions = cache->exclusionsByDialogId.find(dialogId);
	if (!cache->sharedPatterns.empty()) {
		for (const auto &pattern : cache->sharedPatterns) {
			if (exclusions != cache->exclusionsByDialogId.end() && exclusions->second.contains(pattern)) {
				continue;
			}
			if (matches(pattern.pattern)) {
				return true;
			}
		}
	}
	return false;
}

bool isEnabled(not_null<PeerData*> peer) {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.filtersEnabled()) {
		return false;
	}
	if (peer->isBroadcast()) {
		return settings.filtersEnabledInChannels();
	}
	if (peer->isChat() || peer->isMegagroup()) {
		return settings.filtersEnabledInGroups();
	}
	if (peer->isUser()) {
		return settings.filtersEnabledInPrivate();
	}
	return true;
}

bool isBlocked(const not_null<HistoryItem*> item) {
	const auto &settings = AyuSettings::getInstance();

	auto shadowBanMatched = false;
	const auto blocked = [&]() -> bool
	{
		const auto isShadowBanned = [&](PeerData *peer) {
			return peer
				&& (peer->isUser() || peer->isBroadcast())
				&& settings.isShadowBanned(getDialogIdFromPeer(peer));
		};

		if (isShadowBanned(item->from())
			&& item->from()->id != item->history()->peer->id) {
			shadowBanMatched = true;
			return true;
		}

		if (item->from()->isUser()
			&& item->from()->asUser()->isBlocked()) {
			// don't hide messages if it's a dialog with blocked user
			return item->from()->asUser()->id != item->history()->peer->id;
		}

		if (const auto forwarded = item->Get<HistoryMessageForwarded>()) {
			if (const auto originalSender = forwarded->originalSender) {
				const auto originalShadowBanned = isShadowBanned(originalSender);
				if (originalShadowBanned
					|| (originalSender->isUser()
						&& originalSender->asUser()->isBlocked())) {
					shadowBanMatched = originalShadowBanned;
					return true;
				}
			}
		}
		return false;
	}();

	return settings.filtersEnabled()
		&& (shadowBanMatched || settings.hideFromBlocked())
		&& blocked;
}

bool isBlocked(const not_null<PeerData*> peer) {
	const auto &settings = AyuSettings::getInstance();
	return settings.filtersEnabled() &&
	(
		(peer->isUser() && peer->asUser()->isBlocked() && settings.hideFromBlocked()) ||
		((peer->isUser() || peer->isBroadcast()) && settings.isShadowBanned(getDialogIdFromPeer(peer)))
	);
}

static base::flat_set<not_null<const HistoryItem*>> notifiedDuplicates;
static const HistoryItem *s_removingItem = nullptr;

const HistoryItem *removingItem() {
	return s_removingItem;
}

std::vector<not_null<HistoryItem*>> getSortedHistoryItems(not_null<History*> history) {
	std::vector<not_null<HistoryItem*>> list;
	list.reserve(history->items().size());
	for (const auto &it : history->items()) {
		const auto item = it.get();
		if (!item || item == s_removingItem || item->isService()) {
			continue;
		}
		list.push_back(item);
	}
	ranges::sort(list, [](not_null<HistoryItem*> a, not_null<HistoryItem*> b) {
		if (a->id != b->id) {
			return a->id < b->id;
		}
		return a->date() < b->date();
	});
	return list;
}

const HistoryItem *getDuplicateHead(const not_null<const HistoryItem*> item) {
	if (item.get() == s_removingItem || !AyuSettings::getInstance().collapseDuplicates() || item->isService()) {
		return nullptr;
	}
	const QString text = item->originalText().text;
	if (text.isEmpty()) {
		return nullptr;
	}

	const auto history = item->history();
	const auto list = getSortedHistoryItems(history);
	const auto it = ranges::find(list, item.get(), [](not_null<HistoryItem*> i) { return i.get(); });
	if (it == list.begin() || it == list.end()) {
		return nullptr;
	}

	const HistoryItem *head = nullptr;
	auto curr = it;
	while (curr != list.begin()) {
		--curr;
		const auto prev = *curr;
		if (prev->from() == item->from() && prev->originalText().text == text) {
			head = prev;
		} else {
			break;
		}
	}

	return head;
}

bool isDuplicateMessage(const not_null<HistoryItem*> item) {
	if (item.get() == s_removingItem || !AyuSettings::getInstance().collapseDuplicates() || item->isService()) {
		return false;
	}
	return (getDuplicateHead(item) != nullptr);
}

std::vector<not_null<HistoryItem*>> getDuplicateGroup(not_null<HistoryItem*> item) {
	std::vector<not_null<HistoryItem*>> result;
	if (!AyuSettings::getInstance().collapseDuplicates() || item->isService()) {
		result.push_back(item);
		return result;
	}
	const QString text = item->originalText().text;
	if (text.isEmpty()) {
		result.push_back(item);
		return result;
	}

	const auto head = getDuplicateHead(item);
	const auto realHead = head ? const_cast<HistoryItem*>(head) : item.get();

	const auto history = realHead->history();
	const auto list = getSortedHistoryItems(history);
	const auto it = ranges::find(list, realHead, [](not_null<HistoryItem*> i) { return i.get(); });
	if (it == list.end()) {
		result.push_back(realHead);
		return result;
	}

	for (auto curr = it; curr != list.end(); ++curr) {
		const auto msg = *curr;
		if (msg->from() == realHead->from() && msg->originalText().text == text) {
			result.push_back(msg);
		} else {
			break;
		}
	}

	return result;
}

int countDuplicateGroupSize(const not_null<HistoryItem*> item) {
	return static_cast<int>(getDuplicateGroup(item).size());
}

void handleDuplicateItemRemoved(not_null<const HistoryItem*> item) {
	s_removingItem = item.get();
	notifiedDuplicates.remove(item);

	const auto history = item->history();
	const auto group = getDuplicateGroup(const_cast<HistoryItem*>(item.get()));

	HistoryItem *nextHead = nullptr;
	if (!group.empty() && group.front() == item.get() && group.size() > 1) {
		nextHead = group[1];
	}

	if (nextHead) {
		notifiedDuplicates.remove(nextHead);
		const auto nextHeadPtr = nextHead;
		crl::on_main([=] {
			s_removingItem = nullptr;
			nextHeadPtr->history()->owner().requestItemViewRefresh(nextHeadPtr);
		});
	} else {
		crl::on_main([] { s_removingItem = nullptr; });
	}
}

bool isBlockedOrRegexFiltered(const not_null<HistoryItem*> item) {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.filtersEnabled()) {
		return false;
	}

	if (item->out()) {
		return false;
	}

	if (filterBlocked(item)) {
		FiltersCacheController::putHiddenBlockedMessage(item);
		return true;
	}

	if (!isEnabled(item->history()->peer)) return false;

	const auto cached = FiltersCacheController::isFiltered(item);
	if (cached.has_value()) {
		return cached.value();
	}
	const auto group = item->history()->owner().groups().find(item);
	const auto cache = FiltersCacheController::snapshot();
	const auto res = isFiltered(
		FilterUtils::extractAllText(item, group),
		getDialogIdFromPeer(item->history()->peer),
		cache);

	if (res.has_value()) {
		FiltersCacheController::putFiltered(item, group, res.value(), cache);
		return res.value();
	}
	return false;
}

bool filtered(const not_null<HistoryItem*> item) {
	if (showingFilteredMessages.contains(item->history()->peer->id.value)) {
		return false;
	}

	const auto &settings = AyuSettings::getInstance();
	if (!settings.filtersEnabled()) {
		return false;
	}

	if (!isEnabled(item->history()->peer)) return false;

	if (settings.collapseDuplicates() && isDuplicateMessage(item)) {
		return true;
	}

	return isBlockedOrRegexFiltered(item);
}

std::optional<bool> filteredMessagesShown(not_null<PeerData*> peer) {
	if (!showingFilteredMessages.contains(peer->id.value)
		&& !FiltersCacheController::hasFilteredMessages(peer)) {
		return std::nullopt;
	}
	return showingFilteredMessages.contains(peer->id.value);
}

void toggleFilteredMessagesShown(not_null<PeerData*> peer) {
	if (showingFilteredMessages.contains(peer->id.value)) {
		showingFilteredMessages.erase(peer->id.value);
	} else {
		showingFilteredMessages.insert(peer->id.value);
	}
	FiltersCacheController::fireUpdate();
}

void invalidate(not_null<HistoryItem*> item) {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.filtersEnabled()) {
		return;
	}

	FiltersCacheController::invalidate(item);
}

}
