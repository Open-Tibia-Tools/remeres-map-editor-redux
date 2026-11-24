# Lasso Selection (grid-line tile outline)

## 1. Pattern & Key Files

- `source/editor/lasso_geometry.h` — polygon even-odd fill, `IntersectsTile`, `ComputeLassoTileBounds`.
- `source/editor/lasso_tile_outline.h` — `LassoOutline::Compute`: pure O(perimeter) boundary segments.
- `source/rendering/ui/lasso_gesture.h` — pure gesture state machine (point cap 4096, 8 DIP sampling).
- `source/rendering/ui/selection_controller.{h,cpp}` — builds the polygon in input events and computes the outline.
- `source/rendering/drawers/overlays/lasso_tile_outline_drawer.{h,cpp}` — NanoVG grid lines from the segments.
- `source/rendering/core/map_view_math.{h,cpp}` — `MapTileToScreenDIP`: float tile→screen conversion for overlays.
- `source/rendering/drawers/overlays/lasso_overlay_drawer.cpp` — lasso path, rubber band, click handles only.
- `source/editor/selection_thread.cpp` — the real selection; identical tile rule.

## 2. When to Use It (Do's & Don'ts)

- Do use `LassoOutline::Compute` for the boundary; it is O(perimeter) and property-tested against brute force.
- Do keep the preview on the current floor (box-selection parity): the line encloses exactly the tiles under the drawn gesture.
- Don't tint items during the preview — the tint appears after Alt release, like box selection.
- Don't add an area scan or a mask; the boundary segments already encode the selected tile set.
- Don't modify `LassoPolygon` (immutable, shared with `SelectionThread`).

## 3. Minimal C++ Usage Snippet

```cpp
LassoOutline::Scratch scratch;
LassoOutline::Compute(polygon, map_width, map_height, scratch);
// scratch.segments: tile-space grid boundary, drawn by LassoTileOutlineDrawer
```

## 4. Invariants & Pitfalls

- The outline marks the tiles the current-floor polygon selects (`IntersectsTile` closed-tile rule), exactly like the commit.
- In the compensated VisibleFloors+show-all-off configuration the commit applies its legacy pre-shift to both box and lasso equally (box-preview parity).
- `MapTileToScreenDIP` subtracts `GetFloorTileOffset(floor)`; for the current floor this coincides with the render-path offset (both are `(GROUND_LAYER - floor)` underground, both 0 above ground).
- The outline uses hardcoded white (shade alpha 0.2, dashes alpha 0.95, 1 px thickness) and a constant 3 px dash / 2 px gap, matching `SelectionDrawer`'s box selection.
- The preview is render-only: it never mutates `Tile`, `Selection`, `ActionQueue`, or OTBM, and never re-bakes a chunk VBO.
- A resize, DPI change, external view change, or selection-settings change invalidates the preview; never render a polygon built for another view.
- The lasso path is outline + rubber band + click-vertex handles; freehand drags show no handles.
