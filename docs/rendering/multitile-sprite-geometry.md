# Multi-tile sprite geometry: render depth and pixel-perfect picking

## 1. Pattern & Key Files

- `source/rendering/core/render_depth.h` — canonical `(diagonal, sublayer)` depth model.
- `source/rendering/core/item_render_rules.h` — shared sublayer, common-step and elevation rules.
- `source/rendering/ui/pick_candidate_order.h` — candidate anchor order for pixel-perfect picking.
- `source/rendering/ui/selection_picker.cpp` — depth-ordered pixel-perfect picking.

## 2. When to Use It

Use `render_depth.h` + `item_render_rules.h` whenever code needs to sort item draws or hit-test them against pixels. Do not hardcode `4`, `10`, `elev > 0 ? 1 : 0` or common-item counters in another render path.

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
- Visibility filters remain path-specific: renderers apply tech/invalid/pickupable rules at draw time, while the picker uses its own hit-test filters. This document unifies the depth/elevation math, not those policies.
