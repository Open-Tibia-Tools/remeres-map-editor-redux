# Map Version Mismatch Handling & Force Load

## 1. Pattern & Key Files
- `editor/persistence/map_load_decision.h` & `editor/persistence/switch_client_preflight.h`: Stateless load decision and `EvaluateSwitchPrecondition` rules.
- `ui/dialogs/client_mismatch_presentation.h`: Stateless presentation builder formatting `OTBM X · OTB M.m` (`\xC2\xB7`), tooltips, and switch-unavailable notices.
- `ui/dialogs/client_mismatch_dialog.{h,cpp}`: Two-card comparison dialog with `Switch Client | Force Load | Cancel` footer (never opens a folder picker).
- `editor/managers/editor_manager.{h,cpp}`: `SwitchClientVersionForLoad`, `CountRealOpenMaps`, and non-destructive two-phase `CloseAllEditors`.
- `tests/test_map_load_decision.cpp`, `tests/test_switch_client_preflight.cpp`, `tests/test_client_mismatch_presentation.cpp`: Catch2 coverage.

## 2. When to Use It
Do:
- Evaluate `SwitchPreconditionStatus` via const `hasConfiguredClientPath() && getClientPath().DirExists()` and `CheckModularClientData` before showing `ClientMismatchDialog`.
- Pass `MapVersion`, UTF-8 map filename, and `SwitchPreconditionStatus` into `ClientMismatchDialog`.
Don't:
- Don't call `loadValidPaths()` during map open/mismatch handling, mix wxWidgets UI into presentation/preflight headers, or use raw non-ASCII literals in C++.

## 3. Minimal Usage
```cpp
const auto status = EvaluateSwitchPrecondition({
	.is_same_client = current->getID() == target->getID(),
	.has_modular_data = g_version.CheckModularClientData(*target, err),
	.has_configured_client_paths = target->hasConfiguredClientPath() && target->getClientPath().DirExists(),
});
ClientMismatchDialog dlg(g_gui.root, *current, *target, ver, filename, CountRealOpenMaps(), status);
```

## 4. Invariants & Pitfalls
- **Const check before modal**: Never call non-const `hasValidPaths()` when constructing `ClientMismatchDialog` (it emits `wxLogError` dialogs).
- **Default button**: `Force Load` (`PrimaryButton`, `SetDefault`) is always the default primary button; `Switch Client` is disabled when `status != Ready`.
- **Save identity after Force Load**: Forced-load maps save with the active loaded client's `g_item_definitions` versions without extra prompts.
