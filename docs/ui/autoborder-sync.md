# Autoborder State, Capability & UI Sync

## 1. Pattern & Key Files
- `brushes/brush.h`, `brushes/door/door_brush.h` — `Brush::supportsAutoborder()` (separate from `needBorders()`).
- `ui/autoborder_ui_state.h` — pure Decide predicates and status labels (`AutoborderStatusLabel`, `ShouldRunAutoborderPreview`).
- `ui/gui_autoborder_ext.cpp` — `GUI::SetAutoborderEnabled(bool)` and `GUI::UpdateAutoborderPreview(Position)`.
- `ui/tool_options_surface.{h,cpp}` — `"Enable Autoborder (A)"` & `"Preview Border"` paired via `HasAutoborderControls()` and synced via `g_settings.addObserver`.
- `ui/managers/status_manager.{h,cpp}` — `AutoborderStatusLabel()` and `UpdateAutoborderIndicator()`.

## 2. When to Use It
Do:
- Route all UI/hotkey Autoborder toggles through `g_gui.SetAutoborderEnabled(bool)` so settings, menu checkmark, status bar field 4, Brush Options checkbox, and stationary-cursor preview stay synchronized.
- Gate both Brush Options border checkboxes and `AutoborderPreviewManager` on `brush->supportsAutoborder()`.
Don't:
- Don't override `needBorders()` on single-tile brushes like `DoorBrush` just to show UI/preview (`DrawingController` uses `needBorders()` for radius/area tile collection).
- Don't rename `Config::USE_AUTOMAGIC` / `Config::BORDER_IS_GROUND` (breaks existing `config.toml`).

## 3. Minimal Usage
```cpp
if (IsMutatingUi()) {
	return;
}
g_gui.SetAutoborderEnabled(event.IsChecked());
g_gui.FocusCurrentMapCanvas();
```

## 4. Invariants & Pitfalls
- Always unsubscribe `Settings::ObserverId` via `g_settings.removeObserver(...)` in `~ToolOptionsSurface()`.
- Wrap observer-driven `SetValue(...)` calls in `SetMutatingUi(true/false)` to prevent re-entrant checkbox events.
- When `DoorBrush` runs in `AutoborderPreviewManager`, never clean walls or wallize `tilestodraw` (only `tilestoborder` neighbors).
