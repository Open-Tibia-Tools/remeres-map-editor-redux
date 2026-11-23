# Multi-tile sprite geometry: render depth, picking and opaque-part footprint

## 1. Pattern & Key Files

- `source/rendering/core/render_depth.h` — canonical `(diagonal, sublayer)` depth model.
- `source/rendering/core/item_render_rules.h` — shared sublayer, common-step and elevation rules.
- `source/rendering/ui/pick_candidate_order.h` — candidate anchor order for pixel-perfect picking.
- `source/rendering/core/sprite_footprint.h` — opaque-part mask of a composite sprite.
- `source/rendering/core/game_sprite.cpp` — lazily built, cached footprint.
- `source/rendering/ui/selection_picker.cpp` — depth-ordered pixel-perfect picking.

## 2. When to Use It

Use `render_depth.h` + `item_render_rules.h` whenever code needs to sort item draws or hit-test them against pixels. Do not hardcode `4`, `10`, `elev > 0 ? 1 : 0` or common-item counters in another render path.

Do not use the footprint for gameplay/pathing logic or the pathing wash. `IsTilePathBlocking()` is the single source of truth: a multi-tile blocking item washes its anchor tile only, because only that tile blocks in game. The footprint is display-only (see `docs/rendering/flatten-multi-tile-ground.md`).

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
- The footprint is the union over frames, patterns and layers: a part counts as opaque if any frame draws a visible texel there.
- The footprint is built lazily and cached. If a pixel read fails it retries at most 3 times, then caches the conservative result (part opaque) until `invalidateMetricCaches()`.
- Visibility filters remain path-specific: renderers apply tech/invalid/pickupable rules at draw time, while the picker uses its own hit-test filters. This document unifies the depth/elevation math, not those policies.
- `drawoffset_x/y` is clamped to `[0, TILE_SIZE]` during DAT/protobuf parsing.
