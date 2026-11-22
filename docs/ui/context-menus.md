# Context menus: per tile vs per item

## 1. Pattern & Key Files

Two independent context menu models share the `MAP_POPUP_MENU_*` ids and the presentation table, but
have different commands, facts and dispatchers.

- Per tile (map canvas): `ui/tile_context_menu_model.h`, `ui/map_popup_menu.*`,
  `ui/tile_context_actions.*`.
- Per item (Tile Properties list): `ui/item_context_menu_model.h`, `ui/item_popup_menu.*`,
  `ui/item_context_actions.*`.
- Shared: `ui/context_menu_item_facts.h` (`CollectItemFacts`), `ui/context_menu_presentation.*`
  (labels/help/icons keyed by menu id).
- Row math and row delete: `map/tile_stack_rows.h`, `ui/tile_item_actions.h` (`DeleteRow`).

## 2. When to Use It

Do:
- Add canvas actions to `TileContextMenu` and item-row actions to `ItemContextMenu`.
- Put shared labels/help/icons in `ContextMenu::PresentMenuEntry`; item entries override row wording.
- Resolve the tile and row again inside `ItemContextActions::Execute`; never trust a stored pointer.
- Route row deletion through `TileItemActions::DeleteRow` (undoable, keeps one row selected).

Don't:
- Don't reuse the tile menu on a row surface: `Delete` removes tiles and `BORDER_IS_GROUND` expands the row.
- Don't call `Selection::add` from a row-scoped surface; use `Selection::setSingleItem`.
- Don't put GL, `wxMenu` or `Item` access into the model headers; they stay pure and unit-tested.

## 3. Minimal Usage

```cpp
item_list->SetSelection(row);
editor->selection.setSingleItem(tile, item);
if (g_gui.IsDrawingMode()) g_gui.SetSelectionMode(); // Copy needs selection mode
ItemPopupMenu menu;
menu.Update(ItemContextMenu::Facts { .item = ContextMenu::CollectItemFacts(*item) });
if (const int id = item_list->GetPopupMenuSelectionFromUser(menu, point); id != wxID_NONE) {
	ItemContextActions::Execute(id, *editor, tile->getPosition(), row);
}
```

## 4. Invariants & Pitfalls

- `CollectItemFacts` reads only the clicked item; tile-level facts belong to `TileContextMenu::Facts`.
- `Selection::setSingleItem` selects exactly one item and never expands to the ground/border layer.
- `ItemContextMenu` has no `Cut`, `Paste`, `Copy Position`, `Properties`, `Browse Field`,
  `Tile Properties`, or tile-level selectors; brush selectors need the matching item row.
- `TileItemActions::DeleteRow` re-resolves the row against the live tile and commits a `Change`.
- Only the tile menu shows Lua entries; the item menu is row-scoped and has no `Script` command.
