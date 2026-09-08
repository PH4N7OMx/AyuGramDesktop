# First-Run Channel Promo Popup and Full Localization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show a one-time promotional popup prompting users to subscribe to the official Telegram channel (`https://t.me/ayufork`) after logging in, and provide full multi-language translations across all 24 languages in `Languages` (for both channel promo and blocked user avatars).

**Architecture:** A persistent boolean flag `channelPromoShown` is added to `AyuSettings` (saved in `settings.json`). Inside `Window::SessionController::SessionController` for the primary window (`_isPrimary`), when an authorized session starts, `checkChannelPromo()` checks the flag. If false, it sets the flag to true and saves immediately, then presents a `Ui::GenericBox` modal with "Subscribe" (`UrlClickHandler::Open`) and "Later" buttons. All 24 language files in `Languages/values/langs/*/Shared.json` and `Telegram/Resources/langs/lang.strings` are updated with the required localization keys.

**Tech Stack:** C++20, Qt 6 / Telegram Desktop Core UI (`Ui::GenericBox`, `UrlClickHandler`), nlohmann::json, Python 3 for JSON verification.

## Global Constraints

- Target platform: Windows
- Do not build Release or start heavy compiler builds (per `AGENTS.md`).
- Respect existing JSON formatting in `Languages/values/langs/*/Shared.json` (2 spaces indentation, valid JSON).
- Use tab indentation for C++ source files (`ayu_settings.h`, `ayu_settings.cpp`, `window_session_controller.h`, `window_session_controller.cpp`).
- Keep git commits clean and atomic per task.

---

### Task 1: Add localization keys across all 24 languages in `Languages` repository

**Files:**
- Modify: `c:/Users/aver/Desktop/ayu/Languages/values/langs/*/Shared.json` (24 files: `ar`, `be`, `de`, `el`, `en`, `eo`, `es`, `fa`, `fi`, `fr`, `he`, `it`, `lv`, `os`, `pl`, `pt`, `ro`, `ru`, `tr`, `uk`, `vi`, `zh`, `zh-hans`, `zh-hant`)

**Interfaces:**
- Produces: JSON keys:
  - `ChannelPromoTitle`
  - `ChannelPromoText`
  - `ChannelPromoSubscribe`
  - `ChannelPromoLater`
  - `LoadBlockedAvatars` (for the remaining 22 languages)
  - `LoadBlockedAvatarsDescription` (for the remaining 22 languages)

- [ ] **Step 1: Write a Python update script to inject translations into all 24 Shared.json files**
Run a python script in `c:/Users/aver/Desktop/ayu/Languages` that reads each `Shared.json`, adds the missing keys with accurate translations, and formats with 2 spaces indentation.

- [ ] **Step 2: Validate JSON syntax across all 24 files**
Run Python validation: ensure each file parses without `json.decoder.JSONDecodeError` and contains all required keys.

- [ ] **Step 3: Commit changes in `Languages` repository**
```bash
git add values/langs/
git commit -m "i18n: add channel promo and blocked avatars translations to all languages"
```

---

### Task 2: Add localization keys to `Telegram/Resources/langs/lang.strings`

**Files:**
- Modify: `c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev/Telegram/Resources/langs/lang.strings`

**Interfaces:**
- Consumes: Language keys from Task 1.
- Produces: Strings accessible in C++ via `tr::ayu_ChannelPromoTitle()`, `tr::ayu_ChannelPromoText()`, `tr::ayu_ChannelPromoSubscribe()`, `tr::ayu_ChannelPromoLater()`.

- [ ] **Step 1: Add string definitions to `lang.strings`**
Add under the AyuGram section:
```text
"ayu_ChannelPromoTitle" = "AyuGram Fork";
"ayu_ChannelPromoText" = "Subscribe to our official Telegram channel for updates and announcements!";
"ayu_ChannelPromoSubscribe" = "Subscribe";
"ayu_ChannelPromoLater" = "Later";
```

- [ ] **Step 2: Verify `lang.strings` syntax**
Check that quotes and semicolons match the existing `lang.strings` syntax.

- [ ] **Step 3: Commit changes in `AyuGramDesktop-dev` repository**
```bash
git add Telegram/Resources/langs/lang.strings
git commit -m "i18n: add channel promo strings to lang.strings"
```

---

### Task 3: Add `channelPromoShown` flag to `AyuSettings`

**Files:**
- Modify: `c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev/Telegram/SourceFiles/ayu/ayu_settings.h`
- Modify: `c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev/Telegram/SourceFiles/ayu/ayu_settings.cpp`

**Interfaces:**
- Produces:
  - `[[nodiscard]] bool channelPromoShown() const`
  - `void setChannelPromoShown(bool val)`
  - `[[nodiscard]] rpl::producer<bool> channelPromoShownValue() const`
  - `[[nodiscard]] rpl::producer<bool> channelPromoShownChanges() const`
  - JSON property: `"channelPromoShown"` in `to_json` and `from_json`

