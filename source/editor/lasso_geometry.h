//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_LASSO_GEOMETRY_H_
#define RME_EDITOR_LASSO_GEOMETRY_H_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// Tile-space geometry for the lasso selection.
//
// Tile (x, y) covers the closed square [x, x + 1] x [y, y + 1]. The polygon is closed implicitly
// (the last vertex connects to the first). Inside tests use the even-odd rule, so the overlapping
// part of a self-intersecting polygon is outside; points on an edge or a vertex count as inside.
//
// A tile is selected when its center or any corner is inside the polygon, any polygon vertex lies
// in the tile, or any polygon edge touches a tile edge. Corner-only tests would leave holes where a
// thin polygon crosses a tile without covering a corner.
//
// Canvas coordinates are multiples of 1 / TILE_SIZE, so the orientation tests are exact in double
// precision for map-sized values and the boundary checks below compare against zero exactly.

struct LassoPoint {
	double x = 0.0;
	double y = 0.0;
};

struct LassoTileBounds {
	int min_x = 0;
	int min_y = 0;
	int max_x = 0;
	int max_y = 0;
};

namespace LassoGeometry {

	// Twice the signed area of triangle (o, a, b); positive when b is left of o->a.
	[[nodiscard]] constexpr double Cross(const LassoPoint& o, const LassoPoint& a, const LassoPoint& b) noexcept {
		return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
	}

	[[nodiscard]] constexpr bool OnSegment(const LassoPoint& p, const LassoPoint& a, const LassoPoint& b) noexcept {
		return Cross(a, b, p) == 0.0
			&& p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x)
			&& p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
	}

	// Closed segment test: touching endpoints and collinear overlaps intersect.
	[[nodiscard]] constexpr bool SegmentsIntersect(const LassoPoint& a, const LassoPoint& b, const LassoPoint& c, const LassoPoint& d) noexcept {
		const double d1 = Cross(c, d, a);
		const double d2 = Cross(c, d, b);
		const double d3 = Cross(a, b, c);
		const double d4 = Cross(a, b, d);
		if (((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) && ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0))) {
			return true;
		}
		return (d1 == 0.0 && OnSegment(a, c, d))
			|| (d2 == 0.0 && OnSegment(b, c, d))
			|| (d3 == 0.0 && OnSegment(c, a, b))
			|| (d4 == 0.0 && OnSegment(d, a, b));
	}

	[[nodiscard]] constexpr bool PointInTile(const LassoPoint& p, int tile_x, int tile_y) noexcept {
		return p.x >= tile_x && p.x <= tile_x + 1.0 && p.y >= tile_y && p.y <= tile_y + 1.0;
	}

} // namespace LassoGeometry

// Immutable polygon shared read-only by selection worker threads.
// Edges are bucketed per tile row so a tile test only visits the edges that reach its row.
class LassoPolygon {
public:
	explicit LassoPolygon(std::span<const LassoPoint> polygon_vertices) :
		vertices(polygon_vertices.begin(), polygon_vertices.end()) {
		if (vertices.empty()) {
			return;
		}

		min_x = max_x = vertices.front().x;
		min_y = max_y = vertices.front().y;
		for (const LassoPoint& vertex : vertices) {
			min_x = std::min(min_x, vertex.x);
			max_x = std::max(max_x, vertex.x);
			min_y = std::min(min_y, vertex.y);
			max_y = std::max(max_y, vertex.y);
		}

		BuildRowBuckets();
	}

	[[nodiscard]] std::span<const LassoPoint> Vertices() const noexcept {
		return vertices;
	}
	[[nodiscard]] double MinX() const noexcept {
		return min_x;
	}
	[[nodiscard]] double MinY() const noexcept {
		return min_y;
	}
	[[nodiscard]] double MaxX() const noexcept {
		return max_x;
	}
	[[nodiscard]] double MaxY() const noexcept {
		return max_y;
	}

	// Even-odd test against every edge; boundary counts as inside.
	[[nodiscard]] bool ContainsPoint(const LassoPoint& p) const noexcept {
		if (vertices.size() < 3) {
			return false;
		}
		bool inside = false;
		for (std::size_t edge = 0; edge < vertices.size(); ++edge) {
			if (TestEdge(p, edge, inside)) {
				return true;
			}
		}
		return inside;
	}

	[[nodiscard]] bool IntersectsTile(int tile_x, int tile_y) const noexcept {
		if (vertices.size() < 3) {
			return false;
		}
		if (tile_x + 1.0 < min_x || tile_x > max_x || tile_y + 1.0 < min_y || tile_y > max_y) {
			return false;
		}

		const std::int64_t row_index = static_cast<std::int64_t>(tile_y) - row_origin;
		if (row_index < 0 || row_index + 1 >= static_cast<std::int64_t>(row_offsets.size())) {
			return false;
		}
		const auto row = static_cast<std::size_t>(row_index);
		const std::span<const std::uint32_t> edges(row_edges.data() + row_offsets[row], row_offsets[row + 1] - row_offsets[row]);
		if (edges.empty()) {
			return false;
		}

		const LassoPoint corners[4] = {
			{ static_cast<double>(tile_x), static_cast<double>(tile_y) },
			{ tile_x + 1.0, static_cast<double>(tile_y) },
			{ tile_x + 1.0, tile_y + 1.0 },
			{ static_cast<double>(tile_x), tile_y + 1.0 },
		};
		const LassoPoint center { tile_x + 0.5, tile_y + 0.5 };

		// Every edge that crosses or touches a point inside this row is in the row bucket,
		// so the even-odd test over the bucket equals the test over the whole polygon.
		if (ContainsPointInRow(center, edges)) {
			return true;
		}
		for (const LassoPoint& corner : corners) {
			if (ContainsPointInRow(corner, edges)) {
				return true;
			}
		}

		for (const std::uint32_t edge : edges) {
			const LassoPoint& a = vertices[edge];
			const LassoPoint& b = vertices[NextVertex(edge)];
			if (LassoGeometry::PointInTile(a, tile_x, tile_y) || LassoGeometry::PointInTile(b, tile_x, tile_y)) {
				return true;
			}
			for (int side = 0; side < 4; ++side) {
				if (LassoGeometry::SegmentsIntersect(a, b, corners[side], corners[(side + 1) % 4])) {
					return true;
				}
			}
		}
		return false;
	}

private:
	[[nodiscard]] std::size_t NextVertex(std::size_t index) const noexcept {
		return index + 1 == vertices.size() ? 0 : index + 1;
	}

