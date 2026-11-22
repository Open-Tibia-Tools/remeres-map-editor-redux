#include "ui/tile_properties/tile_properties_width.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <span>

using TilePropertiesWidth::SplitterMinWidth;
using TilePropertiesWidth::WidestSection;

TEST_CASE("the widest section sets the property column width", "[tile_properties]") {
	constexpr std::array<int, 4> widths { 180, 274, 0, 212 };
	STATIC_REQUIRE(WidestSection(widths) == 274);
}

TEST_CASE("an empty section does not lower the width", "[tile_properties]") {
	constexpr std::array<int, 3> widths { 0, 0, 96 };
	STATIC_REQUIRE(WidestSection(widths) == 96);
}

TEST_CASE("no sections means no width", "[tile_properties]") {
	STATIC_REQUIRE(WidestSection(std::span<const int> {}) == 0);
}

TEST_CASE("splitter width keeps both panes at their minimum", "[tile_properties]") {
	// One border on each side of the splitter, the sash between the panes.
	STATIC_REQUIRE(SplitterMinWidth(224, 274, 7, 2) == 2 + 224 + 7 + 274 + 2);
	STATIC_REQUIRE(SplitterMinWidth(224, 274, 7, 0) == 224 + 7 + 274);
}
