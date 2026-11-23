# Flatten Multi-Tile Ground: repeat the top-left part on an empty anchor

## 1. Pattern & Key Files

- `source/rendering/core/sprite_footprint.h` — `FlattenGroundPart()` (Decide, pure): picks the part to repeat.
- `source/rendering/core/ground_flatten.{h,cpp}` — `ResolveGroundFlattenPart()`: option + 1x1 early-out + cached footprint.
- `source/rendering/core/chunk_cache_manager.cpp` — ground bake and in-place animation patch (`forEachSpriteRegion(..., anchor_part, ...)`, `AnimatedSpriteRun::flatten_column/row`).
- `source/rendering/drawers/entities/item_drawer.cpp` — classic path (`BlitItem` composite branch).
- `Config::FLATTEN_MULTI_TILE_GROUND` / `DrawingOptions::flatten_multi_tile_ground`, View menu "Flatten Multi-Tile Ground" (default on).

## 2. When to Use It

- Use it for grounds such as mountain 919: a multi-part ground whose anchor part (0,0) is empty and whose top-left part is opaque. Without it the anchor tile shows no pixel of its own ground.
- Draw the extra part inside the normal ground draw, never as a separate overlay: it then inherits tint, selection darkening, house id/flags and depth, and is skipped where the ground is skipped (tech/meta/invalid).
- Do not use it for walls or items, and do not hardcode item ids: the rule is geometry-only.

## 3. Minimal Usage

```cpp
const auto part = rme::rendering::ResolveGroundFlattenPart(options, definition.isGroundTile(), *sprite);
if (part) {
    for (int layer = 0; layer < sprite->layers; ++layer) {
        const AtlasRegion* region = sprite->getAtlasRegion(part->column, part->row, layer, subtype, px, py, pz, frame);
        // draw `region` at the anchor part's position (tile minus draw offset), ground colour and depth
    }
}
```

## 4. Invariants & Pitfalls

- The part uses the anchor's Ground depth. Neighbour parts (larger x+y) and the anchor's borders (Border sublayer) are closer and stay on top under `GL_LEQUAL`; transparent texels are discarded and write no depth.
- The chunk bake and the animation patch must visit the same regions: the run stores the flatten part, otherwise the patch sees a different instance count and forces a re-bake.
- Toggling the option sets `chunk_bake_dirty_` and re-bakes all chunks.
- The in-game preview builds its own options; it reads the setting next to `SHOW_SHADE`.
- If a sprite's pixels cannot be decoded, its footprint is conservative (anchor opaque) and stays cached after 3 attempts, so that sprite is not flattened.
- Purely visual: no OTBM, item data, walkability or undo changes. The pathing wash stays on the anchor tile.