	// Returns true when p lies on the edge; otherwise toggles `inside` when the edge crosses
	// the horizontal ray from p towards +x.
	[[nodiscard]] bool TestEdge(const LassoPoint& p, std::size_t edge, bool& inside) const noexcept {
		const LassoPoint& a = vertices[edge];
		const LassoPoint& b = vertices[NextVertex(edge)];
		if (LassoGeometry::OnSegment(p, a, b)) {
			return true;
		}
		if ((a.y > p.y) != (b.y > p.y)) {
			const double crossing_x = a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y);
			if (p.x < crossing_x) {
				inside = !inside;
			}
		}
		return false;
	}

	[[nodiscard]] bool ContainsPointInRow(const LassoPoint& p, std::span<const std::uint32_t> edges) const noexcept {
		bool inside = false;
		for (const std::uint32_t edge : edges) {
			if (TestEdge(p, edge, inside)) {
				return true;
			}
		}
		return inside;
	}

	// Tile row r covers [r, r + 1]; an edge spanning [y0, y1] reaches rows ceil(y0) - 1 .. floor(y1).
	[[nodiscard]] static std::int64_t FirstRow(double y0) noexcept {
		return static_cast<std::int64_t>(std::ceil(y0)) - 1;
	}
	[[nodiscard]] static std::int64_t LastRow(double y1) noexcept {
		return static_cast<std::int64_t>(std::floor(y1));
	}

	void BuildRowBuckets() {
		if (vertices.size() < 3) {
			return;
		}

		row_origin = FirstRow(min_y);
		const std::int64_t row_count = LastRow(max_y) - row_origin + 1;
		row_offsets.assign(static_cast<std::size_t>(row_count) + 1, 0);

		const auto for_each_edge_row = [this](auto&& visit) {
			for (std::size_t edge = 0; edge < vertices.size(); ++edge) {
				const LassoPoint& a = vertices[edge];
				const LassoPoint& b = vertices[NextVertex(edge)];
				const std::int64_t first = FirstRow(std::min(a.y, b.y)) - row_origin;
				const std::int64_t last = LastRow(std::max(a.y, b.y)) - row_origin;
				for (std::int64_t row = first; row <= last; ++row) {
					visit(static_cast<std::size_t>(row), static_cast<std::uint32_t>(edge));
				}
			}
		};

		for_each_edge_row([this](std::size_t row, std::uint32_t) {
			++row_offsets[row + 1];
		});
		for (std::size_t row = 1; row < row_offsets.size(); ++row) {
			row_offsets[row] += row_offsets[row - 1];
		}

		row_edges.resize(row_offsets.back());
		std::vector<std::uint32_t> fill(row_offsets.begin(), row_offsets.end() - 1);
		for_each_edge_row([this, &fill](std::size_t row, std::uint32_t edge) {
			row_edges[fill[row]++] = edge;
		});
	}

	std::vector<LassoPoint> vertices;
	double min_x = 0.0;
	double min_y = 0.0;
	double max_x = 0.0;
	double max_y = 0.0;

	std::int64_t row_origin = 0;
	std::vector<std::uint32_t> row_offsets;
	std::vector<std::uint32_t> row_edges;
};

// Candidate tiles: the polygon's tile bounding box expanded by one tile and clamped to the map.
// Returns nullopt when the polygon lies completely outside the map.
[[nodiscard]] inline std::optional<LassoTileBounds> ComputeLassoTileBounds(const LassoPolygon& polygon, int map_width, int map_height) noexcept {
	if (polygon.Vertices().size() < 3 || map_width <= 0 || map_height <= 0) {
		return std::nullopt;
	}

	const double min_x = std::floor(polygon.MinX()) - 1.0;
	const double min_y = std::floor(polygon.MinY()) - 1.0;
	const double max_x = std::floor(polygon.MaxX()) + 1.0;
	const double max_y = std::floor(polygon.MaxY()) + 1.0;

	const double clamped_min_x = std::max(min_x, 0.0);
	const double clamped_min_y = std::max(min_y, 0.0);
	const double clamped_max_x = std::min(max_x, static_cast<double>(map_width - 1));
	const double clamped_max_y = std::min(max_y, static_cast<double>(map_height - 1));
	if (clamped_min_x > clamped_max_x || clamped_min_y > clamped_max_y) {
		return std::nullopt;
	}

	return LassoTileBounds {
		.min_x = static_cast<int>(clamped_min_x),
		.min_y = static_cast<int>(clamped_min_y),
		.max_x = static_cast<int>(clamped_max_x),
		.max_y = static_cast<int>(clamped_max_y),
	};
}

#endif
