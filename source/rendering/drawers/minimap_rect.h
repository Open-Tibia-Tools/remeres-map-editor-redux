#ifndef RME_RENDERING_DRAWERS_MINIMAP_RECT_H_
#define RME_RENDERING_DRAWERS_MINIMAP_RECT_H_

#include <algorithm>

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
struct MinimapMapBounds {
	int min_x = 0;
	int min_y = 0;
	int end_x = 0;
	int end_y = 0;
};

[[nodiscard]] inline MinimapMapBounds ToMinimapMapBounds(const MinimapDirtyRect& page_rect, int page_x, int page_y, int page_size) {
	const int min_x = page_x * page_size + page_rect.x;
	const int min_y = page_y * page_size + page_rect.y;
	return {
		.min_x = min_x,
		.min_y = min_y,
		.end_x = min_x + page_rect.width,
		.end_y = min_y + page_rect.height,
	};
}

#endif
