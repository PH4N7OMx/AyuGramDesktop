# Telegram Desktop v7.2.9 Upgrade, Deleted Gifts Removal & macOS Style Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade AyuGram Desktop to tdesktop v7.2.9, completely remove the patched deleted gifts feature, and implement a full Telegram macOS (TelegramSwift) UI style toggleable in Appearance settings.

**Architecture:** 
1. Core upgrade via git merge of tag `v7.2.9`.
2. Removal of `Ayu::DeletedGifts` module from CMake and gift common logic.
3. Feature flag `interfaceStyle` in `AyuSettings` with settings UI and restart prompt.
4. Adaptive UI rendering hooks in `Dialogs::Ui::PaintRow`, bubble radii, and compose controls.

**Tech Stack:** C++20, Qt 5/6, CMake, AyuGram / tdesktop architecture.

## Global Constraints
- Preserve all AyuGram customizations, CI build files, and custom patches.
- Maintain documentation integrity and code comments.
- Do not introduce banned Qt helpers (like `qMin`, `qMax`, `qRound`, use `std::min`, `std::max`, `base::SafeRound`).

---

### Task 1: Upgrade to Telegram Desktop v7.2.9

**Files:**
- Merge: Upstream tag `v7.2.9`
- Modify:
  - `Telegram/SourceFiles/core/version.h`
  - `Telegram/Resources/winrc/Telegram.rc`
  - `Telegram/Resources/winrc/Updater.rc`
  - `Telegram/build/version`

- [ ] **Step 1: Perform git merge of tag v7.2.9**
Merge tag `v7.2.9` into `dev` branch.
- [ ] **Step 2: Resolve any conflicts and preserve AyuGram branding / CI**
Ensure `.github/workflows/release.yml` and build configurations are intact.
- [ ] **Step 3: Update version metadata to 7.2.9**
Update `version.h`, `Telegram.rc`, `Updater.rc`, `build/version`.
- [ ] **Step 4: Commit merge**
Commit merge of v7.2.9.

---

### Task 2: Remove Deleted Gifts Feature

**Files:**
- Delete:
  - `Telegram/SourceFiles/ayu/features/deleted_gifts/deleted_gifts.h`
  - `Telegram/SourceFiles/ayu/features/deleted_gifts/deleted_gifts.cpp`
- Modify:
  - `Telegram/CMakeLists.txt`
  - `Telegram/SourceFiles/info/peer_gifts/info_peer_gifts_common.cpp`

- [ ] **Step 1: Remove files and CMake entries**
Delete `deleted_gifts.h` and `deleted_gifts.cpp`. Remove from `Telegram/CMakeLists.txt`.
- [ ] **Step 2: Revert info_peer_gifts_common.cpp**
Remove `DeletedGifts::Manager::injectGifts` and sticker pack update subscriptions.
- [ ] **Step 3: Commit deleted gifts removal**
Commit: `git commit -m "feat(gifts): remove deleted gifts injection"`

---

### Task 3: AyuSettings: Interface Style Setting & Persistence

**Files:**
- Modify:
  - `Telegram/SourceFiles/ayu/ayu_settings.h`
  - `Telegram/SourceFiles/ayu/ayu_settings.cpp`
  - `Telegram/SourceFiles/ayu/ui/settings/settings_appearance.cpp`

- [ ] **Step 1: Add InterfaceStyle enum and members in ayu_settings.h/cpp**
Define `enum class InterfaceStyle { Default = 0, TelegramSwift = 1 };`, getter, setter, `isTelegramSwiftStyle()`, and serialization in JSON.
- [ ] **Step 2: Add setting selector in settings_appearance.cpp**
Add UI selector under Appearance with `ShowRestartPrompt(controller)`.
- [ ] **Step 3: Commit settings addition**
Commit: `git commit -m "feat(settings): add interfaceStyle setting with macOS option"`

---

### Task 4: Telegram macOS Style: Chats List & Sidebar Layout

**Files:**
- Modify:
  - `Telegram/SourceFiles/dialogs/ui/dialogs_layout.cpp`

- [ ] **Step 1: Implement floating rounded row selection**
When `isTelegramSwiftStyle()` is active, draw row selection as inset rounded pill (margin 8px, 2px, radius 10px).
- [ ] **Step 2: Implement hairline separators**
Draw 1px separator indented past avatar.
- [ ] **Step 3: Commit dialogs layout styling**
Commit: `git commit -m "feat(ui): implement TelegramSwift floating dialogs rows and separators"`

---

### Task 5: Telegram macOS Style: Bubbles & Compose Area Styling

**Files:**
- Modify:
  - `Telegram/SourceFiles/ui/chat/chat_style_radius.cpp`
  - `Telegram/SourceFiles/history/view/controls/history_view_compose_controls.cpp`

- [ ] **Step 1: Enforce TelegramSwift bubble radius**
In `chat_style_radius.cpp`, default to 16px radius when in macOS style.
- [ ] **Step 2: Style compose input area into capsule**
In `history_view_compose_controls.cpp`, render capsule pill background around the compose text field.
- [ ] **Step 3: Commit bubble and compose area updates**
Commit: `git commit -m "feat(ui): implement TelegramSwift bubble and compose bar styling"`

---

### Task 6: Final Verification

- [ ] **Step 1: Verify git status and commits**
Check `git log -n 5` and working tree cleanliness.
- [ ] **Step 2: Create Walkthrough**
Create `walkthrough.md` documenting all changes.
