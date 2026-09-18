# Design Document: Telegram Desktop v7.2.9 Upgrade, Deleted Gifts Removal & Telegram macOS (TelegramSwift) Style

## 1. Overview
This specification details three cohesive goals for AyuGram Desktop:
1. Upgrade the underlying core to upstream Telegram Desktop `v7.2.9` (merging official release tag `v7.2.9`).
2. Remove the deleted gifts injection feature (added in commit `74c4e5401f`), as Telegram has server-side patched and disabled gifting deleted gifts.
3. Implement a complete 1:1 Telegram macOS (TelegramSwift) user interface style mode, toggleable in AyuGram Appearance Settings with an application restart prompt.

## 2. Goals & Non-Goals

### Goals
- **Upstream v7.2.9**: Merge `v7.2.9` from upstream `tdesktop` into branch `dev` while preserving all AyuGram patches, build fixes (MSVC, Xcode 16 extern C linkage, macOS brew pkgconfig), and submodules.
- **Deleted Gifts Removal**: Completely revert and remove `Telegram/SourceFiles/ayu/features/deleted_gifts/` and its hooks in `info_peer_gifts_common.cpp` and `Telegram/CMakeLists.txt`.
- **Telegram macOS (TelegramSwift) Style Mode**:
  - Add setting `interfaceStyle` (values: `Default`, `TelegramSwiftMacOS`) in `AyuSettings`, serialized to `ayu_settings.json`.
  - Add selector in **Settings -> AyuGram -> Appearance** with a restart prompt dialog (`ShowRestartPrompt`).
  - **Chats List / Sidebar**:
    - Draw active and hover dialog rows as floating rounded rectangles (pill item: 8px horizontal inset, 2px vertical inset, 10px corner radius) matching TelegramSwift.
    - Render hairline separators (0.5–1px) between chat rows indented past the avatar.
    - Style unread badges and search field with macOS rounded capsule geometry.
  - **Chat Canvas & Message Bubbles**:
    - Enforce TelegramSwift bubble corner radius (16–18px for outer corners, 6–8px for inner grouped corners) in `Ui::CachedCornerRadiusValue` / bubble rounding routines.
    - Match TelegramSwift tail curvature and compact bottom-right timestamp/checkmark metrics.
  - **Compose Input Bar**:
    - Render the text input area as a capsule (pill container with 18–20px rounding) with attachment and emoji buttons embedded, and circular action button on the right.
  - **Top Bar Header**:
    - Clean macOS header typography, centered or aligned peer info, and subtle bottom hairline divider.

### Non-Goals
- Altering the core Qt networking / MTProto engine.
- Breaking compatibility with custom user themes on standard desktop mode.

## 3. Detailed Component Plan

### 3.1 Upstream Merge (`v7.2.9`)
- Merge git tag `v7.2.9` into `dev`.
- Retain existing AyuGram CI workflows (`release.yml`) and build compatibility fixes.
- Version metadata updates:
  - `Telegram/SourceFiles/core/version.h`: `AppVersion = 7002009`, `AppVersionStr = "7.2.9"`.
  - `Telegram/Resources/winrc/Telegram.rc` and `Updater.rc`: `"7.2.9.0"`.
  - `Telegram/build/version`: `7002009`, `7.2.9`.

### 3.2 Deleted Gifts Removal
- Delete:
  - `Telegram/SourceFiles/ayu/features/deleted_gifts/deleted_gifts.h`
  - `Telegram/SourceFiles/ayu/features/deleted_gifts/deleted_gifts.cpp`
- Modify `Telegram/CMakeLists.txt`: remove `deleted_gifts.h` and `deleted_gifts.cpp`.
- Modify `Telegram/SourceFiles/info/peer_gifts/info_peer_gifts_common.cpp`:
  - Remove `#include "ayu/features/deleted_gifts/deleted_gifts.h"`.
  - Revert `api->requestStarGifts(...)` and remove `Ayu::DeletedGifts::Manager::instance().injectGifts(...)` and `sessionUpdated` subscription.

### 3.3 Telegram macOS (TelegramSwift) Style Mode

#### 3.3.1 Settings & State Management
- File: `Telegram/SourceFiles/ayu/ayu_settings.h` and `ayu_settings.cpp`:
  - Add enum `enum class InterfaceStyle { Default = 0, TelegramSwift = 1 };`.
  - Add field `rpl::variable<InterfaceStyle> _interfaceStyle = InterfaceStyle::Default;`.
  - Add getter `interfaceStyle()`, setter `setInterfaceStyle(InterfaceStyle style)`.
  - Add convenience helper `[[nodiscard]] bool isTelegramSwiftStyle() const;`.
  - Serialize/deserialize `interfaceStyle` in `load()` and `save()`.
- File: `Telegram/SourceFiles/ayu/ui/settings/settings_appearance.cpp`:
  - In `BuildAppearance(...)`, add a radio/dropdown selector:
    - *Стиль интерфейса (Interface Style)*:
      1. *По умолчанию (Telegram Desktop)*
      2. *macOS (Telegram Swift)*
  - On change, invoke `ShowRestartPrompt(controller)` prompting application restart.

#### 3.3.2 Sidebar & Dialogs List (`dialogs/ui/dialogs_layout.cpp`)
- When `AyuSettings::getInstance().isTelegramSwiftStyle()` is active:
  - In `PaintRow(...)`:
    - Draw window/sidebar background behind row.
    - If `context.active` or `context.selected`, draw a floating rounded rectangle:
      `const auto pillRect = geometry.marginsRemoved(QMargins(8, 2, 8, 2));`
      `p.drawRoundedRect(pillRect, 10, 10);` with `st::dialogsBgActive` or `st::dialogsBgOver`.
    - Hairline separator: draw a 1px divider at `geometry.bottom()`, starting from `st::dialogsPadding.x() + st::dialogsPhotoSize + 12px` to `geometry.width() - 12px`.
  - Search field & filter tabs: apply pill-shaped background geometry.

#### 3.3.3 Message Bubbles & Chat History
- Files: `Telegram/SourceFiles/ui/chat/chat_style_radius.cpp`, `Telegram/SourceFiles/ui/cached_round_corners.cpp`:
  - When `isTelegramSwiftStyle()` is active:
    - Large bubble radius defaults to 16px.
    - Small bubble radius defaults to 6px.
  - File: `Telegram/SourceFiles/history/view/history_view_message.cpp`:
    - Support smooth continuous squircle corner rendering and macOS-like tail integration.
    - Align bottom info metrics (timestamp + checkmark ticks) to TelegramSwift padding.

#### 3.3.4 Bottom Compose Input Bar
- File: `Telegram/SourceFiles/history/view/controls/history_view_compose_controls.cpp`:
  - When `isTelegramSwiftStyle()` is active:
    - Wrap the message input field in a rounded capsule container (background with 18px border radius, subtle border).
    - Position paperclip attachment button inside/adjacent to capsule.
    - Style Send/Mic action button as a circular button on the right.

## 4. Verification Plan
1. **Upstream Merge Verification**:
   - Run `git log -n 1` to verify merge of tag `v7.2.9`.
   - Verify `git status` is clean.
2. **Deleted Gifts Verification**:
   - Verify `deleted_gifts.cpp` and `.h` are deleted.
   - Verify `info_peer_gifts_common.cpp` compiles cleanly without gift injection references.
3. **Telegram macOS Style Verification**:
   - Switch style in Settings -> AyuGram -> Appearance.
   - Verify restart prompt triggers properly.
   - Verify dialogs list renders floating rounded selected rows and hairline separators.
   - Verify bubbles render with 16px rounded corners.
