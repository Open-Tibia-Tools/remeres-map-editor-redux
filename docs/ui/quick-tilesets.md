# Quick Tilesets, Recent and Favorites

## 1. Pattern & Key Files

- `palette/quick_tilesets/quick_tileset_model.h` — pure model (Recent 20/60, sets, favorites, tab ops, JSON).
- `palette/quick_tilesets/brush_collection_policy.h`, `brush_collection_menu_model.h` — pure decisions: which brushes are collected, which context menu entries exist.
- `palette/quick_tilesets/brush_reference.{h,cpp}` — `Brush*` ↔ `"r:<id>" / "c:<name>" / "b:<name>"`; resolving creates lazy RAW/creature brushes.
- `palette/quick_tilesets/quick_tileset_store.{h,cpp}` — `quick-tilesets.json` in `FileSystem::GetLocalDataDirectory()`, one section per client version.
- `palette/quick_tilesets/quick_tilesets.{h,cpp}` — facade: pane lifetime, Recent feed, favorites, context menu dispatcher, drag & drop.
- `palette/quick_tilesets/quick_tilesets_window.{h,cpp}` — the AUI pane (Recent + user sets, Brushes/RAW grids).
- `palette/favorites/favorites_palette.{h,cpp}` — "Favorites" palette page; toggle in `BrushItemsWindow`.

## 2. When to Use It

- Feed Recent only from user picks: `EVT_VIRTUAL_BRUSH_GRID_PICKED` (grid click, search commit). Do not call `RememberUserPick` from `BrushManager::SelectBrush`: Lua, toolbars, eyedroppers and hotkeys would flood the list and the file.
- Change state only through `QuickTilesets::Apply(change, scope)`; the scope decides what refreshes.
- Keep `VirtualBrushGrid` free of this feature: it sends `wxEVT_CONTEXT_MENU`, `..._BEGIN_DRAG` and `..._PICKED`; `PaletteWindow` and the pane handle them.
- Don't store `Brush*` across a client version change: the pane is destroyed with the version.

## 3. Minimal Usage

```cpp
Bind(EVT_VIRTUAL_BRUSH_GRID_PICKED, [](wxCommandEvent& event) {
	QuickTilesets::RememberUserPick(static_cast<Brush*>(event.GetClientData()));
});
QuickTilesets::Apply([&](QuickTilesets::State& state) {
	return QuickTilesets::RenameTab(state, id, name);
}, QuickTilesets::ChangeScope::Tabs);
```

## 4. Invariants & Pitfalls

- Every change saves synchronously: the file is re-read and only the current client version's section is replaced (other versions and a second editor instance keep their data), then committed with `wxTempFile`.
- A file that cannot be read or parsed is never overwritten: the store is read-only and the error is shown once. A failed save also switches to read-only; later changes stay in memory.
- References that the loaded data does not have are kept in the set and counted in the hint.
- Lifetime: `EnsureWindow()` from `LayoutManager::LoadPerspective` (version loaded), `DestroyWindow()` in `VersionManager::LoadVersion`, `UnloadVersion` and `MainFrame::OnExit` before `aui_manager->UnInit`. Layout is `Config::QUICK_TILESETS_LAYOUT`.
- The file schema matches BME (`palette` is kept but unused), so a BME file can be copied over.
- Palette-context and tool brushes (house, waypoint, zone, temple, eraser, door, ...) are never collected.
