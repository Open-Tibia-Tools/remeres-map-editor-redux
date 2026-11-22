#include "editor/lasso_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <utility>
#include <vector>

namespace {

	using TileSet = std::set<std::pair<int, int>>;

	TileSet SelectedTiles(const LassoPolygon& polygon, const LassoTileBounds& bounds) {
		TileSet tiles;
		for (int y = bounds.min_y; y <= bounds.max_y; ++y) {
			for (int x = bounds.min_x; x <= bounds.max_x; ++x) {
				if (polygon.IntersectsTile(x, y)) {
					tiles.emplace(x, y);
				}
			}
		}
		return tiles;
	}

	// Reference tile test straight from the rule, without row buckets.
	bool ReferenceIntersectsTile(const LassoPolygon& polygon, int x, int y) {
		const LassoPoint corners[4] = { { double(x), double(y) }, { x + 1.0, double(y) }, { x + 1.0, y + 1.0 }, { double(x), y + 1.0 } };
		if (polygon.ContainsPoint({ x + 0.5, y + 0.5 })) {
			return true;
		}
		for (const LassoPoint& corner : corners) {
			if (polygon.ContainsPoint(corner)) {
				return true;
			}
		}
		const auto vertices = polygon.Vertices();
		for (std::size_t i = 0; i < vertices.size(); ++i) {
			const LassoPoint& a = vertices[i];
			const LassoPoint& b = vertices[(i + 1) % vertices.size()];
			if (LassoGeometry::PointInTile(a, x, y)) {
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

	const std::vector<LassoPoint> kSquare = { { 2.0, 2.0 }, { 6.0, 2.0 }, { 6.0, 6.0 }, { 2.0, 6.0 } };

} // namespace

TEST_CASE("point in polygon: convex square, boundary counts as inside", "[lasso][geometry]") {
	const LassoPolygon square(kSquare);
	REQUIRE(square.ContainsPoint({ 4.0, 4.0 }));
	REQUIRE(square.ContainsPoint({ 2.0, 4.0 })); // on edge
	REQUIRE(square.ContainsPoint({ 6.0, 6.0 })); // on vertex
	REQUIRE(square.ContainsPoint({ 4.0, 2.0 })); // on horizontal edge
	REQUIRE_FALSE(square.ContainsPoint({ 1.99, 4.0 }));
	REQUIRE_FALSE(square.ContainsPoint({ 7.0, 7.0 }));
}

TEST_CASE("point in polygon: concave U shape", "[lasso][geometry]") {
	// U opening upwards: notch between x 3..5 above y 3.
	const std::vector<LassoPoint> u = { { 1, 1 }, { 7, 1 }, { 7, 7 }, { 5, 7 }, { 5, 3 }, { 3, 3 }, { 3, 7 }, { 1, 7 } };
	const LassoPolygon polygon(u);
	REQUIRE(polygon.ContainsPoint({ 2.0, 5.0 }));
	REQUIRE(polygon.ContainsPoint({ 6.0, 5.0 }));
	REQUIRE(polygon.ContainsPoint({ 4.0, 2.0 }));
	REQUIRE_FALSE(polygon.ContainsPoint({ 4.0, 5.0 }));
	REQUIRE(polygon.ContainsPoint({ 4.0, 3.0 })); // notch floor is boundary
}

TEST_CASE("point in polygon: self-intersecting uses even-odd", "[lasso][geometry]") {
	// Pentagram: the inner pentagon is covered twice and therefore outside under even-odd.
	const std::vector<LassoPoint> star = { { 50, 0 }, { 79, 90 }, { 2, 35 }, { 98, 35 }, { 21, 90 } };
	const LassoPolygon polygon(star);
	REQUIRE_FALSE(polygon.ContainsPoint({ 50.0, 50.0 })); // inner pentagon
	REQUIRE(polygon.ContainsPoint({ 50.0, 15.0 })); // top spike
	REQUIRE_FALSE(polygon.ContainsPoint({ 5.0, 5.0 }));

	// Bow tie: both lobes inside, crossing point on the boundary.
	const std::vector<LassoPoint> bow_tie = { { 0, 0 }, { 4, 4 }, { 4, 0 }, { 0, 4 } };
	const LassoPolygon tie(bow_tie);
	REQUIRE(tie.ContainsPoint({ 0.5, 2.0 }));
	REQUIRE(tie.ContainsPoint({ 3.5, 2.0 }));
	REQUIRE(tie.ContainsPoint({ 2.0, 2.0 }));
	REQUIRE_FALSE(tie.ContainsPoint({ 2.0, 0.5 }));
}

TEST_CASE("tiles: square selects its covered tiles and edge-touching neighbours", "[lasso][geometry]") {
	const LassoPolygon square(kSquare);
	const auto bounds = ComputeLassoTileBounds(square, 100, 100);
	REQUIRE(bounds.has_value());
	const TileSet tiles = SelectedTiles(square, *bounds);
	// Closed squares: tiles 1..6 touch the boundary lines x = 2 / x = 6.
	for (int y = 1; y <= 6; ++y) {
		for (int x = 1; x <= 6; ++x) {
			REQUIRE(tiles.contains({ x, y }));
		}
	}
	REQUIRE(tiles.size() == 36);
	REQUIRE_FALSE(square.IntersectsTile(0, 0));
	REQUIRE_FALSE(square.IntersectsTile(7, 3));
}

TEST_CASE("tiles: tile fully inside and tile containing the whole polygon", "[lasso][geometry]") {
	const std::vector<LassoPoint> big = { { 0.25, 0.25 }, { 20.5, 0.25 }, { 20.5, 20.5 }, { 0.25, 20.5 } };
	REQUIRE(LassoPolygon(big).IntersectsTile(10, 10));

	const std::vector<LassoPoint> tiny = { { 5.25, 5.25 }, { 5.75, 5.25 }, { 5.5, 5.75 } };
	const LassoPolygon polygon(tiny);
	REQUIRE(polygon.IntersectsTile(5, 5));
	REQUIRE_FALSE(polygon.IntersectsTile(4, 5));
	REQUIRE_FALSE(polygon.IntersectsTile(6, 5));
}

TEST_CASE("tiles: thin polygon crossing a tile without a corner inside", "[lasso][geometry]") {
	// A sliver along y = 5.125 .. 5.25 through row 5: no tile corner or center lies inside it, and
	// tiles 2..8 contain no polygon vertex either, so only the edge crossings select them.
	const std::vector<LassoPoint> sliver = { { 1.25, 5.125 }, { 9.75, 5.125 }, { 9.75, 5.25 }, { 1.25, 5.25 } };
	const LassoPolygon polygon(sliver);
	REQUIRE_FALSE(polygon.ContainsPoint({ 5.5, 5.5 }));
	REQUIRE_FALSE(polygon.ContainsPoint({ 5.0, 5.0 }));
	REQUIRE_FALSE(polygon.ContainsPoint({ 6.0, 5.0 }));
	REQUIRE_FALSE(polygon.ContainsPoint({ 5.0, 6.0 }));
	REQUIRE_FALSE(polygon.ContainsPoint({ 6.0, 6.0 }));
	for (int x = 1; x <= 9; ++x) {
		REQUIRE(polygon.IntersectsTile(x, 5));
	}
	REQUIRE_FALSE(polygon.IntersectsTile(5, 4));
	REQUIRE_FALSE(polygon.IntersectsTile(5, 6));

	// Diagonal sliver crossing the tiles along the main diagonal.
	const std::vector<LassoPoint> diagonal = { { 0.5, 0.375 }, { 10.5, 10.375 }, { 10.5, 10.625 }, { 0.5, 0.625 } };
	const LassoPolygon diag(diagonal);
	REQUIRE(diag.IntersectsTile(5, 5));
	REQUIRE_FALSE(diag.IntersectsTile(5, 8));
}

TEST_CASE("tiles: edge-only touch and shared vertex count", "[lasso][geometry]") {
	const LassoPolygon square(kSquare);
	// Tile (1, 3) only touches the polygon along x = 2.
	REQUIRE(square.IntersectsTile(1, 3));
	// Tile (1, 1) only shares the vertex (2, 2).
	REQUIRE(square.IntersectsTile(1, 1));
	// Tile (0, 3) is one tile away.
	REQUIRE_FALSE(square.IntersectsTile(0, 3));
}

TEST_CASE("tiles: row buckets agree with the plain rule", "[lasso][geometry]") {
	const std::vector<std::vector<LassoPoint>> polygons = {
		kSquare,
		{ { 1, 1 }, { 7, 1 }, { 7, 7 }, { 5, 7 }, { 5, 3 }, { 3, 3 }, { 3, 7 }, { 1, 7 } },
		{ { 50, 0 }, { 79, 90 }, { 2, 35 }, { 98, 35 }, { 21, 90 } },
		{ { 0.5, 0.375 }, { 10.5, 10.375 }, { 10.5, 10.625 }, { 0.5, 0.625 } },
		{ { 3.03125, 7.96875 }, { 12.5, 1.0 }, { 18.25, 9.75 }, { 9.0, 4.0 }, { 4.5, 14.0 } },
	};
	for (const auto& vertices : polygons) {
		const LassoPolygon polygon(vertices);
		const auto bounds = ComputeLassoTileBounds(polygon, 200, 200);
		REQUIRE(bounds.has_value());
		for (int y = bounds->min_y; y <= bounds->max_y; ++y) {
			for (int x = bounds->min_x; x <= bounds->max_x; ++x) {
				INFO("tile " << x << "," << y);
				REQUIRE(polygon.IntersectsTile(x, y) == ReferenceIntersectsTile(polygon, x, y));
			}
		}
	}
}

TEST_CASE("degenerate polygons select nothing", "[lasso][geometry]") {
	REQUIRE_FALSE(LassoPolygon(std::vector<LassoPoint> {}).IntersectsTile(0, 0));
	const std::vector<LassoPoint> two = { { 0.5, 0.5 }, { 3.5, 3.5 } };
	const LassoPolygon segment(two);
	REQUIRE_FALSE(segment.IntersectsTile(1, 1));
	REQUIRE_FALSE(segment.ContainsPoint({ 1.0, 1.0 }));
	REQUIRE_FALSE(ComputeLassoTileBounds(segment, 100, 100).has_value());
}

TEST_CASE("tile bounds expand by one tile and clamp to the map", "[lasso][geometry]") {
	const LassoPolygon square(kSquare);
	const auto bounds = ComputeLassoTileBounds(square, 100, 100);
	REQUIRE(bounds.has_value());
	REQUIRE(bounds->min_x == 1);
	REQUIRE(bounds->min_y == 1);
	REQUIRE(bounds->max_x == 7);
	REQUIRE(bounds->max_y == 7);

	const std::vector<LassoPoint> negative = { { -5.5, -3.0 }, { 2.5, -3.0 }, { 2.5, 1.5 } };
	const auto clamped_low = ComputeLassoTileBounds(LassoPolygon(negative), 100, 100);
	REQUIRE(clamped_low.has_value());
	REQUIRE(clamped_low->min_x == 0);
	REQUIRE(clamped_low->min_y == 0);
	REQUIRE(clamped_low->max_x == 3);
	REQUIRE(clamped_low->max_y == 2);

	const std::vector<LassoPoint> high = { { 95.5, 95.5 }, { 130.0, 96.0 }, { 120.0, 140.0 } };
	const auto clamped_high = ComputeLassoTileBounds(LassoPolygon(high), 100, 100);
	REQUIRE(clamped_high.has_value());
	REQUIRE(clamped_high->min_x == 94);
	REQUIRE(clamped_high->max_x == 99);
	REQUIRE(clamped_high->max_y == 99);

	const std::vector<LassoPoint> outside = { { -20.0, -20.0 }, { -10.0, -20.0 }, { -10.0, -10.0 } };
	REQUIRE_FALSE(ComputeLassoTileBounds(LassoPolygon(outside), 100, 100).has_value());
	REQUIRE_FALSE(ComputeLassoTileBounds(square, 0, 100).has_value());
}

TEST_CASE("segment intersection is closed", "[lasso][geometry]") {
	using LassoGeometry::SegmentsIntersect;
	REQUIRE(SegmentsIntersect({ 0, 0 }, { 2, 2 }, { 0, 2 }, { 2, 0 }));
	REQUIRE(SegmentsIntersect({ 0, 0 }, { 1, 0 }, { 1, 0 }, { 1, 5 })); // shared endpoint
	REQUIRE(SegmentsIntersect({ 0, 0 }, { 3, 0 }, { 2, 0 }, { 5, 0 })); // collinear overlap
	REQUIRE_FALSE(SegmentsIntersect({ 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 }));
	REQUIRE_FALSE(SegmentsIntersect({ 0, 0 }, { 1, 1 }, { 0, 1 }, { 0.25, 0.875 }));
}
