# Multi-tile sprite geometry: render depth, picking and pathing footprint

## 1. Pattern & Key Files

- `source/rendering/core/render_depth.h` — canonical `(diagonal, sublayer)` depth model.
- `source/rendering/core/item_render_rules.h` — shared sublayer, common-step and elevation rules.
- `source/rendering/ui/pick_candidate_order.h` — candidate anchor order for pixel-perfect picking.
- `source/rendering/core/sprite_footprint.h` — opaque-part mask, tile coverage and reach bound.
- `source/rendering/core/game_sprite.cpp` — lazily built, cached footprint.
- `source/rendering/drawers/overlays/zone_overlay_drawer.cpp` — pathing wash built from the footprint union.
- `source/rendering/ui/selection_picker.cpp` — depth-ordered pixel-perfect picking.

## 2. When to Use It

Use `render_depth.h` + `item_render_rules.h` whenever code needs to sort item draws or hit-test them against pixels. Do not hardcode `4`, `10`, `elev > 0 ? 1 : 0` or common-item counters in another render path.

Do not use the footprint for gameplay/pathing logic. It is display-only: `IsTilePathBlocking()` remains the single source of truth for actual blocking.

## 3. Minimal Usage

```cpp
const auto layer = rme::rendering::ResolveItemSublayer(item.isAlwaysOnBottom(), definition.hasFlag(ItemFlag::TopEffect));
const int common_step = layer == rme::rendering::RenderSublayer::CommonItem
    ? rme::rendering::CommonItemElevationStep(common_index)
    : 0;
rme::rendering::AdvanceItemElevation(elevation_px, sprite->draw_height, sprite->hasElevation());
```

## 4. Invariants & Pitfalls

- Diagonal dominates sublayer. In a tie, the picker uses the chunk-cache painter order (`dx` descending, `dy` ascending); the dynamic/live renderer can differ at exactly equal depth. That ambiguity is documented and pinned by a test.
- Common-item depth steps are `0..3`. Hidden/meta items still consume a step, like `tile_renderer`.
- The footprint is the union over frames, patterns and layers. It is conservative and may wash parts that are transparent in the current animation frame.
- Tile coverage uses tile centres: a draw offset smaller than half a tile does not spill onto a neighbour; a 64 px part covers two tiles per axis.
- The footprint is built lazily and cached. If a pixel read fails it retries at most 3 times, then caches the conservative (over-wash) result until `invalidateMetricCaches()`.
- Visibility filters remain path-specific: renderers apply tech/invalid/pickupable rules at draw time, while the picker uses its own hit-test filters. This document unifies the depth/elevation math, not those policies.
- Anchor scans pad right/down by `kSpriteFootprintReachTiles` (34). Elevation larger than one tile can still clip at the right/bottom viewport edge.
- `drawoffset_x/y` is clamped to `[0, TILE_SIZE]` during DAT/protobuf parsing.
