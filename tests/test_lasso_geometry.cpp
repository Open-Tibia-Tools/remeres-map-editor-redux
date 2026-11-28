#include "editor/lasso_geometry.h"
#include "editor/lasso_tile_outline.h"
#include "rendering/core/map_view_math.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <random>
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

	// Reference point test straight from the even-odd rule, without row buckets.
	bool ReferenceContainsPoint(const LassoPolygon& polygon, const LassoPoint& p) {
		const auto vertices = polygon.Vertices();
		if (vertices.size() < 3) {
			return false;
		}
		bool inside = false;
		for (std::size_t i = 0; i < vertices.size(); ++i) {
			const LassoPoint& a = vertices[i];
			const LassoPoint& b = vertices[(i + 1) % vertices.size()];
			if (LassoGeometry::OnSegment(p, a, b)) {
				return true;
			}
			if ((a.y > p.y) != (b.y > p.y)) {
				const double crossing_x = a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y);
				if (p.x < crossing_x) {
					inside = !inside;
				}
			}
		}
		return inside;
	}

	// Reference tile test straight from the rule, without row buckets.
	bool ReferenceIntersectsTile(const LassoPolygon& polygon, int x, int y) {
		const LassoPoint corners[4] = { { double(x), double(y) }, { x + 1.0, double(y) }, { x + 1.0, y + 1.0 }, { double(x), y + 1.0 } };
		if (ReferenceContainsPoint(polygon, { x + 0.5, y + 0.5 })) {
			return true;
		}
		for (const LassoPoint& corner : corners) {
			if (ReferenceContainsPoint(polygon, corner)) {
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

	// Seeded random polygon with occasional duplicate vertices (degenerate edges).
	std::vector<LassoPoint> RandomPolygon(std::mt19937& rng, int vertex_count) {
		std::uniform_real_distribution<double> coord(0.0, 64.0);
		std::uniform_int_distribution<int> duplicate(0, 5);
		std::vector<LassoPoint> points;
		points.reserve(static_cast<std::size_t>(vertex_count));
		for (int i = 0; i < vertex_count; ++i) {
			if (!points.empty() && duplicate(rng) == 0) {
				points.push_back(points.back());
			} else {
				points.push_back({ coord(rng), coord(rng) });
			}
		}
		return points;
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

TEST_CASE("tile outline matches the brute-force boundary", "[lasso][outline]") {
	std::mt19937 rng(20261126);
	for (int iteration = 0; iteration < 80; ++iteration) {
		const std::vector<LassoPoint> points = RandomPolygon(rng, 3 + iteration % 10);
		const LassoPolygon polygon(points);
		LassoOutline::Scratch scratch;
		LassoOutline::Compute(polygon, 0, 0, scratch); // no clamp
		const auto& actual = scratch.segments;

		// Brute-force reference: iterate the polygon's tile bbox + 1, emit sides where
		// membership differs between neighbours.
		std::vector<LassoTileSegment> expected;
		const int min_x = static_cast<int>(std::floor(polygon.MinX())) - 2;
		const int max_x = static_cast<int>(std::ceil(polygon.MaxX())) + 2;
		const int min_y = static_cast<int>(std::floor(polygon.MinY())) - 2;
		const int max_y = static_cast<int>(std::ceil(polygon.MaxY())) + 2;
		for (int y = min_y; y <= max_y; ++y) {
			for (int x = min_x; x <= max_x; ++x) {
				if (!polygon.IntersectsTile(x, y)) {
					continue;
				}
				if (!polygon.IntersectsTile(x - 1, y)) {
					expected.push_back({ .x = x, .y = y, .horizontal = 0, .reversed = 1 });
				}
				if (!polygon.IntersectsTile(x + 1, y)) {
					expected.push_back({ .x = x + 1, .y = y, .horizontal = 0, .reversed = 0 });
				}
				if (!polygon.IntersectsTile(x, y - 1)) {
					expected.push_back({ .x = x, .y = y, .horizontal = 1, .reversed = 0 });
				}
				if (!polygon.IntersectsTile(x, y + 1)) {
					expected.push_back({ .x = x, .y = y + 1, .horizontal = 1, .reversed = 1 });
				}
			}
		}
		std::sort(expected.begin(), expected.end());
		std::vector<LassoTileSegment> sorted_actual(actual);
		std::sort(sorted_actual.begin(), sorted_actual.end());
		REQUIRE(sorted_actual == expected);
	}
}

TEST_CASE("tile outline degenerates and clamps", "[lasso][outline]") {
	const std::vector<LassoPoint> two = { { 2.0, 2.0 }, { 6.0, 2.0 } };
	const LassoPolygon degenerate(two);
	LassoOutline::Scratch scratch;
	LassoOutline::Compute(degenerate, 100, 100, scratch);
	REQUIRE(scratch.segments.empty());

	// A square at (2,2)-(6,6) clamped to a 4x4 map should still emit its sides.
	const LassoPolygon square(kSquare);
	LassoOutline::Compute(square, 4, 4, scratch);
	REQUIRE_FALSE(scratch.segments.empty());
	// Every segment must be within the clamped map bounds.
	for (const auto& s : scratch.segments) {
		if (s.horizontal == 0) {
			REQUIRE(s.x >= 0);
			REQUIRE(s.x <= 4);
			REQUIRE(s.y >= 0);
			REQUIRE(s.y < 4);
		} else {
			REQUIRE(s.x >= 0);
			REQUIRE(s.x < 4);
			REQUIRE(s.y >= 0);
			REQUIRE(s.y <= 4);
		}
	}
}

TEST_CASE("tile outline segments chain into closed loops", "[lasso][outline]") {
	// A polygon entirely inside tile (0, 0) selects exactly that tile: 4 boundary segments.
	const std::vector<LassoPoint> square = { { 0.1, 0.1 }, { 0.9, 0.1 }, { 0.9, 0.9 }, { 0.1, 0.9 } };
	const LassoPolygon polygon(square);
	LassoOutline::Scratch scratch;
	LassoOutline::Compute(polygon, 0, 0, scratch);
	REQUIRE(scratch.segments.size() == 4);

	// Verify each segment's direction: left=up(reversed), right=down(forward),
	// top=right(forward), bottom=left(reversed).
	uint8_t found[4] = {};
	for (const auto& seg : scratch.segments) {
		if (seg.horizontal == 0 && seg.x == 0) {
			REQUIRE(seg.reversed == 1); // left edge walks up
			found[0] = 1;
		} else if (seg.horizontal == 0 && seg.x == 1) {
			REQUIRE(seg.reversed == 0); // right edge walks down
			found[1] = 1;
		} else if (seg.horizontal == 1 && seg.y == 0) {
			REQUIRE(seg.reversed == 0); // top edge walks right
			found[2] = 1;
		} else if (seg.horizontal == 1 && seg.y == 1) {
			REQUIRE(seg.reversed == 1); // bottom edge walks left
			found[3] = 1;
		}
	}
	REQUIRE(found[0] == 1);
	REQUIRE(found[1] == 1);
	REQUIRE(found[2] == 1);
	REQUIRE(found[3] == 1);
}

TEST_CASE("MapTileToScreenDIP converts without ceil quantization", "[lasso][outline]") {
	MainMapViewport viewport;
	viewport.view_scroll_x = 100;
	viewport.view_scroll_y = 50;
	viewport.zoom = 2.0;
	viewport.scale_factor = 1.0;
	viewport.floor = 7; // GROUND_LAYER: offset 0

	double sx, sy;
	MainMapViewMath::MapTileToScreenDIP(5, 3, 7, viewport, &sx, &sy);
	// (5 * 32 - 100) / (2 * 1) = 60 / 2 = 30
	REQUIRE(sx == Catch::Approx(30.0));
	REQUIRE(sy == Catch::Approx((3 * 32 - 50) / 2.0));

	// Underground floor: GetFloorTileOffset(5) = 2, so tile 5 on floor 5 renders at tile (5-2)=3.
	viewport.floor = 5;
	MainMapViewMath::MapTileToScreenDIP(5, 3, 5, viewport, &sx, &sy);
	REQUIRE(sx == Catch::Approx((3 * 32 - 100) / 2.0));
	REQUIRE(sy == Catch::Approx((1 * 32 - 50) / 2.0));
}
