#include "map/tile_stack_rows.h"

#include <catch2/catch_test_macros.hpp>

using TileStackRows::CanMoveDown;
using TileStackRows::CanMoveUp;
using TileStackRows::FallbackRowAfterDelete;
using TileStackRows::Resolve;
using TileStackRows::RowCount;

TEST_CASE("ground is row 0 and items follow in tile order", "[tile_stack_rows]") {
	REQUIRE(RowCount(true, 3) == 4);
	REQUIRE(RowCount(false, 3) == 3);

	const auto ground = Resolve(0, true, 3);
	REQUIRE(ground);
	REQUIRE(ground->is_ground);

	const auto first_item = Resolve(1, true, 3);
	REQUIRE(first_item);
	REQUIRE_FALSE(first_item->is_ground);
	REQUIRE(first_item->item_index == 0);

	const auto last_item = Resolve(3, true, 3);
	REQUIRE(last_item);
	REQUIRE(last_item->item_index == 2);
}

TEST_CASE("without ground rows map directly to items", "[tile_stack_rows]") {
	const auto row = Resolve(0, false, 2);
	REQUIRE(row);
	REQUIRE_FALSE(row->is_ground);
	REQUIRE(row->item_index == 0);
	REQUIRE(Resolve(1, false, 2)->item_index == 1);
}

TEST_CASE("out of range rows do not resolve", "[tile_stack_rows]") {
	REQUIRE_FALSE(Resolve(-1, true, 3));
	REQUIRE_FALSE(Resolve(4, true, 3));
	REQUIRE_FALSE(Resolve(0, false, 0));

	const auto ground_only = Resolve(0, true, 0);
	REQUIRE(ground_only);
	REQUIRE(ground_only->is_ground);
}

TEST_CASE("ground never moves and items never move below it", "[tile_stack_rows]") {
	// ground + 3 items: rows 0 (ground), 1, 2, 3
	REQUIRE_FALSE(CanMoveUp(0, true, 3));
	REQUIRE_FALSE(CanMoveDown(0, true, 3));

	REQUIRE_FALSE(CanMoveUp(1, true, 3));
	REQUIRE(CanMoveDown(1, true, 3));

	REQUIRE(CanMoveUp(2, true, 3));
	REQUIRE(CanMoveDown(2, true, 3));

	REQUIRE(CanMoveUp(3, true, 3));
	REQUIRE_FALSE(CanMoveDown(3, true, 3));

	REQUIRE_FALSE(CanMoveUp(-1, true, 3));
	REQUIRE_FALSE(CanMoveDown(-1, true, 3));
}

TEST_CASE("moves without ground use the whole list", "[tile_stack_rows]") {
	REQUIRE_FALSE(CanMoveUp(0, false, 2));
	REQUIRE(CanMoveDown(0, false, 2));
	REQUIRE(CanMoveUp(1, false, 2));
	REQUIRE_FALSE(CanMoveDown(1, false, 2));

	REQUIRE_FALSE(CanMoveUp(0, false, 1));
	REQUIRE_FALSE(CanMoveDown(0, false, 1));
}

TEST_CASE("selection falls back to the previous row after delete", "[tile_stack_rows]") {
	// deleting the ground (row 0) from ground + 3 items selects the first item
	REQUIRE(FallbackRowAfterDelete(0, 3) == 0);
	// deleting a middle row selects the row above it
	REQUIRE(FallbackRowAfterDelete(2, 3) == 1);
	// deleting the last row selects the new last row
	REQUIRE(FallbackRowAfterDelete(3, 3) == 2);
	// nothing left to select
	REQUIRE_FALSE(FallbackRowAfterDelete(0, 0));
	REQUIRE_FALSE(FallbackRowAfterDelete(5, 0));
	// stale row larger than the remaining list is clamped
	REQUIRE(FallbackRowAfterDelete(10, 2) == 1);
}
