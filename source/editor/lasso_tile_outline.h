//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_LASSO_TILE_OUTLINE_H_
#define RME_EDITOR_LASSO_TILE_OUTLINE_H_

#include "editor/lasso_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// One grid segment of the tile-set boundary. A vertical segment (horizontal == 0) runs from
// (x, y) to (x, y + 1), separating tile (x - 1, y) from tile (x, y). A horizontal segment
// (horizontal != 0) runs from (x, y) to (x + 1, y), separating tile (x, y - 1) from tile (x, y).
struct LassoTileSegment {
	int x = 0;
	int y = 0;
	// 0: vertical segment, 1: horizontal segment.
	uint8_t horizontal = 0;
	// 0: forward (down/right), 1: reversed (up/left). Determines the traversal direction
	// for chaining into a closed fill path.
	uint8_t reversed = 0;

	friend bool operator==(const LassoTileSegment&, const LassoTileSegment&) = default;
	friend bool operator<(const LassoTileSegment& lhs, const LassoTileSegment& rhs) noexcept {
		if (lhs.x != rhs.x) {
			return lhs.x < rhs.x;
		}
		if (lhs.y != rhs.y) {
			return lhs.y < rhs.y;
		}
		if (lhs.horizontal != rhs.horizontal) {
			return lhs.horizontal < rhs.horizontal;
		}
		return lhs.reversed < rhs.reversed;
	}
};

// Computes the boundary segments of the set of tiles selected by a lasso polygon on one floor.
// Cost is O(perimeter): candidate tiles are collected in a one-tile band around each edge,
// and only their sides against unselected neighbours are emitted. The result is identical to
// a brute-force scan over every tile in the polygon's bounding box (property-tested).
namespace LassoOutline {

	// Scratch buffers reused across calls; the caller owns them so the hot path stays
	// allocation-free after the first call.
	struct Scratch {
		std::vector<LassoTileSegment> segments;
		std::unordered_set<uint64_t> candidates;
		std::unordered_map<uint64_t, bool> membership;

		void clear() {
			segments.clear();
			candidates.clear();
			membership.clear();
		}
	};

	[[nodiscard]] inline uint64_t PackTile(int x, int y) noexcept {
		return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
	}

	// Collects the one-tile band of candidates around one edge within one row.
	inline void CollectRowBand(const LassoPoint& a, const LassoPoint& b, double band_lo, std::unordered_set<uint64_t>& candidates) {
		const double band_hi = band_lo + 1.0;
		const double lo_y = std::min(a.y, b.y);
		const double hi_y = std::max(a.y, b.y);
		const double y_start = std::max(lo_y, band_lo);
		const double y_end = std::min(hi_y, band_hi);
		if (y_start > y_end) {
			return;
		}
		double x_min;
		double x_max;
		if (a.y == b.y) {
			// Horizontal edge: the x-range is the full edge span.
			x_min = std::min(a.x, b.x);
			x_max = std::max(a.x, b.x);
		} else {
			// Clip the edge to the y-band and take the x-range at the two clip boundaries.
			const double t_start = (y_start - a.y) / (b.y - a.y);
			const double t_end = (y_end - a.y) / (b.y - a.y);
			const double x_start = a.x + t_start * (b.x - a.x);
			const double x_end = a.x + t_end * (b.x - a.x);
			x_min = std::min(x_start, x_end);
			x_max = std::max(x_start, x_end);
		}
		// Conservative integer dilation: one tile on each side (closed-tile semantics).
		const int tile_lo = static_cast<int>(std::floor(x_min)) - 1;
		const int tile_hi = static_cast<int>(std::floor(x_max)) + 1;
		const int row = static_cast<int>(band_lo);
		for (int tx = tile_lo; tx <= tile_hi; ++tx) {
			candidates.insert(PackTile(tx, row));
		}
	}

