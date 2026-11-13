#include "ui/managers/layout_manager.h"

#include <catch2/catch_test_macros.hpp>

using rme::ui::layout::FitPaneInDisplay;
using rme::ui::layout::PaneRect;

TEST_CASE("pane inside the display is unchanged", "[pane]") {
	const auto fitted = FitPaneInDisplay({ 100, 100, 300, 200 }, { 0, 0, 1920, 1080 });
	REQUIRE(fitted.has_value());
	REQUIRE(fitted->x == 100);
	REQUIRE(fitted->y == 100);
	REQUIRE(fitted->width == 300);
	REQUIRE(fitted->height == 200);
}

TEST_CASE("pane overhanging the right and bottom edges is clamped", "[pane]") {
	const auto fitted = FitPaneInDisplay({ 1800, 1000, 300, 200 }, { 0, 0, 1920, 1080 });
	REQUIRE(fitted.has_value());
	REQUIRE(fitted->x == 1620);
	REQUIRE(fitted->y == 880);
}

TEST_CASE("pane larger than the display is rejected", "[pane]") {
	REQUIRE_FALSE(FitPaneInDisplay({ 0, 0, 2000, 1100 }, { 0, 0, 1920, 1080 }).has_value());
}

TEST_CASE("display with a negative origin is handled", "[pane]") {
	const auto fitted = FitPaneInDisplay({ -2000, 100, 300, 200 }, { -1920, 0, 1920, 1080 });
	REQUIRE(fitted.has_value());
	REQUIRE(fitted->x == -1920);
	REQUIRE(fitted->y == 100);
}
