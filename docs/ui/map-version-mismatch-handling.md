# Map Version Mismatch Handling & Force Load

## 1. Pattern & Key Files
- `editor/persistence/map_load_decision.h`: Stateless `IsClientCompatibleWithMap` and `DecideMapLoadAction` rules shared across loading and persistence.
- `ui/dialogs/client_mismatch_dialog.{h,cpp}`: Themed modal dialog using `Theme::Grid` / `Theme::GetFont` and `ApplyThemeButtonStyle` for a single horizontal color-coded footer (`Force Load`, `Switch`, `Cancel`).
- `editor/managers/editor_manager.{h,cpp}`: Orchestration via `SwitchClientVersionForLoad`, `CountRealOpenMaps`, and deferred placeholder cleanup.
- `tests/test_map_load_decision.cpp`: Catch2 unit test coverage.

## 2. When to Use It
Do:
- Share `IsClientCompatibleWithMap` so pre-load decisions and `EditorPersistence::loadMap` agree on protocol + `items_major` checks.
- Validate target paths (`hasValidPaths`/`loadValidPaths`) before `CloseAllEditors()`, and close empty placeholder tabs only after `EditorFactory::LoadFromFile` succeeds.
Don't:
- Don't mix wxWidgets UI into `map_load_decision.h` or bypass `DialogUtil::LoadWarningsDialog` on version switch.
- Don't count an unmodified, unnamed placeholder tab as an open user map.

## 3. Minimal Usage
```cpp
const auto decision = DecideMapLoadAction(decision_input);
if (decision.resolution == MapLoadResolution::PromptMismatch) {
	ClientMismatchDialog dlg(g_gui.root, *current, *target, ver.items_major, CountRealOpenMaps(), target->hasValidPaths());
	dlg.ShowModal();
}
```

## 4. Invariants & Pitfalls
- **Re-read active client after modal**: Fetch `g_version.getLoadedVersion()` after `dlg.ShowModal()` before applying `ForceLoadActive`.
- **Safe default**: Default to Switch when `CountRealOpenMaps() == 0`; default to Force Load when real maps are already open.
