#include "brushes/creature/creature_placement.h"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using CreaturePlacement::Check;
using CreaturePlacement::Describe;
using CreaturePlacement::Input;
using CreaturePlacement::Result;

namespace {

	// A plain walkable tile outside every spawn, with the default "Place spawn with creature" setting on.
	Input PlainTile() {
		Input input;
		input.has_creature_type = true;
		input.has_tile = true;
		input.auto_create_spawn = true;
		return input;
	}

} // namespace

TEST_CASE("plain tile with the default setting is allowed (a spawn will be created)", "[creature_placement]") {
	REQUIRE(Check(PlainTile()) == Result::Allowed);
}

TEST_CASE("inside an existing spawn placement does not depend on the setting", "[creature_placement]") {
	Input input = PlainTile();
	input.inside_spawn = true;
	input.auto_create_spawn = false;
	REQUIRE(Check(input) == Result::Allowed);
}

TEST_CASE("explicit opt-out outside every spawn is rejected with a reason", "[creature_placement]") {
	Input input = PlainTile();
	input.auto_create_spawn = false;
	REQUIRE(Check(input) == Result::NoSpawn);
	REQUIRE_FALSE(std::string_view(Describe(Result::NoSpawn)).empty());
}

TEST_CASE("monsters are rejected in protection zones, NPCs are not", "[creature_placement]") {
	Input monster = PlainTile();
	monster.protection_zone = true;
	REQUIRE(Check(monster) == Result::MonsterInProtectionZone);

	Input npc = monster;
	npc.is_npc = true;
	REQUIRE(Check(npc) == Result::Allowed);
}

TEST_CASE("missing creature type, missing tile and blocking tiles are rejected", "[creature_placement]") {
	Input no_type = PlainTile();
	no_type.has_creature_type = false;
	REQUIRE(Check(no_type) == Result::NoCreatureType);

	Input no_tile = PlainTile();
	no_tile.has_tile = false;
	REQUIRE(Check(no_tile) == Result::NoTile);

	Input blocking = PlainTile();
	blocking.tile_blocking = true;
	blocking.inside_spawn = true;
	REQUIRE(Check(blocking) == Result::TileBlocking);
}

TEST_CASE("every rejection has a status text and Allowed has none", "[creature_placement]") {
	REQUIRE(std::string_view(Describe(Result::Allowed)).empty());
	for (const Result result : { Result::NoCreatureType, Result::NoTile, Result::TileBlocking, Result::NoSpawn, Result::MonsterInProtectionZone }) {
		REQUIRE_FALSE(std::string_view(Describe(result)).empty());
	}
}
