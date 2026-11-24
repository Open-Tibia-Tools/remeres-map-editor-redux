#include "ui/autoborder_ui_state.h"

#include <catch2/catch_test_macros.hpp>

using rme::ui::AutoborderStatusLabel;
using rme::ui::AutoborderToggleStatusMessage;
using rme::ui::ShouldApplyPreviewBordersToDrawnTiles;
using rme::ui::ShouldCleanWallsInAutoborderPreview;
using rme::ui::ShouldRunAutoborderPreview;
using rme::ui::ShouldShowAutoborderControls;

TEST_CASE("Autoborder status bar label and toggle toast wording are unified", "[autoborder_state]") {
	REQUIRE(AutoborderStatusLabel(true) == "Autoborder: ON (A)");
	REQUIRE(AutoborderStatusLabel(false) == "Autoborder: off (A)");
	REQUIRE(AutoborderToggleStatusMessage(true) == "Autoborder enabled.");
	REQUIRE(AutoborderToggleStatusMessage(false) == "Autoborder disabled.");
}

TEST_CASE("Brush Options Autoborder controls follow brush support consistently", "[autoborder_state]") {
	REQUIRE(ShouldShowAutoborderControls(true));
	REQUIRE_FALSE(ShouldShowAutoborderControls(false));
}

TEST_CASE("Autoborder preview only runs when drawing with a supported non-doodad brush and both toggles on", "[autoborder_state]") {
	REQUIRE(ShouldRunAutoborderPreview(true, true, false, true, true));

	// Disabled if not in drawing mode
	REQUIRE_FALSE(ShouldRunAutoborderPreview(false, true, false, true, true));
	// Disabled if brush does not support autoborder
	REQUIRE_FALSE(ShouldRunAutoborderPreview(true, false, false, true, true));
	// Disabled for DoodadBrush (secondary_map is reserved for g_doodad_preview)
	REQUIRE_FALSE(ShouldRunAutoborderPreview(true, true, true, true, true));
	// Disabled if Autoborder (A) is off even when Preview Border is checked
	REQUIRE_FALSE(ShouldRunAutoborderPreview(true, true, false, false, true));
	// Disabled if Preview Border is unchecked even when Autoborder (A) is on
	REQUIRE_FALSE(ShouldRunAutoborderPreview(true, true, false, true, false));
}

TEST_CASE("Autoborder preview simulation distinguishes WallBrush from DoorBrush", "[autoborder_state]") {
	// WallBrush cleans old walls on tilestodraw and borders drawn tiles
	REQUIRE(ShouldCleanWallsInAutoborderPreview(true, false));
	REQUIRE(ShouldApplyPreviewBordersToDrawnTiles(false));

	// DoorBrush transforms existing walls on tilestodraw and only wallizes neighbors (tilestoborder)
	REQUIRE_FALSE(ShouldCleanWallsInAutoborderPreview(false, true));
	REQUIRE_FALSE(ShouldCleanWallsInAutoborderPreview(true, true));
	REQUIRE_FALSE(ShouldApplyPreviewBordersToDrawnTiles(true));
}
