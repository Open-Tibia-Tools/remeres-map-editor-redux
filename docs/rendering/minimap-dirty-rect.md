# Minimap dirty-rect upload

## 1. Pattern & Key Files

The minimap keeps one `GL_R8UI` texture per 512x512 tile page and refreshes only dirty rects,
split into a testable decision and a thin GL apply:

- `minimap_rect.h` - dirty rects, map bounds, `kMinimapPageSize`, `kMinimapNodeTiles`.
- `minimap_buffer.h` - `FillMinimapColorBuffer()`, the pure traversal and buffer fill.
- `minimap_cache.cpp` - page bookkeeping, clamping and the GL upload.
- `tests/test_minimap_rect.cpp` - coverage tests with a sentinel-prefilled buffer.

The first three live under `source/rendering/drawers/`. `ToMinimapMapBounds()` converts a page-local
rect into map coordinates with an **exclusive** end, matching `Map::visitLeaves()`;
`FillMinimapColorBuffer()` is the only place that hands those bounds to the grid.

## 2. When to Use It

Do:
- Route every refresh through `ToMinimapMapBounds()` + `FillMinimapColorBuffer()`.
- Zero the buffer first; a tile without a node is uploaded as color index 0.
- Use a test sentinel the color function can never produce.

Don't:
- Don't hand an inclusive end to `visitLeaves()`; a 1x1 rect early-returns and uploads black.
- Don't add GL calls to the fill helper; it must stay callable from Catch2.

## 3. Minimal Usage

```cpp
upload_buffer_.assign(static_cast<size_t>(clamped.width) * clamped.height, 0);
const MinimapMapBounds bounds = ToMinimapMapBounds(clamped, page.page_x, page.page_y);
FillMinimapColorBuffer(map, bounds, upload_buffer_, [&](const MapNode* node, int local_x, int local_y) -> uint8_t {
	const Floor* node_floor = node->getFloor(floor);
	if (!node_floor) {
		return 0;
	}
	const Tile* tile = node_floor->locs[static_cast<size_t>(local_x * kMinimapNodeTiles + local_y)].get();
	return tile ? tile->getMiniMapColor() : 0;
});
```

## 4. Invariants & Pitfalls

- `MinimapMapBounds::end_*` is exclusive; `end - min` is the rect size and the row stride.
- `MinimapDirtyRect` is page-local and must be clamped to the page before conversion.
- `kMinimapNodeTiles` must match `SpatialHashGrid::NODE_SHIFT`; `minimap_cache.cpp` static_asserts it.
- The buffer must hold at least `width * height` bytes; the fill asserts this in debug builds.
- Unvisited slots stay untouched, so production callers must zero the buffer first.
