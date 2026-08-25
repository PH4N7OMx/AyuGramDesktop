# Design Document: Telegram Desktop v7.1.2 Upgrade

## 1. Overview
This specification details the upgrade of AyuGram Desktop from upstream Telegram Desktop `v7.1.1` to `v7.1.2`.

## 2. Goals
- Merge upstream `tdesktop` release tag `v7.1.2` into the `dev` branch of `AyuGramDesktop-dev`.
- Synchronize submodules with upstream (`lib_base`, `lib_lottie`, `lib_spellcheck`, `lib_webview`, `cmake`).
- Merge upstream `desktop-app/lib_ui` commit `df4a4fb7a4645dbbe17e90ec4ca502723c006fb2` into the `master-ui` branch of `PH4N7OMx/lib_ui` and update the submodule pointer.
- Resolve all merge conflicts cleanly while preserving AyuGram features (file reordering, send as sticker, Ghost Mode, custom settings, branding, release workflows).
- Update application version metadata to `7.1.2` (version code `7001002`).

## 3. Detailed Component Plan

### 3.1 Submodule Synchronization (`lib_ui`)
- Upstream `v7.1.2` updates `Telegram/lib_ui` from commit `e3163c7537` to `df4a4fb7a4645dbbe17e90ec4ca502723c006fb2`.
- In `lib_ui` repository (`c:\Users\aver\Desktop\ayu\lib_ui` and submodule `Telegram/lib_ui`), merge upstream commit `df4a4fb7a4645dbbe17e90ec4ca502723c006fb2` into `master-ui`.
- AyuGram additions in `lib_ui` (such as `streamer_mode` hooks) merge cleanly without conflicts.
- Update the submodule reference in `AyuGramDesktop-dev`.

### 3.2 Upstream Merge & Conflict Handling
- Execute `git merge v7.1.2` in `AyuGramDesktop-dev` on branch `dev`.
- **Workflows (`.github/workflows/`)**:
  - Remove incoming `.github/workflows/linux.yml`, `mac.yml`, `mac_packaged.yml`, `snap.yml`, `win.yml` (since AyuGram uses `release.yml` for unified multi-platform builds).
- **Send Files Box (`Telegram/SourceFiles/boxes/send_files_box.cpp`)**:
  - Integrate upstream's `SendMenu::FillSendMenu` return check (`result != SendMenu::FillMenuResult::Prepared`) with AyuGram's "Send As Sticker" single image menu action and drag-and-drop file reordering.
  - Call `_menu->popupPrepared()` once actions are registered.
- **Version & Branding Metadata**:
  - `Telegram/SourceFiles/core/version.h`: set `AppVersion = 7001002`, `AppVersionStr = "7.1.2"`. Retain `AppName = "AyuGram Desktop"`, `AppFile = "AyuGram"`, etc.
  - `Telegram/Resources/winrc/Telegram.rc`: set `FileVersion` and `ProductVersion` to `"7.1.2.0"`. Retain `CompanyName = "Radolyn Labs"`, `ProductName = "AyuGram Desktop"`.
  - `Telegram/Resources/winrc/Updater.rc`: set `FileVersion` and `ProductVersion` to `"7.1.2.0"`. Retain `CompanyName = "Radolyn Labs"`, `FileDescription = "AyuGram Desktop Updater"`.
  - `Telegram/build/version`: update `AppVersion` to `7001002`, `AppVersionStr` and `AppVersionOriginal` to `7.1.2`.

## 4. Verification Plan
- Verify git merge status: working tree clean, merge commit created on branch `dev`.
- Verify all submodule pointers match expected commits (`lib_ui` contains merged upstream changes, other submodules match upstream `v7.1.2`).
- Inspect diffs of conflicted files to ensure no AyuGram features were omitted or corrupted.
