# Design Document: Telegram Desktop v7.1.1 Upgrade & TTL Photo Preservation in Ghost Mode

## 1. Overview
This specification details the upgrade of AyuGram Desktop from upstream Telegram Desktop `v7.0.9` to `v7.1.1`, along with modifications to preserve, view repeatedly, and download self-destructing / view-once (TTL) photos and media in Spy/Ghost Mode and when message saving is enabled.

## 2. Goals
- Upgrade `AyuGramDesktop-dev` to Telegram Desktop release tag `v7.1.1` and resolve all merge conflicts with AyuGram features.
- In Spy/Ghost Mode (`ghost.sendReadMessages() == false`), ensure viewing self-destructing / view-once photos and videos does not send `messages.readMessageContents` to Telegram servers.
- Prevent local deletion / expiration of TTL photos and media (`clearMediaAsExpired`, `checkSingleViewMediaBurn`) when message saving is enabled or under Ghost Mode.
- Allow repeated viewing and unrestricted saving / copying / downloading of TTL photos and media via context menus and the Media Viewer toolbar.
- Bypass screenshot protection for disappearing media in Media Viewer so that screen captures are not blacked out.

## 3. Architectural Design & Component Modifications

### 3.1 Upstream Merge (v7.0.9 -> v7.1.1)
- Merge git tag `v7.1.1` from remote `tdesktop` into the `dev` branch.
- Re-apply AyuGram integrations in conflicted files (such as `history_item.cpp`, `history_view_context_menu.cpp`, `media_view_overlay_widget.cpp`, `apiwrap.cpp`, `window_session_controller.cpp`, etc.).
- Verify submodule alignment (`lib_ui`, `lib_base`, `MicroTeX`, `rlottie`, `cmark-gfm`, etc.).

### 3.2 Ghost Mode: Prevent Read Receipts for TTL Media
- In `Media::View::OverlayWidget::markTimedMediaRead()` and `ApiWrap::markContentsRead()`:
  - Guard `MTPmessages_ReadMessageContents` calls behind `ghost.sendReadMessages()`.
  - When Ghost Mode is active, do not mark TTL contents read on the server.

### 3.3 Media Preservation & Anti-Burn
- In `HistoryItem::clearMediaAsExpired()`:
  - If `isMessageSavable(this)` is true (i.e. `saveDeletedMessages()` is active) or Ghost Mode is active:
    - Do not unarm/purge the photo object or clear `activeMediaView` bytes.
    - Do not remove image thumbnails or local cache keys.
    - Do not overwrite the message text with `lng_ttl_photo_expired` service text.
- In `Media::View::OverlayWidget::checkSingleViewMediaBurn()`:
  - Check `isMessageSavable(item)`. If savable, bypass calling `item->clearMediaAsExpired()`.

### 3.4 Unrestricted Saving & Context Menus
- In `HistoryItem::forbidsSaving()` / `CopyMediaRestrictionTypeFor`:
  - When message saving is enabled or for TTL media, do not forbid saving.
- In `Media::View::OverlayWidget::computeSaveButtonVisible()`:
  - Return `true` for loaded TTL photos and videos when saving is allowed.
- In `HistoryView::AddPhotoActions` and context menu builders:
  - Ensure "Save Image", "Copy Image", and "Save Video" menu actions remain available on TTL items.

### 3.5 Screenshot Protection
- In `Media::View::OverlayWidget::contentNeedsScreenshotProtection()`:
  - Return `false` when anti-screenshot protection / saving is allowed, preventing OS window blackout.

## 4. Verification Plan
- Git merge and conflict resolution verification.
- Compilation and build check.
- Functional verification:
  - Receipt of self-destructing / view-once photos.
  - Opening photo in viewer without server read receipt when in Ghost Mode.
  - Closing viewer and confirming photo remains intact and viewable again.
  - Saving photo to disk and copying image to clipboard.
