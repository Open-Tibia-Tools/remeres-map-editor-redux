# Floor-Local Viewport Tile Bounds

## 1. Pattern & Key Files
- `rendering/core/viewport_tile_bounds.h` — dependency-free `ViewportTileBounds` and `ComputeViewportTileBounds(...)`.
- `rendering/core/render_view.{h,cpp}` — `RenderView::ComputeFloorTileBounds(int map_z, int extra_margin_tiles)` delegating to `ComputeViewportTileBounds`.

## 2. When to Use It
Do:
- Call `view.ComputeFloorTileBounds(z, 1)` in drawers that emit vertices in floor-local shifted screen space (`x * TILE_SIZE + (-view_scroll_x - offset)`), especially when zoomed far in (e.g., 800%) with `Show all Floors` off.
- Unit-test floor-offset math against `viewport_tile_bounds.h` directly in Catch2 without pulling `glm` or GL headers.
Don't:
- Don't add the `(GROUND_LAYER - map_z)` offset globally to `RenderView::getBoundsForFloor`, because `GridDrawer` and `LightDrawer` depend on its unshifted scroll-space coordinates.

## 3. Minimal Usage
```cpp
const ViewBounds bounds = view.ComputeFloorTileBounds(z, 1);
if (bounds.end_x < bounds.start_x || bounds.end_y < bounds.start_y) {
    return;
}
```

## 4. Invariants & Pitfalls
- Uses `std::floor` on floating-point division by `TILE_SIZE` so negative coordinates round consistently downward.
- Returns inverted bounds (`end < start`) when `logical_width <= 0`, `logical_height <= 0`, or `tile_size <= 0`.
