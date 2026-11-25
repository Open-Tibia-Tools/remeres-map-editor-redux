# Map Version Mismatch Handling & Force Load

## 1. Pattern & Key Files
- `editor/persistence/map_load_decision.h`: Stateless `IsClientCompatibleWithMap` and `DecideMapLoadAction` rules shared across loading and persistence.
- `ui/dialogs/client_mismatch_presentation.h`: Stateless presentation builder formatting inline `OTBM X · OTB M.m` metadata (`\xC2\xB7`) and card labels.
- `ui/dialogs/client_mismatch_dialog.{h,cpp}`: Minimal two-card comparison dialog (`Loaded Client` -> `<map_filename>`) with equal-width `Switch Client | Force Load | Cancel` footer.
- `editor/managers/editor_manager.{h,cpp}`: Orchestration via `SwitchClientVersionForLoad`, `CountRealOpenMaps`, and deferred placeholder cleanup.
- `tests/test_map_load_decision.cpp` & `tests/test_client_mismatch_presentation.cpp`: Catch2 unit test coverage.

## 2. When to Use It
Do:
- Share `IsClientCompatibleWithMap` so pre-load decisions and `EditorPersistence::loadMap` agree on protocol + `items_major` checks.
- Pass `MapVersion` and UTF-8 map filename (`fileName.GetFullName().utf8_string()`) into `ClientMismatchDialog` for inline OTBM/OTB display.
Don't:
- Don't mix wxWidgets UI into `map_load_decision.h` or `client_mismatch_presentation.h`, or use raw non-ASCII literals in C++ source (`CMakeLists.txt` does not pass `/utf-8` on MSVC).

## 3. Minimal Usage
```cpp
const auto decision = DecideMapLoadAction(decision_input);
if (decision.resolution == MapLoadResolution::PromptMismatch) {
	ClientMismatchDialog dlg(g_gui.root, *current, *target, ver, fileName.GetFullName().utf8_string(), CountRealOpenMaps());
	dlg.ShowModal();
}
```

## 4. Invariants & Pitfalls
- **Re-read active client after modal**: Fetch `g_version.getLoadedVersion()` after `dlg.ShowModal()` before applying `ForceLoadActive`.
- **Default button**: `Force Load` (`PrimaryButton`, `SetDefault`) is always the default primary button; `Switch Client` (`SecondaryButton`) still performs path validation inside `SwitchClientVersionForLoad` before closing open tabs.
- **Save identity after Force Load**: Forced-load maps are intentionally saved with the active loaded client's `g_item_definitions` major/minor versions (`IOMapOTBM::saveMap`), without a separate conversion confirmation prompt.