- [ ] **Step 1: Declare getters, setter, producer, and variable in `ayu_settings.h`**
In `ayu_settings.h`:
```cpp
[[nodiscard]] bool channelPromoShown() const { return _channelPromoShown.current(); }
void setChannelPromoShown(bool val);
[[nodiscard]] rpl::producer<bool> channelPromoShownValue() const { return _channelPromoShown.value(); }
[[nodiscard]] rpl::producer<bool> channelPromoShownChanges() const { return _channelPromoShown.changes(); }
```
And private member:
```cpp
rpl::variable<bool> _channelPromoShown = false;
```

- [ ] **Step 2: Implement setter and JSON serialization in `ayu_settings.cpp`**
In `ayu_settings.cpp`:
```cpp
void AyuSettings::setChannelPromoShown(bool val) {
	if (_channelPromoShown.current() == val) return;
	_channelPromoShown = val;
	save();
}
```
In `to_json`:
```cpp
{"channelPromoShown", s._channelPromoShown.current()},
```
In `from_json`:
```cpp
s._channelPromoShown = j.value("channelPromoShown", defaults._channelPromoShown.current());
```

- [ ] **Step 3: Verify formatting and syntax**
Inspect `git diff` to ensure tab indentation and clean formatting.

- [ ] **Step 4: Commit changes**
```bash
git add Telegram/SourceFiles/ayu/ayu_settings.h Telegram/SourceFiles/ayu/ayu_settings.cpp
git commit -m "feat(ayu): add channelPromoShown setting"
```

---

### Task 4: Hook first-run channel promo dialog into `SessionController`

**Files:**
- Modify: `c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev/Telegram/SourceFiles/window/window_session_controller.h`
- Modify: `c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev/Telegram/SourceFiles/window/window_session_controller.cpp`

**Interfaces:**
- Consumes:
  - `AyuSettings::getInstance().channelPromoShown()`
  - `AyuSettings::getInstance().setChannelPromoShown(true)`
  - `tr::ayu_ChannelPromoTitle()`, `tr::ayu_ChannelPromoText()`, `tr::ayu_ChannelPromoSubscribe()`, `tr::ayu_ChannelPromoLater()`
  - `UrlClickHandler::Open(u"https://t.me/ayufork"_q)`
  - `Ui::GenericBox`

- [ ] **Step 1: Add private method declaration to `window_session_controller.h`**
In `SessionController` private section:
```cpp
void checkChannelPromo();
```

- [ ] **Step 2: Implement `checkChannelPromo()` in `window_session_controller.cpp`**
```cpp
void SessionController::checkChannelPromo() {
	if (AyuSettings::getInstance().channelPromoShown()) {
		return;
	}
	AyuSettings::getInstance().setChannelPromoShown(true);

	const auto weak = base::make_weak(this);
	_window->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::ayu_ChannelPromoTitle());
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::ayu_ChannelPromoText(),
			st::boxLabel));
		box->addButton(tr::ayu_ChannelPromoSubscribe(), [=] {
			UrlClickHandler::Open(u"https://t.me/ayufork"_q);
			box->closeBox();
		});
		box->addButton(tr::ayu_ChannelPromoLater(), [=] {
			box->closeBox();
		});
		box->setCloseByOutsideClick(true);
	}));
}
```

- [ ] **Step 3: Call `checkChannelPromo()` in `SessionController::SessionController`**
In `SessionController::SessionController`:
Inside `if (_isPrimary)` (around lines 1605-1610 or 1753):
```cpp
if (_isPrimary) {
	crl::on_main(base::make_weak(this), [=] {
		checkChannelPromo();
	});
}
```

- [ ] **Step 4: Verify necessary includes are present**
Ensure `#include "ayu/ayu_settings.h"`, `#include "ui/layers/generic_box.h"`, `#include "ui/widgets/labels.h"`, and `#include "ui/basic_click_handlers.h"` are included or available.

- [ ] **Step 5: Commit changes**
```bash
git add Telegram/SourceFiles/window/window_session_controller.h Telegram/SourceFiles/window/window_session_controller.cpp
git commit -m "feat(ayu): show channel promo dialog on first authorized run"
```

---

### Task 5: Verification, whole-branch review and documentation

**Files:**
- Test scripts / verification checks
- Update `walkthrough.md`

- [ ] **Step 1: Run comprehensive verification script**
Verify JSON files in `Languages/values/langs/`, verify `lang.strings`, verify git diffs.

- [ ] **Step 2: Perform code review**
Check edge cases: multi-account login, crash resilience, outside-click behavior, URL handling.

- [ ] **Step 3: Update documentation and walkthrough**
Update `walkthrough.md` with complete summary of changes.
