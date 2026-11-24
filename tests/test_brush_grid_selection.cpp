#include "palette/controls/brush_grid_selection.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <optional>
#include <vector>

// Brush is only used through pointers here; the test addresses stand in for brushes.
class Brush { };

namespace {

	using Modifier = BrushGridSelection::Modifier;

	struct Fixture {
		std::array<Brush, 6> storage {};
		std::vector<Brush*> display;
		BrushGridSelection selection;

		Fixture() {
			for (Brush& brush : storage) {
				display.push_back(&brush);
			}
		}

		Brush* at(int index) {
			return display[static_cast<size_t>(index)];
		}

		std::vector<Brush*> selected() const {
			return selection.Selected(display);
		}
	};

} // namespace

TEST_CASE("a plain click picks a single brush", "[brush_grid_selection]") {
	Fixture f;
	const auto result = f.selection.Press(f.display, 2, Modifier::None);
	REQUIRE(result.picked == f.at(2));
	REQUIRE(result.can_drag);
	REQUIRE(f.selection.Primary() == f.at(2));
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(2) });
}

TEST_CASE("Ctrl toggles members without changing the primary brush", "[brush_grid_selection]") {
	Fixture f;
	f.selection.Press(f.display, 1, Modifier::None);
	REQUIRE(f.selection.Press(f.display, 3, Modifier::Toggle).picked == nullptr);
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(1), f.at(3) });
	REQUIRE(f.selection.Primary() == f.at(1));

	// Toggling the primary off keeps it as the current brush.
	f.selection.Press(f.display, 1, Modifier::Toggle);
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(3) });
	REQUIRE(f.selection.Primary() == f.at(1));
}

TEST_CASE("Shift selects the range from the anchor", "[brush_grid_selection]") {
	Fixture f;
	f.selection.Press(f.display, 1, Modifier::None);
	REQUIRE(f.selection.Press(f.display, 4, Modifier::Range).picked == nullptr);
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(1), f.at(2), f.at(3), f.at(4) });

	// The anchor stays, so a second Shift click replaces the range.
	f.selection.Press(f.display, 0, Modifier::Range);
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(0), f.at(1) });
	REQUIRE(f.selection.Primary() == f.at(1));
}

TEST_CASE("Ctrl moves the anchor for the next Shift range", "[brush_grid_selection]") {
	Fixture f;
	f.selection.Press(f.display, 0, Modifier::None);
	f.selection.Press(f.display, 4, Modifier::Toggle);
	f.selection.Press(f.display, 5, Modifier::Range);
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(4), f.at(5) });
}

TEST_CASE("a click on a group member keeps the group until release", "[brush_grid_selection]") {
	Fixture f;
	f.selection.Press(f.display, 1, Modifier::None);
	f.selection.Press(f.display, 3, Modifier::Range);

	const auto press = f.selection.Press(f.display, 2, Modifier::None);
	REQUIRE(press.picked == nullptr);
	REQUIRE(press.can_drag);
	REQUIRE(f.selected().size() == 3);

	SECTION("a drag keeps the group") {
		REQUIRE(f.selection.Release(true) == nullptr);
		REQUIRE(f.selected().size() == 3);
	}
	SECTION("a release without a drag collapses to the clicked brush") {
		REQUIRE(f.selection.Release(false) == f.at(2));
		REQUIRE(f.selected() == std::vector<Brush*> { f.at(2) });
		REQUIRE(f.selection.Primary() == f.at(2));
	}
}

TEST_CASE("a click on empty space collapses to the primary brush", "[brush_grid_selection]") {
	Fixture f;
	f.selection.Press(f.display, 1, Modifier::None);
	f.selection.Press(f.display, 4, Modifier::Toggle);
	REQUIRE(f.selection.Press(f.display, std::nullopt, Modifier::None).picked == nullptr);
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(1) });

	// With a modifier, empty space changes nothing.
	f.selection.Press(f.display, 4, Modifier::Toggle);
	f.selection.Press(f.display, 99, Modifier::Toggle);
	REQUIRE(f.selected().size() == 2);
}

TEST_CASE("a right click outside the selection replaces it without a pick", "[brush_grid_selection]") {
	Fixture f;
	f.selection.Press(f.display, 1, Modifier::None);
	f.selection.Press(f.display, 2, Modifier::Toggle);

	f.selection.PressContext(f.display, 2);
	REQUIRE(f.selected().size() == 2);

	f.selection.PressContext(f.display, 5);
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(5) });
	REQUIRE(f.selection.Primary() == f.at(1));

	// Empty space (no cell, or past the last one) keeps the selection.
	f.selection.PressContext(f.display, std::nullopt);
	f.selection.PressContext(f.display, 99);
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(5) });
}

TEST_CASE("reset selects one brush or none", "[brush_grid_selection]") {
	Fixture f;
	f.selection.Press(f.display, 1, Modifier::None);
	f.selection.Press(f.display, 3, Modifier::Range);
	f.selection.Reset(f.at(5));
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(5) });
	REQUIRE(f.selection.Primary() == f.at(5));
	f.selection.Reset(nullptr);
	REQUIRE(f.selected().empty());
	REQUIRE(f.selection.Primary() == nullptr);
}

TEST_CASE("remap keeps members that are still displayed", "[brush_grid_selection]") {
	Fixture f;
	f.selection.Press(f.display, 1, Modifier::None);
	f.selection.Press(f.display, 4, Modifier::Range);

	// Filter: only brushes 0, 2 and 4 remain, in reverse order.
	std::vector<Brush*> filtered { f.at(4), f.at(2), f.at(0) };
	f.selection.Remap(filtered);
	REQUIRE(f.selection.Selected(filtered) == std::vector<Brush*> { f.at(4), f.at(2) });
	REQUIRE(f.selection.Primary() == f.at(1));

	// The anchor (brush 1) is hidden, so the next range starts at the clicked cell.
	f.selection.Press(filtered, 0, Modifier::Range);
	REQUIRE(f.selection.Selected(filtered) == std::vector<Brush*> { f.at(4) });
}

TEST_CASE("a pick committed on release keeps the primary until then", "[brush_grid_selection]") {
	Fixture f;
	f.selection.Press(f.display, 1, Modifier::None);

	const auto press = f.selection.Press(f.display, 4, Modifier::None, BrushGridSelection::Commit::OnRelease);
	REQUIRE(press.picked == f.at(4));
	REQUIRE(f.selected() == std::vector<Brush*> { f.at(4) });
	// A drag or a release elsewhere leaves the current brush alone.
	REQUIRE(f.selection.Primary() == f.at(1));

	// The caller commits the pick on release.
	f.selection.Reset(press.picked);
	REQUIRE(f.selection.Primary() == f.at(4));
}
