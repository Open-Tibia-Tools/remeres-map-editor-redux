# Magic wand selection: connected positions and one-step select actions

## 1. Pattern & Key Files

- `source/editor/connected_positions.h`: pure geometry (no map, tile or wx). `collect` is a four-connected, single-floor flood with a membership predicate and a cancel callback. `borderRing` returns the eight-neighbour ring around a patch.
- `source/editor/operations/magic_wand.{h,cpp}`: picks the target (doodad > wall > ground), collects positions, then builds one `ACTION_SELECT`. Also holds the global toggle (`Config::MAGIC_WAND_SELECTION`).
- `source/rendering/ui/selection_controller.cpp`: a click without Shift or Alt runs the wand while the toggle is on. The release is consumed.
- `source/rendering/ui/keyboard_handler.cpp`: Tab on the map toggles the tool. `Selection > Selection Method > Magic Wand (Tab)` mirrors it.
- `tests/test_magic_wand.cpp`: Catch2 cases for the flood and the ring.

## 2. When to Use It

- Use `ConnectedPositions` for any "spread from a click" feature and keep map access in the predicate. Unit-test the geometry, not the predicate.
- When one tile needs several items selected at once, build the `ACTION_SELECT` yourself: take one `deepCopy` per tile, select on the copy, call `addChange` once, then commit with `Selection::commitPreparedAction`. That call keeps the Lua `selectionChange` event in step with `finish()`.
- Do not loop `Selection::add(tile, item)` inside a `start()`/`finish()` session for that. Each call copies the live tile, so only the last item of a tile survives. With `BORDER_IS_GROUND`, it also selects grounds you did not ask for.

## 3. Minimal Usage

```cpp
auto patch = ConnectedPositions::collect(origin,
    [&](const Position& p) { const Tile* t = map.getTile(p); return t && t->getGroundBrush() == ground; },
    [&](size_t found) { return progress.Tick("Finding connected tiles...", found); });
if (!patch) return; // cancelled: nothing changed
const PositionVector ring = ConnectedPositions::borderRing(*patch);
```

## 4. Invariants & Pitfalls

- Collect positions while the progress bar may pump events. Fetch `Tile*` only afterwards, and commit without pumping again.
- A live client gets no progress bar, because pumping socket events could close its editor mid-search.
- Cancelling returns before any `Action` exists, so the previous selection stays.
- Replace mode also adds deselect copies for previously selected tiles outside the target. `TileOperations::deselect` on those copies drops their spawn/creature selection too.
- `ACTION_SELECT` changes stay local: they are not sent to live peers and do not mark the map modified.
- Doodads are not connected by nature. The doodad target scans the whole current floor once.
