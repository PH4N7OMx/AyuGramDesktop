# Telegram Desktop v7.1.2 Upgrade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade AyuGram Desktop to upstream Telegram Desktop `v7.1.2` (version code `7001002`), preserving all AyuGram features, submodules, and custom workflows.

**Architecture:** Merge upstream tag `v7.1.2` into `dev` branch, update `lib_ui` fork with upstream changes, resolve all merge conflicts across source files, resources, workflows, and version headers.

**Tech Stack:** C++20, Qt6 / Qt5, CMake, Git, GitHub Actions.

## Global Constraints
- Do not lose or regress any AyuGram features (Ghost Mode, TTL photo anti-burn/saving, streamer mode, send as sticker, drag-drop file reorder, custom settings).
- Maintain submodules properly configured (`lib_ui` pointing to merged `master-ui`, standard submodules aligned with upstream `v7.1.2`).
- Target version code `7001002`, version string `7.1.2`.

---

### Task 1: Submodule `lib_ui` Upstream Synchronization

**Files:**
- Modify: `c:/Users/aver/Desktop/ayu/lib_ui`
- Modify: `Telegram/lib_ui`

**Interfaces:**
- Consumes: upstream commit `df4a4fb7a4645dbbe17e90ec4ca502723c006fb2` from `https://github.com/desktop-app/lib_ui.git`
- Produces: merged commit on branch `master-ui` in `lib_ui` repository containing both upstream v7.1.2 fixes and AyuGram hooks.

- [ ] **Step 1: Fetch upstream in `lib_ui`**
```bash
git -C c:/Users/aver/Desktop/ayu/lib_ui fetch https://github.com/desktop-app/lib_ui.git master
```

- [ ] **Step 2: Merge upstream commit into `master-ui` in `lib_ui`**
```bash
git -C c:/Users/aver/Desktop/ayu/lib_ui checkout master-ui
git -C c:/Users/aver/Desktop/ayu/lib_ui merge df4a4fb7a4645dbbe17e90ec4ca502723c006fb2 -m "Merge upstream changes (v7.1.2) into master-ui"
```

- [ ] **Step 3: Update `Telegram/lib_ui` submodule in `AyuGramDesktop-dev` to the merged commit**
```bash
git -C c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev/Telegram/lib_ui fetch c:/Users/aver/Desktop/ayu/lib_ui master-ui
git -C c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev/Telegram/lib_ui checkout FETCH_HEAD
```

---

### Task 2: Upstream Merge of `v7.1.2` into `dev` & Conflict Resolution

**Files:**
- Modify: `Telegram/SourceFiles/boxes/send_files_box.cpp`
- Modify: `Telegram/SourceFiles/core/version.h`
- Modify: `Telegram/Resources/winrc/Telegram.rc`
- Modify: `Telegram/Resources/winrc/Updater.rc`
- Modify: `Telegram/build/version`
- Delete: `.github/workflows/linux.yml`, `.github/workflows/mac.yml`, `.github/workflows/mac_packaged.yml`, `.github/workflows/snap.yml`, `.github/workflows/win.yml`
- Modify: `Telegram/lib_ui`

**Interfaces:**
- Consumes: git tag `v7.1.2` from `tdesktop` remote.
- Produces: clean merge commit `Merge tag 'v7.1.2' into dev` in `AyuGramDesktop-dev`.

- [ ] **Step 1: Start merge of tag `v7.1.2`**
```bash
git -C c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev merge --no-commit --no-ff v7.1.2
```

- [ ] **Step 2: Resolve `.github/workflows` conflicts by deleting upstream files**
```bash
git -C c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev rm .github/workflows/linux.yml .github/workflows/mac.yml .github/workflows/mac_packaged.yml .github/workflows/snap.yml .github/workflows/win.yml
```

- [ ] **Step 3: Resolve conflict in `Telegram/SourceFiles/boxes/send_files_box.cpp`**
Integrate `SendMenu::FillSendMenu` return check with AyuGram "Send As Sticker" action and drag-and-drop reordering:
```cpp
		const auto result = SendMenu::FillSendMenu(
			_menu.get(),
			_show,
			_sendMenuDetails(),
			_sendMenuCallback,
			&_st.tabbed.icons,
			position);
		if (result != SendMenu::FillMenuResult::Prepared) {
			_menu = nullptr;
			return true;
		}

		using ImageInfo = Ui::PreparedFileInformation::Image;
		if (_list.files.size() == 1 && std::get_if<ImageInfo>(&_list.files[0].information->media)) {
			_menu->addAction(
				tr::ayu_SendAsSticker(tr::now),
				[=]() mutable
				{
					const auto file = std::move(_list.files[0]);
					_list.files.clear();

					const auto sourceImage = std::get_if<ImageInfo>(&file.information->media);

					QByteArray targetArray;
					QBuffer buffer(&targetArray);
					buffer.open(QIODevice::WriteOnly);
					sourceImage->data.save(&buffer, "WEBP");

					QImage targetImage;
					targetImage.loadFromData(targetArray, "WEBP");

					addFiles(Storage::PrepareMediaFromImage(std::move(targetImage),
															std::move(targetArray),
															st::sendMediaPreviewSize));
					_list.overrideSendImagesAsPhotos = false;
					initSendWay();

					send({}, false);
				},
				&st::menuIconStickers);
		}
		_menu->popupPrepared();
		return true;
```

- [ ] **Step 4: Update `version.h` to version 7.1.2 (7001002)**
In `Telegram/SourceFiles/core/version.h`:
```cpp
constexpr auto AppVersion = 7001002;
constexpr auto AppVersionStr = "7.1.2";
```

- [ ] **Step 5: Update `Telegram.rc` and `Updater.rc` to 7.1.2.0**
In `Telegram/Resources/winrc/Telegram.rc` and `Telegram/Resources/winrc/Updater.rc`:
Ensure `FileVersion` and `ProductVersion` are `"7.1.2.0"` while keeping `"Radolyn Labs"` and `"AyuGram Desktop"`.

- [ ] **Step 6: Update `Telegram/build/version`**
Ensure:
```
AppVersion         7001002
AppVersionStrMajor 7.1
AppVersionStrSmall 7.1.2
AppVersionStr      7.1.2
BetaChannel        0
AlphaVersion       0
AppVersionOriginal 7.1.2
```

- [ ] **Step 7: Stage and record `Telegram/lib_ui` submodule**
```bash
git -C c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev add Telegram/lib_ui
```

- [ ] **Step 8: Stage all resolved files and complete merge commit**
```bash
git -C c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev add -A
git -C c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev commit -m "Merge tag 'v7.1.2' into dev"
```

---

### Task 3: Verification and Submodule Consistency

**Files:**
- Verify: all files in repository

**Interfaces:**
- Consumes: merged `dev` branch.
- Produces: completely validated repository ready for immediate build.

- [ ] **Step 1: Run `git submodule update --init --recursive`**
```bash
git -C c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev submodule update --init --recursive
```

- [ ] **Step 2: Verify `git status` and `git submodule status`**
Check that working tree is clean and submodules match expected commits.

- [ ] **Step 3: Verify diff against `v7.1.2` for integrity of AyuGram patches**
Verify that all AyuGram features (Ghost Mode, TTL anti-burn, settings, streamer mode, gift catalog, sticker sender, window settings) are intact.
