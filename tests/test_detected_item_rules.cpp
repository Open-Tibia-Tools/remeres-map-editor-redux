#include "palette/detected/detected_item_rules.h"

#include <catch2/catch_test_macros.hpp>

namespace {
	DetectedItemFacts BorderPiece() {
		DetectedItemFacts item;
		item.server_id = 12000;
		item.client_id = 12000;
		item.ground_border_piece = true;
		return item;
	}
}

TEST_CASE("a ground border piece nothing refers to is an unregistered border", "[detected]") {
	CHECK(IsUnregisteredBorder(BorderPiece()));
}

TEST_CASE("registered, owned, ground, meta and non-border items are not unregistered borders", "[detected]") {
	auto registered = BorderPiece();
	registered.registered_border = true;
	CHECK_FALSE(IsUnregisteredBorder(registered));

	auto owned = BorderPiece();
	owned.brush_owned = true;
	CHECK_FALSE(IsUnregisteredBorder(owned));

	auto ground = BorderPiece();
	ground.ground = true;
	CHECK_FALSE(IsUnregisteredBorder(ground));

	auto meta = BorderPiece();
	meta.meta = true;
	CHECK_FALSE(IsUnregisteredBorder(meta));

	auto object = BorderPiece();
	object.ground_border_piece = false;
	CHECK_FALSE(IsUnregisteredBorder(object));
}

TEST_CASE("walkable grounds are classified by their ground brush borders", "[detected]") {
	DetectedItemFacts ground;
	ground.ground = true;
	CHECK(ClassifyGroundBorder(ground) == GroundBorderState::NoGroundBrush);

	ground.ground_brush_owned = true;
	CHECK(ClassifyGroundBorder(ground) == GroundBorderState::BrushWithoutBorder);

	ground.ground_brush_has_border = true;
	CHECK(ClassifyGroundBorder(ground) == GroundBorderState::Bordered);
}

TEST_CASE("unpassable grounds, meta items and non-grounds are not floors", "[detected]") {
	DetectedItemFacts water;
	water.ground = true;
	water.unpassable = true;
	CHECK(ClassifyGroundBorder(water) == GroundBorderState::NotAFloor);

	DetectedItemFacts meta;
	meta.ground = true;
	meta.meta = true;
	CHECK(ClassifyGroundBorder(meta) == GroundBorderState::NotAFloor);

	CHECK(ClassifyGroundBorder(DetectedItemFacts {}) == GroundBorderState::NotAFloor);
}
