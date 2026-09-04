# Liquid Glass UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement an iOS-style "Liquid Glass" translucent frosted blur effect in AyuGram Desktop with an optional 3-state setting (Disabled, ChatBars, Full).

**Architecture:** A dedicated `AyuLiquidGlass` helper renders multi-layered frosted glass with backdrop blur, theme-tinted translucency, and 1px specular highlight rims. The setting is stored in `AyuSettings` under `liquidGlassMode` and hooks into `TopBarWidget`, `ComposeControls`, `Dialogs::Widget`, and `MainMenu`.

**Tech Stack:** C++20, Qt 5/6, Desktop App Toolkit (`lib_ui`, `lib_base`).

## Global Constraints
- **NO COMMENTS in the code**: The user explicitly instructed not to leave comments in the code. Preserve existing comments, but do not write any new comments.
- Must support 3 states: `Disabled` (0), `ChatBars` (1), `Full` (2).
- Must seamlessly react to theme changes and setting changes without restarting the app.

---

### Task 1: Add `LiquidGlassMode` Setting Model and Serialization

**Files:**
- Modify: `Telegram/SourceFiles/ayu/ayu_settings.h:20-100, 480-520`
- Modify: `Telegram/SourceFiles/ayu/ayu_settings.cpp:1200-1400`

**Interfaces:**
- Produces:
  ```cpp
  enum class LiquidGlassMode {
      Disabled = 0,
      ChatBars = 1,
      Full = 2,
  };
  ```
  `AyuSettings::liquidGlassMode() const -> LiquidGlassMode`
  `AyuSettings::setLiquidGlassMode(LiquidGlassMode val) -> void`
  `_liquidGlassMode` reactive variable.

- [ ] **Step 1: Add `LiquidGlassMode` enum and declaration in `ayu_settings.h`**

Add `enum class LiquidGlassMode` with `NLOHMANN_JSON_SERIALIZE_ENUM`.
Add `_liquidGlassMode` field and getter/setter:
```cpp
enum class LiquidGlassMode {
	Disabled = 0,
	ChatBars = 1,
	Full = 2,
};

NLOHMANN_JSON_SERIALIZE_ENUM(LiquidGlassMode, {
	{LiquidGlassMode::Disabled, 0},
	{LiquidGlassMode::ChatBars, 1},
	{LiquidGlassMode::Full, 2},
})
```
And inside `class AyuSettings`:
```cpp
[[nodiscard]] LiquidGlassMode liquidGlassMode() const { return _liquidGlassMode.current(); }
[[nodiscard]] rpl::producer<LiquidGlassMode> liquidGlassModeValue() const { return _liquidGlassMode.value(); }
void setLiquidGlassMode(LiquidGlassMode val);
```
And private member:
```cpp
rpl::variable<LiquidGlassMode> _liquidGlassMode = LiquidGlassMode::Disabled;
```

- [ ] **Step 2: Implement setter and JSON serialization in `ayu_settings.cpp`**

In `ayu_settings.cpp`:
Implement `setLiquidGlassMode`:
```cpp
void AyuSettings::setLiquidGlassMode(LiquidGlassMode val) {
	if (_liquidGlassMode.current() == val) return;
	_liquidGlassMode = val;
	save();
	repaintApp();
}
```
In `to_json`:
```cpp
{"liquidGlassMode", s._liquidGlassMode.current()},
```
In `from_json`:
```cpp
s._liquidGlassMode = j.value("liquidGlassMode", defaults._liquidGlassMode.current());
```

- [ ] **Step 3: Commit Task 1**

```bash
git add Telegram/SourceFiles/ayu/ayu_settings.h Telegram/SourceFiles/ayu/ayu_settings.cpp
git commit -m "feat(ayu): add LiquidGlassMode setting and serialization"
```

---

### Task 2: Create `AyuLiquidGlass` Rendering Engine

**Files:**
- Create: `Telegram/SourceFiles/ayu/ui/ayu_liquid_glass.h`
- Create: `Telegram/SourceFiles/ayu/ui/ayu_liquid_glass.cpp`
- Modify: `Telegram/CMakeLists.txt:130-150`

**Interfaces:**
- Produces:
  ```cpp
  namespace AyuLiquidGlass {
  void paintGlass(
      QPainter &p,
      const QRect &rect,
      const QColor &tintColor,
      QWidget *widget = nullptr,
      bool topRim = true,
      bool bottomRim = true);
  bool isEnabled(LiquidGlassMode mode = LiquidGlassMode::ChatBars);
  }
  ```

- [ ] **Step 1: Write `ayu_liquid_glass.h`**

Define `AyuLiquidGlass` interface with `paintGlass` and `isEnabled`.

- [ ] **Step 2: Write `ayu_liquid_glass.cpp`**

Implement `paintGlass`:
1. Check `AyuSettings::getInstance().liquidGlassMode()`. If `Disabled`, draw solid `p.fillRect(rect, tintColor)`.
2. Grab backdrop if `widget` has a parent widget: sample background rect using `widget->parentWidget()->render(&bg, ...)` or cached pixmap when appropriate.
3. Apply `Images::Blur` or fast box blur.
4. Draw theme tint: `p.fillRect(rect, QColor(tintColor.red(), tintColor.green(), tintColor.blue(), 195))`.
5. Draw specular top rim (1px highlight `rgba(255, 255, 255, 40)`) and bottom divider rim (`rgba(0, 0, 0, 30)` or light divider).

- [ ] **Step 3: Register in `Telegram/CMakeLists.txt`**

