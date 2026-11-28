#include "rendering/ui/pick_candidate_order.h"
#include "rendering/ui/selection_geometry.h"
#include "rendering/core/item_render_rules.h"
#include "app/definitions.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <utility>

// Pixel-perfect picking visits candidate anchor tiles south-east of the clicked tile and converts the
// click into each candidate's local pixel space (pixel - offset * TILE_SIZE). These cases pin the
// geometry that lets a 2x2 ground (mountain 919: only the on-screen top-left part is opaque) be hit
// from the tile north-west of its anchor.

namespace {

	SpriteLayoutMetrics TwoByTwo() {
		SpriteLayoutMetrics metrics;
		metrics.num_columns = 2;
		metrics.num_rows = 2;
		metrics.column_widths[0] = TILE_SIZE;
		metrics.column_widths[1] = TILE_SIZE;
		metrics.row_heights[0] = TILE_SIZE;
		metrics.row_heights[1] = TILE_SIZE;
		metrics.total_width = 2 * TILE_SIZE;
		metrics.total_height = 2 * TILE_SIZE;
		return metrics;
	}

	// Local pixel of `clicked` (pixel inside the clicked tile) as seen from a candidate `dx`,`dy` tiles away.
	int ToCandidateLocal(int clicked, int offset_in_tiles) {
		return clicked - offset_in_tiles * TILE_SIZE;
	}

} // namespace

TEST_CASE("click on the tile north-west of a 2x2 anchor maps to its top-left part", "[selection][multitile]") {
	const SpriteLayoutMetrics metrics = TwoByTwo();

	// Clicked pixel (10, 12) on the clicked tile, anchor candidate at dx = 1, dy = 1.
	const auto part = FindSelectionSpritePart(ToCandidateLocal(10, 1), ToCandidateLocal(12, 1), 0, 0, metrics);
	REQUIRE(part);
	REQUIRE(part->column == 1);
	REQUIRE(part->row == 1);
	REQUIRE(part->local_x == 10);
	REQUIRE(part->local_y == 12);
}

TEST_CASE("click on the anchor tile itself maps to the anchor part", "[selection][multitile]") {
	const SpriteLayoutMetrics metrics = TwoByTwo();

	// The anchor part is transparent for 919, so the alpha test lets this click fall through.
	const auto part = FindSelectionSpritePart(5, 6, 0, 0, metrics);
	REQUIRE(part);
	REQUIRE(part->column == 0);
	REQUIRE(part->row == 0);
}

TEST_CASE("click north and west of the anchor maps to the side parts", "[selection][multitile]") {
	const SpriteLayoutMetrics metrics = TwoByTwo();

	// Clicked tile directly north of the anchor (dx = 0, dy = 1).
	const auto north = FindSelectionSpritePart(ToCandidateLocal(3, 0), ToCandidateLocal(4, 1), 0, 0, metrics);
	REQUIRE(north);
	REQUIRE(north->column == 0);
	REQUIRE(north->row == 1);

	// Clicked tile directly west of the anchor (dx = 1, dy = 0).
	const auto west = FindSelectionSpritePart(ToCandidateLocal(3, 1), ToCandidateLocal(4, 0), 0, 0, metrics);
	REQUIRE(west);
	REQUIRE(west->column == 1);
	REQUIRE(west->row == 0);
}

TEST_CASE("a 1x1 ground of a neighbouring candidate never covers the clicked tile", "[selection][multitile]") {
	SpriteLayoutMetrics metrics;
	metrics.num_columns = 1;
	metrics.num_rows = 1;
	metrics.column_widths[0] = TILE_SIZE;
	metrics.row_heights[0] = TILE_SIZE;

	for (int dy = 0; dy <= 4; ++dy) {
		for (int dx = 0; dx <= 4; ++dx) {
			if (dx == 0 && dy == 0) {
				continue;
			}
			REQUIRE_FALSE(FindSelectionSpritePart(ToCandidateLocal(0, dx), ToCandidateLocal(0, dy), 0, 0, metrics));
			REQUIRE_FALSE(FindSelectionSpritePart(ToCandidateLocal(TILE_SIZE - 1, dx), ToCandidateLocal(TILE_SIZE - 1, dy), 0, 0, metrics));
		}
	}
}

TEST_CASE("a 2x2 anchor two tiles away does not reach the clicked tile", "[selection][multitile]") {
	const SpriteLayoutMetrics metrics = TwoByTwo();
	REQUIRE_FALSE(FindSelectionSpritePart(ToCandidateLocal(31, 2), ToCandidateLocal(31, 2), 0, 0, metrics));
	REQUIRE_FALSE(FindSelectionSpritePart(ToCandidateLocal(0, 2), ToCandidateLocal(0, 1), 0, 0, metrics));
}

