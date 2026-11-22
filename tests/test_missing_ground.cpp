#include "map/missing_ground.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>

using MissingGround::CountGroundedNeighbours;
using MissingGround::IsHole;

namespace {
	// '#' ground, 'w' item without ground, '.' empty. Rows top to bottom.
	template <size_t Rows>
	struct Grid {
		std::array<std::string_view, Rows> rows;

		[[nodiscard]] char at(int x, int y) const {
			if (y < 0 || y >= static_cast<int>(Rows) || x < 0 || x >= static_cast<int>(rows[y].size())) {
				return '.';
			}
			return rows[y][x];
		}

		[[nodiscard]] bool hole(int x, int y) const {
			const auto has_ground = [this](int nx, int ny) { return at(nx, ny) == '#'; };
			return IsHole(at(x, y) == 'w', at(x, y) == '#', CountGroundedNeighbours(x, y, has_ground));
		}
	};
} // namespace

TEST_CASE("only ungrounded tiles with items can be holes", "[missing_ground]") {
	REQUIRE(IsHole(true, false, 3));
	REQUIRE(IsHole(true, false, 4));
	REQUIRE_FALSE(IsHole(true, false, 2));
	REQUIRE_FALSE(IsHole(false, false, 4));
	REQUIRE_FALSE(IsHole(true, true, 4));
}

TEST_CASE("neighbours are the four orthogonal tiles", "[missing_ground]") {
	int asked = 0;
	const int count = CountGroundedNeighbours(5, 5, [&asked](int x, int y) {
		++asked;
		return (x == 5 && y == 4) || (x == 6 && y == 5) || (x == 4 && y == 4);
	});
	REQUIRE(asked == 4);
	REQUIRE(count == 2); // (4, 4) is diagonal and not counted
}

TEST_CASE("a forgotten tile inside a floor is reported", "[missing_ground]") {
	const Grid<3> grid { { "###", "#w#", "###" } };
	REQUIRE(grid.hole(1, 1));
}

TEST_CASE("a tile missing ground on one side of a floor edge is reported", "[missing_ground]") {
	const Grid<3> grid { { "###", "#w#", "..." } };
	REQUIRE(grid.hole(1, 1));
}

TEST_CASE("walls along an open edge or in open space are not reported", "[missing_ground]") {
	const Grid<3> edge { { "###", "www", "..." } };
	REQUIRE_FALSE(edge.hole(1, 1)); // 1 grounded neighbour
	const Grid<3> corner { { "##.", "#w.", "..." } };
	REQUIRE_FALSE(corner.hole(1, 1)); // 2 grounded neighbours
	const Grid<3> open { { "...", ".w.", "..." } };
	REQUIRE_FALSE(open.hole(1, 1));
}

TEST_CASE("empty tiles and grounded tiles are never reported", "[missing_ground]") {
	const Grid<3> grid { { "###", "#.#", "###" } };
	REQUIRE_FALSE(grid.hole(1, 1));
	REQUIRE_FALSE(grid.hole(0, 0));
}

TEST_CASE("neighbours outside the map count as having no ground", "[missing_ground]") {
	const Grid<2> grid { { "w#", "##" } };
	REQUIRE_FALSE(grid.hole(0, 0)); // only 2 neighbours exist inside the map
}
