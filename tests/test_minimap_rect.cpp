#include "rendering/drawers/minimap_rect.h"

#include <catch2/catch_test_macros.hpp>

// Map::visitLeaves(min_x, min_y, end_x, end_y) returns early when end <= min and visits the
// 4x4 map nodes from (min >> 2) to ((end - 1) >> 2). These cases pin the exclusive-end contract
// for the dirty rects MinimapCache uploads.

namespace {

	constexpr int kPageSize = 512;
	constexpr int kNodeShift = 2;

	bool VisitLeavesWouldRun(const MinimapMapBounds& bounds) {
		return bounds.end_x > bounds.min_x && bounds.end_y > bounds.min_y;
	}

	int LastVisitedNode(int end) {
		return (end - 1) >> kNodeShift;
	}

} // namespace

TEST_CASE("a 1x1 dirty rect maps to a non-empty exclusive range", "[minimap]") {
	const MinimapMapBounds bounds = ToMinimapMapBounds({ .x = 10, .y = 20, .width = 1, .height = 1 }, 0, 0, kPageSize);
	REQUIRE(bounds.min_x == 10);
	REQUIRE(bounds.min_y == 20);
	REQUIRE(bounds.end_x == 11);
	REQUIRE(bounds.end_y == 21);
	REQUIRE(VisitLeavesWouldRun(bounds));
	REQUIRE(LastVisitedNode(bounds.end_x) == (10 >> kNodeShift));
	REQUIRE(LastVisitedNode(bounds.end_y) == (20 >> kNodeShift));
}

TEST_CASE("a 1x1 dirty rect on the last tile of a node visits that node", "[minimap]") {
	const MinimapMapBounds bounds = ToMinimapMapBounds({ .x = 3, .y = 7, .width = 1, .height = 1 }, 0, 0, kPageSize);
	REQUIRE(VisitLeavesWouldRun(bounds));
	REQUIRE(LastVisitedNode(bounds.end_x) == 0);
	REQUIRE(LastVisitedNode(bounds.end_y) == 1);
}

TEST_CASE("a 1x1 dirty rect on the first tile of a node visits that node", "[minimap]") {
	const MinimapMapBounds bounds = ToMinimapMapBounds({ .x = 4, .y = 8, .width = 1, .height = 1 }, 0, 0, kPageSize);
	REQUIRE(VisitLeavesWouldRun(bounds));
	REQUIRE((bounds.min_x >> kNodeShift) == 1);
	REQUIRE(LastVisitedNode(bounds.end_x) == 1);
	REQUIRE(LastVisitedNode(bounds.end_y) == 2);
}

TEST_CASE("page offsets are applied to both ends", "[minimap]") {
	const MinimapMapBounds bounds = ToMinimapMapBounds({ .x = 511, .y = 0, .width = 1, .height = 1 }, 2, 3, kPageSize);
	REQUIRE(bounds.min_x == 2 * kPageSize + 511);
	REQUIRE(bounds.min_y == 3 * kPageSize);
	REQUIRE(bounds.end_x == bounds.min_x + 1);
	REQUIRE(bounds.end_y == bounds.min_y + 1);
	REQUIRE(VisitLeavesWouldRun(bounds));
}

TEST_CASE("a node-aligned rect reaches its last node and no further", "[minimap]") {
	const MinimapMapBounds bounds = ToMinimapMapBounds({ .x = 8, .y = 8, .width = 8, .height = 4 }, 0, 0, kPageSize);
	REQUIRE(bounds.end_x == 16);
	REQUIRE(bounds.end_y == 12);
	REQUIRE((bounds.min_x >> kNodeShift) == 2);
	REQUIRE(LastVisitedNode(bounds.end_x) == 3);
	REQUIRE(LastVisitedNode(bounds.end_y) == 2);
}

TEST_CASE("a rect spanning nodes includes the partially covered last node", "[minimap]") {
	const MinimapMapBounds bounds = ToMinimapMapBounds({ .x = 2, .y = 2, .width = 7, .height = 3 }, 0, 0, kPageSize);
	// Tiles 2..8 in x cover nodes 0, 1 and 2; tiles 2..4 in y cover nodes 0 and 1.
	REQUIRE(LastVisitedNode(bounds.end_x) == 2);
	REQUIRE(LastVisitedNode(bounds.end_y) == 1);
}

TEST_CASE("an empty rect maps to an empty range", "[minimap]") {
	const MinimapMapBounds bounds = ToMinimapMapBounds({ .x = 5, .y = 5, .width = 0, .height = 1 }, 0, 0, kPageSize);
	REQUIRE_FALSE(VisitLeavesWouldRun(bounds));
}
