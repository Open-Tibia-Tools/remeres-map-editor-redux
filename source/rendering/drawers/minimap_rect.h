#ifndef RME_RENDERING_DRAWERS_MINIMAP_RECT_H_
#define RME_RENDERING_DRAWERS_MINIMAP_RECT_H_

#include <algorithm>

// Size of one minimap cache page in tiles. The page grid, the dirty-rect split and the texture
// storage all derive from this constant.
inline constexpr int kMinimapPageSize = 512;

// Tiles per map node edge. Matches SpatialHashGrid::NODE_SHIFT; minimap_cache.cpp asserts that.
inline constexpr int kMinimapNodeTiles = 4;

struct MinimapDirtyRect {
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

[[nodiscard]] inline bool IsValidMinimapRect(const MinimapDirtyRect& rect) {
	return rect.width > 0 && rect.height > 0;
}

[[nodiscard]] inline MinimapDirtyRect UnionMinimapRects(const MinimapDirtyRect& lhs, const MinimapDirtyRect& rhs) {
	if (!IsValidMinimapRect(lhs)) {
		return rhs;
	}
	if (!IsValidMinimapRect(rhs)) {
		return lhs;
	}

	const int min_x = std::min(lhs.x, rhs.x);
	const int min_y = std::min(lhs.y, rhs.y);
	const int max_x = std::max(lhs.x + lhs.width, rhs.x + rhs.width);
	const int max_y = std::max(lhs.y + lhs.height, rhs.y + rhs.height);
	return {
		.x = min_x,
		.y = min_y,
		.width = std::max(0, max_x - min_x),
		.height = std::max(0, max_y - min_y),
	};
}

// Map-space tile bounds of a page-local dirty rect. The end is exclusive, which is the contract of
// Map::visitLeaves(); a 1x1 rect therefore has end == min + 1.
//
// Preconditions: `page_rect` is already clamped to the page (see MinimapCache::clampRect) and
// `page_x` / `page_y` are non-negative. Empty rects are valid and yield end == min.
struct MinimapMapBounds {
	int min_x = 0;
	int min_y = 0;
	int end_x = 0;
	int end_y = 0;
};

[[nodiscard]] constexpr MinimapMapBounds ToMinimapMapBounds(const MinimapDirtyRect& page_rect, int page_x, int page_y) noexcept {
	const int min_x = page_x * kMinimapPageSize + page_rect.x;
	const int min_y = page_y * kMinimapPageSize + page_rect.y;
	return {
		.min_x = min_x,
		.min_y = min_y,
		.end_x = min_x + page_rect.width,
		.end_y = min_y + page_rect.height,
	};
}

#endif