Add `ayu/ui/ayu_liquid_glass.cpp` and `ayu/ui/ayu_liquid_glass.h` to `set(ayugram_files ...)`.

- [ ] **Step 4: Commit Task 2**

```bash
git add Telegram/SourceFiles/ayu/ui/ayu_liquid_glass.h Telegram/SourceFiles/ayu/ui/ayu_liquid_glass.cpp Telegram/CMakeLists.txt
git commit -m "feat(ayu): add AyuLiquidGlass rendering engine"
```

---

### Task 3: Add Liquid Glass Selector in AyuGram Settings

**Files:**
- Modify: `Telegram/SourceFiles/ayu/ui/settings/settings_general.cpp`

**Interfaces:**
- Consumes: `AyuSettings::getInstance().liquidGlassMode()`, `AyuSettings::getInstance().setLiquidGlassMode(mode)`

- [ ] **Step 1: Add setting UI in `settings_general.cpp`**

Under the Interface / General settings section, add radio or dropdown choice:
- "Стиль Liquid Glass (iOS)"
- Options:
  1. "Выключено" -> `LiquidGlassMode::Disabled`
  2. "Только в чатах" -> `LiquidGlassMode::ChatBars`
  3. "Полный интерфейс" -> `LiquidGlassMode::Full`

- [ ] **Step 2: Verify settings change triggers `repaintApp()`**

Ensure selecting a different option immediately updates `AyuSettings::getInstance().liquidGlassMode()`.

- [ ] **Step 3: Commit Task 3**

```bash
git add Telegram/SourceFiles/ayu/ui/settings/settings_general.cpp
git commit -m "feat(ayu): add Liquid Glass option in general settings"
```

---

### Task 4: Integrate Liquid Glass into Top Bar (`HistoryView::TopBarWidget`)

**Files:**
- Modify: `Telegram/SourceFiles/history/view/history_view_top_bar_widget.cpp:534-555`

**Interfaces:**
- Consumes: `AyuLiquidGlass::paintGlass`, `AyuSettings::getInstance().liquidGlassMode()`

- [ ] **Step 1: Include `ayu/ui/ayu_liquid_glass.h`**

Include the header in `history_view_top_bar_widget.cpp`.

- [ ] **Step 2: Update `TopBarWidget::paintEvent`**

Replace:
```cpp
p.fillRect(QRect(0, 0, width(), st::topBarHeight), st::topBarBg);
```
With:
```cpp
if (AyuLiquidGlass::isEnabled(LiquidGlassMode::ChatBars)) {
    AyuLiquidGlass::paintGlass(p, QRect(0, 0, width(), st::topBarHeight), st::topBarBg->c, this, false, true);
} else {
    p.fillRect(QRect(0, 0, width(), st::topBarHeight), st::topBarBg);
}
```

- [ ] **Step 3: Commit Task 4**

```bash
git add Telegram/SourceFiles/history/view/history_view_top_bar_widget.cpp
git commit -m "feat(ayu): integrate liquid glass into chat top bar"
```

---

### Task 5: Integrate Liquid Glass into Compose Controls (`HistoryView::ComposeControls`)

**Files:**
- Modify: `Telegram/SourceFiles/history/view/controls/history_view_compose_controls.cpp:415-430`

**Interfaces:**
- Consumes: `AyuLiquidGlass::paintGlass`, `AyuSettings::getInstance().liquidGlassMode()`

- [ ] **Step 1: Include `ayu/ui/ayu_liquid_glass.h`**

Include the header in `history_view_compose_controls.cpp`.

- [ ] **Step 2: Update `ComposeControls::paintEvent`**

In `ComposeControls::paintEvent`:
Replace flat `p.fillRect(rect(), st::historyComposeAreaBg);` with:
```cpp
if (AyuLiquidGlass::isEnabled(LiquidGlassMode::ChatBars)) {
    AyuLiquidGlass::paintGlass(p, rect(), st::historyComposeAreaBg->c, this, true, false);
} else {
    p.fillRect(rect(), st::historyComposeAreaBg);
}
```

- [ ] **Step 3: Commit Task 5**

```bash
git add Telegram/SourceFiles/history/view/controls/history_view_compose_controls.cpp
git commit -m "feat(ayu): integrate liquid glass into message compose controls"
```

---

### Task 6: Integrate Liquid Glass into Full UI (Dialogs Header & Side Menu)

**Files:**
- Modify: `Telegram/SourceFiles/dialogs/dialogs_widget.cpp`
- Modify: `Telegram/SourceFiles/window/window_main_menu.cpp`

**Interfaces:**
- Consumes: `AyuLiquidGlass::paintGlass`, `AyuSettings::getInstance().liquidGlassMode()`

- [ ] **Step 1: Update `Dialogs::Widget` header painting**

When `liquidGlassMode == LiquidGlassMode::Full`, paint the top search / header bar with `AyuLiquidGlass::paintGlass`.

- [ ] **Step 2: Update `Window::MainMenu` background painting**

When `liquidGlassMode == LiquidGlassMode::Full`, paint the slide-out menu background with `AyuLiquidGlass::paintGlass`.

- [ ] **Step 3: Commit Task 6**

```bash
git add Telegram/SourceFiles/dialogs/dialogs_widget.cpp Telegram/SourceFiles/window/window_main_menu.cpp
git commit -m "feat(ayu): integrate liquid glass into full UI"
```

---

### Task 7: Verification & Final Polish

- [ ] **Step 1: Verify git status and diff**
Check that all modified files contain zero added comments as instructed.
- [ ] **Step 2: Verify build system consistency**
Ensure CMakeLists includes new files and syntax is valid.
