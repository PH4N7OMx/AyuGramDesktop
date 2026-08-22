# Telegram Desktop v7.1.1 Upgrade & TTL Photo Preservation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade AyuGram Desktop to upstream Telegram Desktop `v7.1.1` and ensure self-destructing / view-once (TTL) photos and media are preserved, viewable repeatedly, and downloadable in Spy/Ghost Mode.

**Architecture:** Merge upstream tag `v7.1.1` into the `dev` branch, resolve merge conflicts with AyuGram features, integrate Ghost Mode read blocking for new TTL media view handlers, prevent TTL media purge in `clearMediaAsExpired()` and `checkSingleViewMediaBurn()` when messages are savable, and remove saving / screenshot restrictions for disappearing media.

**Tech Stack:** C++20, Qt 6 / Qt 5, CMake, Ninja, MSVC / GCC, SQLite.

## Global Constraints
- Upstream target version: `v7.1.1` (from remote `tdesktop`).
- Preserve all existing comments and docstrings unless directly related to modified logic.
- Ensure backwards compatibility with existing AyuGram SQLite database message schema.
- Follow AyuGram coding style and upstream tdesktop conventions.

---

### Task 1: Merge upstream tag `v7.1.1` into `dev` branch

**Files:**
- Modify: git repository state in `AyuGramDesktop-dev`

**Interfaces:**
- Consumes: git remote `tdesktop`, tag `v7.1.1`
- Produces: merged branch with conflicts marked or clean merge commit

- [ ] **Step 1: Fetch and initiate merge of tag `v7.1.1`**
Run: `git merge v7.1.1 --no-commit --no-ff`
- [ ] **Step 2: Inspect conflict list and changed files**
Run: `git status` to identify any conflicting files.
- [ ] **Step 3: Update submodules if necessary**
Run: `git submodule update --init --recursive`

---

### Task 2: Resolve Merge Conflicts and Align AyuGram Hooks

**Files:**
- Modify: Conflicted source files in `Telegram/SourceFiles/` (e.g. `history/history_item.cpp`, `history/history_item_helpers.cpp`, `apiwrap.cpp`, `media/view/media_view_overlay_widget.cpp`, `history/view/history_view_context_menu.cpp`, etc.)

**Interfaces:**
- Consumes: AyuGram hooks and upstream v7.1.1 code
- Produces: Conflict-free working tree

- [ ] **Step 1: Resolve conflicts in core history and media files**
Resolve conflict markers preserving AyuGram custom hooks (message saving, ghost mode, custom filters, rich messages).
- [ ] **Step 2: Resolve conflicts in UI and window management**
Resolve conflict markers in context menus, chat sections, and window session controllers.
- [ ] **Step 3: Complete merge commit**
Commit the merge of `v7.1.1`.

---

### Task 3: Ghost Mode: Block Read Receipts on TTL Media Viewing

**Files:**
- Modify: `Telegram/SourceFiles/media/view/media_view_overlay_widget.cpp`
- Modify: `Telegram/SourceFiles/apiwrap.cpp`

**Interfaces:**
- Consumes: `AyuSettings::ghost(session).sendReadMessages()`
- Produces: Non-marking timed media read handler in Ghost Mode

- [ ] **Step 1: Update `OverlayWidget::markTimedMediaRead()`**
Guard the read request so it is skipped when `!AyuSettings::ghost(&item->history()->session()).sendReadMessages()`.
- [ ] **Step 2: Update `ApiWrap::markContentsRead()`**
Ensure timed media (`isTtlCoveredMedia()` / `unsupportedTTL()`) is not marked read on the server when Ghost Mode is active.
- [ ] **Step 3: Commit Ghost Mode read blocking**
Commit changes with message `feat: block server read receipts for TTL media in Ghost Mode`.

---

### Task 4: Anti-Destruction: Prevent Purge and Expiration of Savable TTL Media

**Files:**
- Modify: `Telegram/SourceFiles/history/history_item.cpp`
- Modify: `Telegram/SourceFiles/media/view/media_view_overlay_widget.cpp`
- Modify: `Telegram/SourceFiles/ayu/utils/telegram_helpers.cpp`

**Interfaces:**
- Consumes: `isMessageSavable(item)`
- Produces: Protected media retention when message saving is active

- [ ] **Step 1: Update `HistoryItem::clearMediaAsExpired()`**
If `isMessageSavable(this)` is true, prevent unarming/purging the photo or video object, keep cache and bytes intact, and do not replace the item with `lng_ttl_photo_expired`.
- [ ] **Step 2: Update `OverlayWidget::checkSingleViewMediaBurn()`**
Check `isMessageSavable(item)`. If savable, bypass calling `item->clearMediaAsExpired()`.
- [ ] **Step 3: Update `HistoryItem::getSelfDestructIn()`**
If `isMessageSavable(this)` is true, do not overwrite the item with expired service text.
- [ ] **Step 4: Commit anti-destruction logic**
Commit changes with message `feat: preserve TTL photos and videos when message saving is active`.

---

### Task 5: Enable Saving, Context Menus, and Disable Screenshot Blackouts for TTL Media

**Files:**
- Modify: `Telegram/SourceFiles/media/view/media_view_overlay_widget.cpp`
- Modify: `Telegram/SourceFiles/history/history_item.cpp`
- Modify: `Telegram/SourceFiles/history/view/history_view_context_menu.cpp`

**Interfaces:**
- Consumes: `OverlayWidget::computeSaveButtonVisible()`, `forbidsSaving()`, `contentNeedsScreenshotProtection()`
- Produces: Unrestricted save / copy / screenshot access for TTL media

- [ ] **Step 1: Update `OverlayWidget::computeSaveButtonVisible()`**
Ensure the quick save button is visible and active for TTL photos and videos.
- [ ] **Step 2: Update `OverlayWidget::contentNeedsScreenshotProtection()`**
Ensure screenshot protection does not blackout the media viewer when viewing TTL media in AyuGram.
- [ ] **Step 3: Update `HistoryItem::forbidsSaving()` & context menus**
Ensure `forbidsSaving()` returns false for TTL media when message saving is enabled, allowing "Save Image As...", "Copy Image", etc.
- [ ] **Step 4: Commit save and screenshot accessibility**
Commit changes with message `feat: enable saving and disable screenshot protection for TTL media`.

---

### Task 6: Comprehensive Verification

**Files:**
- Verify: Full codebase in `AyuGramDesktop-dev`

**Interfaces:**
- Consumes: All updated files
- Produces: Verified build and functional compliance

- [ ] **Step 1: Check git diff and log**
Verify all changes match the design document.
- [ ] **Step 2: Verify compilation and CMake configuration integrity**
Validate syntax and includes across all modified files.
- [ ] **Step 3: Final commit and status check**
Ensure working directory is clean and all tasks are completed.
