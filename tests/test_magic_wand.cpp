#include "editor/connected_positions.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <tuple>

// Geometry behind the magic wand: which positions a click floods into and which ring
// around a ground patch can hold its borders. Membership comes from a predicate, so these
// cases use small hand-drawn shapes instead of a map.

namespace {

	using Key = std::tuple<int, int, int>;

	Key KeyOf(const Position& position) {
		return { position.x, position.y, position.z };
	}

	std::set<Key> KeysOf(const PositionVector& positions) {
		std::set<Key> keys;
		for (const Position& position : positions) {
			keys.insert(KeyOf(position));
		}
		return keys;
	}

	auto InSet(const std::set<Key>& members) {
		return [&members](const Position& position) { return members.contains(KeyOf(position)); };
	}

	auto Always() {
		return [](size_t) { return true; };
	}

} // namespace

TEST_CASE("flood takes diagonal members too", "[magic_wand]") {
	// (12,12) only touches (11,11) diagonally, and diagonals count as neighbours.
	const std::set<Key> members { { 10, 10, 7 }, { 11, 10, 7 }, { 11, 11, 7 }, { 12, 12, 7 } };
	const auto found = ConnectedPositions::collect(Position(10, 10, 7), InSet(members), Always());
	REQUIRE(found);
	REQUIRE(KeysOf(*found) == members);
}

TEST_CASE("flood fills a whole checkerboard", "[magic_wand]") {
	// Same-ground tiles on a checkerboard only ever touch diagonally; the wand must still
	// select all of them as one patch.
	std::set<Key> members;
	for (int y = 0; y < 4; ++y) {
		for (int x = 0; x < 4; ++x) {
			if ((x + y) % 2 == 0) {
				members.insert({ 100 + x, 100 + y, 7 });
			}
		}
	}
	REQUIRE(members.size() == 8);
	const auto found = ConnectedPositions::collect(Position(100, 100, 7), InSet(members), Always());
	REQUIRE(found);
	REQUIRE(KeysOf(*found) == members);
}

TEST_CASE("flood stops at a gap", "[magic_wand]") {
	// (12,10) is missing, so (13,10) is not reached.
	const std::set<Key> members { { 10, 10, 7 }, { 11, 10, 7 }, { 13, 10, 7 } };
	const auto found = ConnectedPositions::collect(Position(10, 10, 7), InSet(members), Always());
	REQUIRE(found);
	REQUIRE(found->size() == 2);
}

TEST_CASE("flood stays on the origin floor", "[magic_wand]") {
	const std::set<Key> members { { 10, 10, 7 }, { 10, 10, 6 }, { 10, 10, 8 }, { 11, 10, 7 } };
	const auto found = ConnectedPositions::collect(Position(10, 10, 7), InSet(members), Always());
	REQUIRE(found);
	REQUIRE(KeysOf(*found) == std::set<Key> { { 10, 10, 7 }, { 11, 10, 7 } });
}

TEST_CASE("flood from a non-member origin is empty", "[magic_wand]") {
	const std::set<Key> members { { 11, 10, 7 } };
	const auto found = ConnectedPositions::collect(Position(10, 10, 7), InSet(members), Always());
	REQUIRE(found);
	REQUIRE(found->empty());
}

TEST_CASE("flood from an invalid origin is empty", "[magic_wand]") {
	const auto found = ConnectedPositions::collect(Position(-1, 10, 7), [](const Position&) { return true; }, Always());
	REQUIRE(found);
	REQUIRE(found->empty());
}

TEST_CASE("flood visits every member once", "[magic_wand]") {
	// A 3x3 block has several paths to each tile; none may be listed twice.
	std::set<Key> members;
	for (int y = 0; y < 3; ++y) {
		for (int x = 0; x < 3; ++x) {
			members.insert({ 100 + x, 100 + y, 7 });
		}
	}
	const auto found = ConnectedPositions::collect(Position(101, 101, 7), InSet(members), Always());
	REQUIRE(found);
	REQUIRE(found->size() == 9);
	REQUIRE(KeysOf(*found) == members);
}

TEST_CASE("flood cancels when keep_going refuses", "[magic_wand]") {
	std::set<Key> members;
	for (int x = 0; x < 10; ++x) {
		members.insert({ 100 + x, 100, 7 });
	}
	size_t calls = 0;
	const auto found = ConnectedPositions::collect(Position(100, 100, 7), InSet(members), [&calls](size_t found_count) {
		++calls;
		return found_count < 3;
	});
	REQUIRE_FALSE(found);
	REQUIRE(calls == 3);
}

TEST_CASE("border ring of one tile is its eight neighbours", "[magic_wand]") {
	const PositionVector patch { Position(10, 10, 7) };
	const PositionVector ring = ConnectedPositions::borderRing(patch);
	REQUIRE(ring.size() == 8);
	const auto keys = KeysOf(ring);
	REQUIRE(keys.size() == 8);
	REQUIRE_FALSE(keys.contains({ 10, 10, 7 }));
	REQUIRE(keys.contains({ 9, 9, 7 }));
	REQUIRE(keys.contains({ 11, 11, 7 }));
}

TEST_CASE("border ring excludes the patch and lists shared neighbours once", "[magic_wand]") {
	// Two tiles side by side share four neighbours; a 2x1 patch has a 4x3 box minus itself.
	const PositionVector patch { Position(10, 10, 7), Position(11, 10, 7) };
	const PositionVector ring = ConnectedPositions::borderRing(patch);
	REQUIRE(ring.size() == 10);
	REQUIRE(KeysOf(ring).size() == ring.size());
	for (const Position& position : ring) {
		REQUIRE(position.z == 7);
		REQUIRE_FALSE((position.y == 10 && (position.x == 10 || position.x == 11)));
	}
}

TEST_CASE("border ring skips positions outside the map", "[magic_wand]") {
	const PositionVector patch { Position(0, 0, 7) };
	const PositionVector ring = ConnectedPositions::borderRing(patch);
	REQUIRE(KeysOf(ring) == std::set<Key> { { 1, 0, 7 }, { 0, 1, 7 }, { 1, 1, 7 } });
}

TEST_CASE("flood never leaves the bounds", "[magic_wand]") {
	std::set<Key> members;
	for (int x = 10; x <= 14; ++x) {
		members.insert({ x, 10, 7 });
	}
	const PositionRect bounds { 10, 10, 11, 10 };
	const auto found = ConnectedPositions::collect(Position(10, 10, 7), InSet(members), Always(), &bounds);
	REQUIRE(found);
	REQUIRE(KeysOf(*found) == std::set<Key> { { 10, 10, 7 }, { 11, 10, 7 } });
}

TEST_CASE("flood from a member outside the bounds is empty", "[magic_wand]") {
	const std::set<Key> members { { 10, 10, 7 }, { 11, 10, 7 } };
	const PositionRect bounds { 11, 10, 12, 10 };
	const auto found = ConnectedPositions::collect(Position(10, 10, 7), InSet(members), Always(), &bounds);
	REQUIRE(found);
	REQUIRE(found->empty());
}

TEST_CASE("border ring drops positions outside the bounds", "[magic_wand]") {
	const PositionVector patch { Position(10, 10, 7) };
	const PositionRect bounds { 10, 10, 11, 11 };
	const PositionVector ring = ConnectedPositions::borderRing(patch, &bounds);
	REQUIRE(KeysOf(ring) == std::set<Key> { { 11, 10, 7 }, { 10, 11, 7 }, { 11, 11, 7 } });
}
