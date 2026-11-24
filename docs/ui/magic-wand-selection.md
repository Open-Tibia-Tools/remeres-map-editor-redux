# Magic wand selection: picked targets, mode-driven plans and one-step select actions

## 1. Pattern & Key Files

- `source/editor/operations/magic_wand_plan.{h,cpp}`: pure decision layer. `ResolveTarget` turns a `SelectionPicker` hit into a typed target plus `TargetNames` (brush/family/creature name and the clicked item's definition name), `BuildPlan` collects positions under a `SearchMode`, `SelectMatching`/`SameSelection` decide what changes on a tile.
- `source/editor/operations/magic_wand.{h,cpp}`: the apply half. One `ACTION_SELECT`, one `Change` per changed tile, committed through `Selection::commitPreparedAction`.
- `source/editor/connected_positions.h` + `source/map/position_rect.h`: flood fill and border ring, both bounded by a `PositionRect`.
- `source/rendering/ui/selection_controller.cpp` / `keyboard_handler.cpp` / `map_display.cpp`: hold-Tab gesture, picker hit, visible rect, release consumption, ESC clearing.
- `tests/test_magic_wand_plan.cpp`: resolution, identity names, both modes, collectors, cancellation and item-level selection.

## 2. When to Use It

- Hold **Tab** and click: the target is whatever `SelectionPicker` reports under the cursor, so Pixel Perfect and Standard selection both work. Held Tab owns the click in **drawing mode too**, so a brush never paints while it is down.
- The **Selection Method** menu holds two selectors, "Magic Wand (Viewport)" and "Magic Wand (Neighbour)" (`Config::MAGIC_WAND_MODE`, `MagicWand::SanitizeSearchMode`): every target kind collects one way, so the tool never mixes a viewport-wide scan with a connected flood.
- **Viewport**: every match in the visible rectangle; a ground also takes the border pieces of its brush found there.
- **Neighbour**: only the matches touching the click, diagonals included (an eight-connected flood); a ground takes its patch plus the border ring. A checkerboard of one ground is one connected patch.
- Ctrl adds to the selection. In selection mode Shift still drags a rectangle and Alt still draws a lasso.
- Identity is the brush plus **two names** (`TargetNames`): the brush/family/creature name and the clicked item's definition name. Brush names come from materials.xml, definition names from the client data, so grounds, borders and variants join their siblings through the item name even when the brushes differ or are nameless. The empty definition name is a name like any other: nameless items form one family.
- **ESC** clears the whole selection on every floor through `SelectionController::ClearSelection` (the keyboard twin of right-clicking empty ground).

## 3. Minimal Usage

```cpp
const TileLookup lookup = [&editor](const Position& p) { return static_cast<const Tile*>(editor.map.getTile(p)); };
const auto resolved = MagicWand::ResolveTarget(hit, lookup);   // WandHit from SelectionPicker
if (!resolved) return;                                          // nothing selectable under the cursor
const auto plan = MagicWand::BuildPlan(resolved->target, resolved->names, mode, lookup,
    resolved->origin, viewport, [&](std::string_view phase, size_t done, size_t total) { return progress.Tick(phase, done, total); });
```

## 4. Invariants & Pitfalls

- Every search is clamped to the visible floor rectangle plus a 2-tile tolerance, including the border ring; the viewport scan iterates that rect, never `map.tiles()`.
- A clicked border resolves to the ground that produced it: the tile's own ground first, then the eight neighbours' grounds, and the plan starts from the owner's tile. A border nobody owns becomes its name family instead.
- `ResolvedTarget::origin` is the collection start, which for ring borders differs from the clicked tile. Passing the hit position instead would flood from a non-member and yield an empty plan.
- Doodads can live in `Tile::ground` (`Tile::addItem` moves ground-typed items there): resolution, collection and selection all check both slots. Spawns are out of scope; a spawn hit resolves the tile's top item.
- Creatures only ever match creatures, by name. Nameless entities share the empty name and form one family.
- A ground patch flood chains through **grounds only** (the brush's own or a ground slot carrying an identity name). Same-name items and border pieces never bridge two separated patches: they are collected through the ring (Neighbour) or the viewport scan instead. Without this rule, a border run named like its ground would chain the flood into every patch it touches.
- Collect positions while the progress bar may pump events and fetch the `Tile*` per iteration: a pumped event can change the selection's tile vector, and on a live server a remote edit dispatched during a pump can swap or destroy the tile under a held pointer. Never keep a `Tile*` across a tick.
- A live client gets no progress bar; closing any bar broadcasts the live-server operation as finished. Cancelling returns before any `Action` exists, so the previous selection stays.
- `Selection::commitPreparedAction`/`finish` decide the Lua `selectionChange` event from the committed action size, not from the flag: `Action::commit` runs a nested INTERNAL session that clears it.
- `positionKey` packs x into bits >= 21, y into 4..20 and z into 0..3; the static_asserts next to it pin the map limits that keep the packing injective. The floor must be part of the key because the cleared selection can hold tiles from any floor.
- The replace tool lists only entities whose `isSelected()` is set, and its Selection scope replaces only those entities: rectangle selections select the whole tile, so their result is unchanged.