	// Memoized tile membership.
	[[nodiscard]] inline bool IsSelected(const LassoPolygon& polygon, int x, int y, std::unordered_map<uint64_t, bool>& membership) {
		const uint64_t key = PackTile(x, y);
		const auto it = membership.find(key);
		if (it != membership.end()) {
			return it->second;
		}
		const bool selected = polygon.IntersectsTile(x, y);
		membership.emplace(key, selected);
		return selected;
	}

	// Returns the boundary of {tiles : polygon.IntersectsTile(t)} clamped to the map.
	// map_width/map_height of 0 disable the clamp (test-only).
	inline void Compute(const LassoPolygon& polygon, int map_width, int map_height, Scratch& scratch) {
		scratch.clear();
		if (polygon.Vertices().size() < 3) {
			return;
		}
		const auto vertices = polygon.Vertices();
		const std::size_t edge_count = vertices.size();

		// Collect candidate tiles: one-tile band around each edge, per row.
		for (std::size_t edge = 0; edge < edge_count; ++edge) {
			const LassoPoint& a = vertices[edge];
			const LassoPoint& b = vertices[(edge + 1) % edge_count];
			const double lo_y = std::min(a.y, b.y);
			const double hi_y = std::max(a.y, b.y);
			const double first_row = std::ceil(lo_y) - 1.0;
			const double last_row = std::floor(hi_y);
			for (double row = first_row; row <= last_row; ++row) {
				CollectRowBand(a, b, row, scratch.candidates);
			}
		}

		// Emit boundary sides: a selected tile's side against an unselected neighbour.
		// `reversed` encodes the traversal direction for chaining into closed fill loops:
		// left edge walks up, right edge walks down, top edge walks right, bottom edge walks left.
		for (const uint64_t key : scratch.candidates) {
			const int tx = static_cast<int>(static_cast<uint32_t>(key >> 32));
			const int ty = static_cast<int>(static_cast<uint32_t>(key));
			if (!IsSelected(polygon, tx, ty, scratch.membership)) {
				continue;
			}
			// Left neighbour unselected: vertical at x = tx, walk UP (reversed).
			if (!IsSelected(polygon, tx - 1, ty, scratch.membership)) {
				scratch.segments.push_back({ .x = tx, .y = ty, .horizontal = 0, .reversed = 1 });
			}
			// Right neighbour unselected: vertical at x = tx + 1, walk DOWN (forward).
			if (!IsSelected(polygon, tx + 1, ty, scratch.membership)) {
				scratch.segments.push_back({ .x = tx + 1, .y = ty, .horizontal = 0, .reversed = 0 });
			}
			// Top neighbour unselected: horizontal at y = ty, walk RIGHT (forward).
			if (!IsSelected(polygon, tx, ty - 1, scratch.membership)) {
				scratch.segments.push_back({ .x = tx, .y = ty, .horizontal = 1, .reversed = 0 });
			}
			// Bottom neighbour unselected: horizontal at y = ty + 1, walk LEFT (reversed).
			if (!IsSelected(polygon, tx, ty + 1, scratch.membership)) {
				scratch.segments.push_back({ .x = tx, .y = ty + 1, .horizontal = 1, .reversed = 1 });
			}
		}

		// Clamp to the map: a vertical segment at x = k separates tiles k-1 and k, so it is
		// valid when 0 <= k <= map_width. A horizontal segment at y = k separates tiles y=k-1
		// and y=k, so it is valid when 0 <= k <= map_height.
		if (map_width > 0 && map_height > 0) {
			scratch.segments.erase(
				std::remove_if(scratch.segments.begin(), scratch.segments.end(), [&](const LassoTileSegment& s) {
					if (s.horizontal == 0) {
						return s.x < 0 || s.x > map_width || s.y < 0 || s.y >= map_height;
					}
					return s.x < 0 || s.x >= map_width || s.y < 0 || s.y > map_height;
				}),
				scratch.segments.end());
		}
	}

} // namespace LassoOutline

#endif
