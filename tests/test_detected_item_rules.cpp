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
