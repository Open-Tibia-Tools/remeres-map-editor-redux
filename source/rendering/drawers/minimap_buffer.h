#ifndef RME_RENDERING_DRAWERS_MINIMAP_BUFFER_H_
#define RME_RENDERING_DRAWERS_MINIMAP_BUFFER_H_

#include "rendering/drawers/minimap_rect.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

// Fills `out` with the minimap colors of every tile covered by `bounds`, in row-major order with a
// stride of `bounds.end_x - bounds.min_x`. Production callers must zero `out` first: tiles without a
// node stay untouched, which is what the texture upload reads back as "no color".
//
// `map` only needs the Map::visitLeaves(min_x, min_y, end_x, end_y, visitor) interface, including
// its exclusive end. `color_at(node, local_x, local_y)` returns the color of one tile inside `node`,
// with local coordinates in 0..kMinimapNodeTiles-1 on each axis.
//
// Keeping the traversal here instead of in MinimapCache means the bounds handed to visitLeaves()
// and the per-node clamps are one unit that can be tested without a map or an OpenGL context.
template <typename MapLike, typename NodeColorAt>
void FillMinimapColorBuffer(const MapLike& map, const MinimapMapBounds& bounds, std::span<uint8_t> out, NodeColorAt&& color_at) {
	const int width = bounds.end_x - bounds.min_x;
	const int height = bounds.end_y - bounds.min_y;
	if (width <= 0 || height <= 0) {
		return;
	}
	assert(out.size() >= static_cast<size_t>(width) * static_cast<size_t>(height));

	map.visitLeaves(bounds.min_x, bounds.min_y, bounds.end_x, bounds.end_y, [&](const auto* node, int node_map_x, int node_map_y) {
		const int start_local_x = std::max(0, bounds.min_x - node_map_x);
		const int end_local_x = std::min(kMinimapNodeTiles - 1, bounds.end_x - 1 - node_map_x);
		const int start_local_y = std::max(0, bounds.min_y - node_map_y);
		const int end_local_y = std::min(kMinimapNodeTiles - 1, bounds.end_y - 1 - node_map_y);

		for (int local_x = start_local_x; local_x <= end_local_x; ++local_x) {
			const size_t buffer_x = static_cast<size_t>(node_map_x + local_x - bounds.min_x);
			for (int local_y = start_local_y; local_y <= end_local_y; ++local_y) {
				const size_t buffer_y = static_cast<size_t>(node_map_y + local_y - bounds.min_y);
				out[buffer_y * static_cast<size_t>(width) + buffer_x] = color_at(node, local_x, local_y);
			}
		}
	});
}

#endif
