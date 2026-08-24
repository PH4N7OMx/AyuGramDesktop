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

namespace {

constexpr auto kDuplicateFastLookupLimit = 200;
constexpr auto kMaxTrackedDuplicates = 2000;

enum class DuplicateLookupDirection {
	Previous,
	Next,
};

base::flat_set<FullMsgId> notifiedDuplicates;
base::flat_set<FullMsgId> removingDuplicates;

[[nodiscard]] bool DuplicateCollapsingEnabled(
		not_null<const HistoryItem*> item) {
	const auto peer = item->history()->peer;
	return AyuSettings::getInstance().collapseDuplicates()
		&& isEnabled(peer)
		&& !showingFilteredMessages.contains(peer->id.value);
}

[[nodiscard]] bool IsRemoving(not_null<const HistoryItem*> item) {
	return removingDuplicates.contains(item->fullId());
}

[[nodiscard]] bool IsDuplicateCandidate(
		not_null<const HistoryItem*> item) {
	return DuplicateCollapsingEnabled(item)
		&& !IsRemoving(item)
		&& !item->isService()
		&& (item->id > 0)
		&& !item->originalText().text.isEmpty();
}

[[nodiscard]] bool SameDuplicateContent(
		not_null<const HistoryItem*> first,
		not_null<const HistoryItem*> second) {
	return first->from() == second->from()
		&& first->originalText() == second->originalText()
		&& first->replyTo() == second->replyTo();
}

[[nodiscard]] HistoryItem *LookupClosestLoadedMessage(
		not_null<const HistoryItem*> item,
		DuplicateLookupDirection direction) {
	auto result = static_cast<HistoryItem*>(nullptr);
	for (const auto &entry : item->history()->items()) {
		const auto candidate = entry.get();
		if (candidate == item.get()
			|| candidate->id <= 0
			|| IsRemoving(candidate)
			|| candidate->isService()) {
			continue;
		}
		if (direction == DuplicateLookupDirection::Previous) {
			if (candidate->id < item->id
				&& (!result || candidate->id > result->id)) {
				result = candidate;
			}
		} else if (candidate->id > item->id
			&& (!result || candidate->id < result->id)) {
			result = candidate;
		}
	}
	return result;
}

[[nodiscard]] HistoryItem *LookupAdjacentMessage(
		not_null<const HistoryItem*> item,
		DuplicateLookupDirection direction) {
	if (item->id <= 0) {
		return nullptr;
	}
	const auto peerId = item->history()->peer->id;
	const auto &owner = item->history()->owner();
	for (auto offset = 1; offset <= kDuplicateFastLookupLimit; ++offset) {
		const auto id = (direction == DuplicateLookupDirection::Previous)
			? (item->id - offset)
			: (item->id + offset);
		if (id <= 0) {
			break;
		}
		const auto adjacent = owner.message(peerId, id);
		if (!adjacent || IsRemoving(adjacent) || adjacent->isService()) {
			continue;
		}
		return adjacent;
	}
	return LookupClosestLoadedMessage(item, direction);
}

void NotifyDuplicateHead(
		not_null<const HistoryItem*> duplicate,
		not_null<const HistoryItem*> head) {
	const auto duplicateId = duplicate->fullId();
	if (notifiedDuplicates.contains(duplicateId)) {
		return;
	}
	if (notifiedDuplicates.size() >= kMaxTrackedDuplicates) {
		notifiedDuplicates.clear();
	}
	notifiedDuplicates.emplace(duplicateId);

	const auto owner = &head->history()->owner();
	const auto headId = head->fullId();
	crl::on_main([=] {
		if (const auto current = owner->message(headId)) {
			owner->requestItemViewRefresh(current);
		}
	});
}

} // namespace

const HistoryItem *getDuplicateHead(
		const not_null<const HistoryItem*> item) {
	if (!IsDuplicateCandidate(item)) {
		return nullptr;
	}
	const auto previous = LookupAdjacentMessage(
		item,
		DuplicateLookupDirection::Previous);
	if (!previous || !SameDuplicateContent(previous, item)) {
		return nullptr;
	}

	auto head = static_cast<const HistoryItem*>(previous);
	while (const auto earlier = LookupAdjacentMessage(
			head,
			DuplicateLookupDirection::Previous)) {
		if (!SameDuplicateContent(earlier, item)) {
			break;
		}
		head = earlier;
	}
	return head;
}

bool isDuplicateMessage(const not_null<HistoryItem*> item) {
	const auto head = getDuplicateHead(item);
	if (!head) {
		return false;
	}
	NotifyDuplicateHead(item, head);
	return true;
}

std::vector<not_null<HistoryItem*>> getDuplicateGroup(
		not_null<HistoryItem*> item) {
	auto result = std::vector<not_null<HistoryItem*>>();
	if (!IsDuplicateCandidate(item)) {
		result.push_back(item);
		return result;
	}

	const auto head = getDuplicateHead(item);
	const auto first = head ? const_cast<HistoryItem*>(head) : item.get();
	result.push_back(first);

	for (auto next = LookupAdjacentMessage(
			first,
			DuplicateLookupDirection::Next);
		next && SameDuplicateContent(first, next);
		next = LookupAdjacentMessage(
			next,
			DuplicateLookupDirection::Next)) {
		result.push_back(next);
	}
	return result;
}

int countDuplicateGroupSize(const not_null<HistoryItem*> item) {
	return static_cast<int>(getDuplicateGroup(item).size());
}

void handleDuplicateItemRemoved(not_null<const HistoryItem*> item) {
	const auto itemId = item->fullId();
	notifiedDuplicates.remove(itemId);
	if (!DuplicateCollapsingEnabled(item)) {
		return;
	}

	removingDuplicates.emplace(itemId);
	const auto previous = LookupAdjacentMessage(
		item,
		DuplicateLookupDirection::Previous);
	const auto next = LookupAdjacentMessage(
		item,
		DuplicateLookupDirection::Next);
	auto head = static_cast<const HistoryItem*>(nullptr);
	if (previous && SameDuplicateContent(previous, item)) {
		head = getDuplicateHead(previous);
		if (!head) {
			head = previous;
		}
	} else if (next && SameDuplicateContent(next, item)) {
		head = next;
	}

	const auto owner = &item->history()->owner();
	const auto headId = head ? head->fullId() : FullMsgId();
	crl::on_main([=] {
		removingDuplicates.remove(itemId);
		if (const auto current = owner->message(headId)) {
			owner->requestItemViewRefresh(current);
		}
	});
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

	if (isDuplicateMessage(item)) {
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
