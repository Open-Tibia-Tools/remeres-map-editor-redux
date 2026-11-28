# GLSL Kitchen-Tile Overlay Indicators

## 1. Pattern & Key Files
- `rendering/indicators/technical_item_registry.h` — `TileIndicatorType` IDs and `GetIndicatorBadgeStyle` metadata.
- `rendering/indicators/zone_flags.h` — 5-bit `MakeIndicatorZoneFlag` / `GetIndicatorTypeFromZoneFlags` (`bits 25..29`, IDs `0..31`).
- `rendering/shaders/indicator_shader.h` — `INDICATOR_SHADER_GLSL` (`evaluateTileIndicator` + `sampleIndicatorBand` for 1- or 2-line pixel typography with 8-way black outline and kitchen-tile bevel).
- `rendering/shaders/sprite_batch_shader.h` & `rendering/shaders/chunk_shader.h` — shader dispatch (`SpriteBatch` reads 5-bit `vZoneFlags`; `ChunkShader` keeps 4-bit baked `flagBits >> 4`).
- `rendering/drawers/overlays/zone_overlay_drawer.{h,cpp}` — reusable row-buffered viewport sweeps (`CellCursor`, `RenderView::ComputeFloorTileBounds`) for neighbour-dependent overlays such as `drawFloorMissingGround`.
- `rendering/core/viewport_tile_bounds.h` & `rendering/core/render_view.{h,cpp}` — dependency-free `ComputeViewportTileBounds` and `RenderView::ComputeFloorTileBounds(map_z, extra_margin_tiles)` inverting floor-shifted screen origin (`base_screen_x/y`).

## 2. When to Use It
Do:
- Route per-tile framed/labeled map overlays through `TileIndicatorType` + `INDICATOR_SHADER_GLSL` instead of drawing flat tinted quads.
- Pass neutral `(1, 1, 1, 1)` vertex tint to `SpriteBatch::draw` and set `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` explicitly before emitting indicator quads.
- Use reusable `std::vector` row buffers + `SpatialHashGrid::CellCursor` when an overlay inspects orthogonal neighbours.
Don't:
- Don't bake neighbour-dependent overlays into 16×16 chunks without cross-chunk border invalidation; render them in `ZoneOverlayDrawer`.
- Don't pass a colored vertex tint to `evaluateTileIndicator` (it multiplies text and border RGB/alpha too).

## 3. Minimal Usage
```cpp
sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
const uint32_t flag = MakeIndicatorZoneFlag(
	TechnicalItemRegistry::GetIndicatorId(TileIndicatorType::MissingGround));
sprite_batch.draw(draw_x, draw_y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, flag);
```

## 4. Invariants & Pitfalls
- `SpriteBatch` indicator IDs occupy bits `25..29` (`0x1Fu`); bits `0..24` belong to zone/blocking/house flags, and `30..31` remain free.
- `ChunkShader` extracts only 4 bits (`1..11`); overlay-only IDs (`>= 12`, including `MissingGround = 18`, `MissingGroundRaw = 19`, and dual split `MissingGroundBoth = 20`) must pass `0` for their visibility uniform in `chunk_shader.h`.
- `uShowMissingGround` packs a 2-bit mask (`1` = `Show Missing Ground` rules, `2` = `Show Missing Ground (Raw: Any Items)` debug mode, `3` = dual split `"no" / "raw"`) so all three indicators stay shader-gated without extra uniforms.
- Keep `evaluateTileIndicator` parameter lists in sync across both `chunk_shader.h` and `sprite_batch_shader.h`.
- Upper-floor (`z < GROUND_LAYER`) `SpriteBatch` overlays emitting vertices at `x * TILE_SIZE + base_screen_x` must iterate `view.ComputeFloorTileBounds(z)` instead of `view.getBoundsForFloor(z)` so zoomed-in viewports do not clip tiles when `Show all Floors` is off.
