# Liquid Glass UI Effect (iOS Style) in AyuGram Desktop

## Overview
Add an optional "Liquid Glass" visual design mode to AyuGram Desktop, inspired by the iOS 26 / Telegram iOS design language.
The feature introduces frosted glass translucency, real-time background blur, theme-adaptive tinting, and delicate specular light rims to key interface components.

## Settings & Data Model

### `LiquidGlassMode` Enum
Defined in `Telegram/SourceFiles/ayu/ayu_settings.h`:
```cpp
enum class LiquidGlassMode {
    Disabled = 0,
    ChatBars = 1,
    Full = 2,
};
```

### Settings Storage
- Stored in `tdata/ayu_settings.json` under key `"liquidGlassMode"`.
- Default value: `LiquidGlassMode::Disabled`.
- Loaded and saved via `AyuSettings::load()` and `AyuSettings::save()`.

### Settings UI
Located in **AyuGram Settings** -> **General / Interface** (`Telegram/SourceFiles/ayu/ui/settings/settings_general.cpp`):
- Single choice selector:
  - "Disabled"
  - "Chat bars only" (Top bar and bottom compose field)
  - "Full interface" (Chat bars + Dialogs search/header + Drawer menu)
- Reactive update: calls `repaintApp()` immediately on change without requiring an app restart.

## Visual Engine (`AyuLiquidGlass`)

A dedicated rendering utility module `ayu/ui/ayu_liquid_glass.h` and `ayu_liquid_glass.cpp`:

### Glass Material Properties
1. **Backdrop Blur**:
   - Samples the underlying widget or window background behind the glass panel.
   - Applies fast downscaled Gaussian/box blur (`Images::Blur` from `ui/image/image_prepare.h`).
   - Caches the blurred buffer to maintain 60-144 FPS during idle or regular typing.
2. **Theme Tinting**:
   - Blends with `st::topBarBg` or `st::historyComposeAreaBg` with 70-80% opacity.
   - Light themes: Milky, clean frosted glass appearance.
   - Dark themes: Deep graphite, high-contrast dark glass appearance.
3. **Specular Rim (Optical Edges)**:
   - 1px highlight along the top edge (`QColor(255, 255, 255, 45)` in light mode, `QColor(255, 255, 255, 25)` in dark mode) to simulate light refraction in polished glass.
   - 1px soft division line along the bottom/outer edge to separate the glass plane from the scrolling content.

## Integration Points

### 1. Chat Top Bar (`HistoryView::TopBarWidget`)
- Located in `Telegram/SourceFiles/history/view/history_view_top_bar_widget.cpp`.
- Active in: `ChatBars` and `Full` modes.
- Replaces flat `p.fillRect(..., st::topBarBg)` with `AyuLiquidGlass::paintGlass(...)`.

### 2. Message Compose Controls (`HistoryView::ComposeControls`)
- Located in `Telegram/SourceFiles/history/view/controls/history_view_compose_controls.cpp`.
- Active in: `ChatBars` and `Full` modes.
- Replaces flat background with `AyuLiquidGlass::paintGlass(...)`.

### 3. Dialogs Header & Search Bar (`Dialogs::Widget`)
- Located in `Telegram/SourceFiles/dialogs/dialogs_widget.cpp`.
- Active in: `Full` mode.
- Renders frosted glass background for the search and navigation area.

### 4. Side Drawer Menu (`Window::MainMenu`)
- Located in `Telegram/SourceFiles/window/window_main_menu.cpp`.
- Active in: `Full` mode.
- Renders frosted glass surface with blur behind the slide-out menu.

## Verification Plan
1. Toggle setting between Disabled, ChatBars, and Full in Settings -> verify reactive redraw without crash.
2. Verify scrolling in chat with complex media (images, stickers, text) -> verify smooth blur beneath top bar and compose controls.
3. Verify light and dark theme switching -> verify correct tint and specular highlights.
4. Verify persistence in `tdata/ayu_settings.json` across client restarts.
