# Magic wand selection: picked targets, visible-area plans and one-step select actions

## 1. Pattern & Key Files

- `source/editor/operations/magic_wand_plan.{h,cpp}`: pure decision layer. `ResolveTarget` turns a `WandHit` from `SelectionPicker` into a typed target (ground/wall/carpet/table/doodad brush, same-name item family or creature name), `BuildPlan` collects positions, `SelectMatching`/`SameSelection` decide what changes on a tile.
- `source/editor/operations/magic_wand.cpp`: the apply half. One `ACTION_SELECT`, one `Change` per changed tile, committed through `Selection::commitPreparedAction`.
- `source/editor/connected_positions.h` + `source/map/position_rect.h`: flood fill and border ring, both bounded by a `PositionRect`.
- `source/rendering/ui/selection_controller.cpp` / `keyboard_handler.cpp` / `map_display.cpp`: hold-Tab gesture, picker hit, visible rect, release consumption.
- `tests/test_magic_wand_plan.cpp`: target resolution, families, collectors, viewport, cancellation and item-level selection.

## 2. When to Use It

- Hold **Tab** and click: the target is whatever `SelectionPicker` reports under the cursor, so Pixel Perfect and Standard selection both work and the click order matches what the user sees (top item before ground). Held Tab owns the click in **drawing mode too**, so a brush never paints while it is down.
- Ctrl adds to the selection. In selection mode Shift still drags a rectangle and Alt still draws a lasso; in drawing mode the wand always runs. Tab is a held modifier, not a toggle.
- Reach for `ConnectedPositions` for any "spread from a click" feature and keep map access in the caller's lookup.
- Build one `ACTION_SELECT` yourself when a tile needs several entities selected: one `deepCopy`, select on the copy, one `addChange`.

## 3. Minimal Usage

```cpp
const auto target = MagicWand::ResolveTarget(hit); // WandHit from SelectionPicker
if (!target || !hit.tile) return;                   // nothing selectable under the cursor
const auto plan = MagicWand::BuildPlan(*target, lookup, hit.tile->getPosition(), viewport,
    [&](std::string_view phase, size_t done, size_t total) { return progress.Tick(phase, done, total); });
if (!plan) return; // cancelled: nothing changed
```

## 4. Invariants & Pitfalls

- Every search is clamped to the visible floor rectangle plus a 2-tile tolerance (and the +1 tile from the client-size corner), including the border ring.
- Selection identity is the entity name (brush name, item definition name or creature name). A wall, a doodad and a raw item that share a name join the same connected selection; brush ownership still covers its own variants and borders.
- Doodads, creatures and same-name item families are scattered, so they scan the whole visible rectangle; ground, wall, carpet and table brushes flood four-connected.
- Items without a brush (raw, decorations, identical-looking variants) are matched by `ItemDefinitionView::name()`; nameless items fall back to their id so unrelated items never group.
- Doodads can live in `Tile::ground` (`Tile::addItem` moves ground-typed items there): target resolution, collection and selection all check both slots.
- Spawns are deliberately out of scope; a spawn hit resolves the tile's top item instead.
- Collect positions while the progress bar may pump events; fetch `Tile*` only afterwards and never keep a map iterator across a tick. The doodad/creature scan iterates the visible rectangle, never `map.tiles()`.
- A live client gets no progress bar (pumping socket events could close its editor); closing any bar broadcasts the live-server operation as finished.
- Cancelling returns before any `Action` exists, so the previous selection stays.
- `Selection::commitPreparedAction`/`finish` decide the Lua `selectionChange` event from the committed action size, not from the flag: `Action::commit` runs a nested INTERNAL session that clears it.
- The replace tool lists only entities whose `isSelected()` is set, and its Selection scope replaces only those entities: the context menu seeds rules from the selected items, and the engine (preview and execution) ignores unselected ground and stack items on the touched tiles. A container brings its contents into scope. Rectangle selections select the whole tile, so their result is unchanged.
