# Two-Phase Version Switch & Non-Destructive Tab Close

## 1. Pattern & Key Files
- `app/managers/prepared_version_load.h`: Owns `PreparedVersionLoad` staging state (`version`, `modular_data_path`, `manifest_files`, `AssetBundle`, `has_otb`).
- `app/managers/version_manager.{h,cpp}`: `CheckModularClientData`, `PrepareVersionLoad` (off-world staging with RAII load bar), and `CommitPreparedVersion` (destructive cutover).
- `editor/managers/editor_manager.cpp`: `SwitchClientVersionForLoad` and two-phase `CloseAllEditors`.

## 2. When to Use It
Do:
- Validate modular data (`materials.xml`, `items.otb` for OTB modes, reject `DatSrv`), client paths (`hasConfiguredClientPath()` + `hasValidPaths()`), and run `PrepareVersionLoad` before closing tabs or unloading the active client.
- Prompt once per unique `Editor*` in `CloseAllEditors` Pass 1 before deleting any `MapTab` in Pass 2.
Don't:
- Don't call `loadValidPaths()` during `SwitchClientVersionForLoad`—users configure client folders beforehand in Preferences.
- Don't delete tabs inside `CloseAllEditors` Pass 1 or leave `g_loading` load bar open across modal save prompts.

## 3. Minimal Usage
```cpp
PreparedVersionLoad prepared;
if (!g_version.PrepareVersionLoad(target.getID(), prepared, error, warnings)) {
	return false; // Active tabs and loaded client remain untouched
}
if (!CloseAllEditors()) {
	return false; // User cancelled save; loaded client remains intact
}
return g_version.CommitPreparedVersion(prepared, error, warnings);
```

## 4. Invariants & Pitfalls
- **RAII load bar**: `PrepareVersionLoad` must destroy its progress dialog before returning so `CloseAllEditors()` save prompts are never blocked by a modal progress bar.
- **Unique editor prompts**: Multiple split-view tabs share one `Editor*`; deduplicate Pass 1 by `Editor*` using snapshotted `wxWindow*` handles re-resolved via `GetTabIndex`.
- **Install failure cleanup**: `CommitPreparedVersion` sets `g_gui.gfx.client_version` and `last_load_has_otb` before `install`, and routes errors through `FailDataLoad()`.