namespace {

	struct VisitedCandidate {
		int diagonal = 0;
		int dx = 0;
		int dy = 0;
	};

	std::vector<VisitedCandidate> VisitAll(int range) {
		std::vector<VisitedCandidate> visited;
		for (int diagonal = PickCandidateOrder::MaxDiagonal(range); diagonal >= 0; --diagonal) {
			PickCandidateOrder::ForEachOnDiagonal(range, diagonal, [&](int dx, int dy) {
				visited.push_back({ diagonal, dx, dy });
			});
		}
		return visited;
	}

} // namespace

TEST_CASE("pick candidates cover the whole search square exactly once", "[selection][pick_order]") {
	const auto visited = VisitAll(4);
	REQUIRE(visited.size() == 25);

	bool seen[5][5] = {};
	for (const VisitedCandidate& candidate : visited) {
		REQUIRE(candidate.dx >= 0);
		REQUIRE(candidate.dx <= 4);
		REQUIRE(candidate.dy >= 0);
		REQUIRE(candidate.dy <= 4);
		REQUIRE(candidate.dx + candidate.dy == candidate.diagonal);
		REQUIRE_FALSE(seen[candidate.dy][candidate.dx]);
		seen[candidate.dy][candidate.dx] = true;
	}
}

TEST_CASE("pick candidates are visited front to back by render diagonal", "[selection][pick_order]") {
	const auto visited = VisitAll(4);
	REQUIRE(visited.front().dx == 4);
	REQUIRE(visited.front().dy == 4);
	REQUIRE(visited.back().dx == 0);
	REQUIRE(visited.back().dy == 0);
	for (size_t i = 1; i < visited.size(); ++i) {
		REQUIRE(visited[i - 1].diagonal >= visited[i].diagonal);
	}
}

TEST_CASE("a wide item two tiles east shares the diagonal of a 2x2 ground anchor", "[selection][pick_order]") {
	// Mountain 919 anchored at (1, 1) and a wide item anchored at (2, 0) both sit on diagonal 2. The picker tests
	// every above-ground layer of a diagonal before its grounds, so the item (drawn over the mountain) wins.
	bool has_item_anchor = false;
	bool has_ground_anchor = false;
	PickCandidateOrder::ForEachOnDiagonal(4, 2, [&](int dx, int dy) {
		has_item_anchor = has_item_anchor || (dx == 2 && dy == 0);
		has_ground_anchor = has_ground_anchor || (dx == 1 && dy == 1);
	});
	REQUIRE(has_item_anchor);
	REQUIRE(has_ground_anchor);
}

TEST_CASE("a range of zero only visits the clicked tile", "[selection][pick_order]") {
	const auto visited = VisitAll(0);
	REQUIRE(visited.size() == 1);
	REQUIRE(visited.front().dx == 0);
	REQUIRE(visited.front().dy == 0);
}

TEST_CASE("within one diagonal candidates follow the chunk-bake painter order", "[selection][pick_order]") {
	std::vector<std::pair<int, int>> order;
	PickCandidateOrder::ForEachOnDiagonal(4, 2, [&](int dx, int dy) {
		order.emplace_back(dx, dy);
	});
	REQUIRE(order == std::vector<std::pair<int, int>> { { 2, 0 }, { 1, 1 }, { 0, 2 } });
}

TEST_CASE("common item depth steps are clamped", "[selection][render_rules]") {
	using rme::rendering::CommonItemElevationStep;

	REQUIRE(CommonItemElevationStep(-1) == 0);
	REQUIRE(CommonItemElevationStep(0) == 0);
	REQUIRE(CommonItemElevationStep(2) == 2);
	REQUIRE(CommonItemElevationStep(3) == 3);
	REQUIRE(CommonItemElevationStep(9) == 3);
}

TEST_CASE("item sublayers and elevation use the shared rules", "[selection][render_rules]")
{
    using rme::rendering::RenderSublayer;
    using rme::rendering::ResolveItemSublayer;

    REQUIRE(ResolveItemSublayer(true, true) == RenderSublayer::BottomItem);
    REQUIRE(ResolveItemSublayer(false, true) == RenderSublayer::TopItem);
    REQUIRE(ResolveItemSublayer(false, false) == RenderSublayer::CommonItem);

    int elevation = 0;
    rme::rendering::AdvanceItemElevation(elevation, 16, false);
    REQUIRE(elevation == 0);
    rme::rendering::AdvanceItemElevation(elevation, 16, true);
    REQUIRE(elevation == 16);
}
