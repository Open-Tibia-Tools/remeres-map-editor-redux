#include "game/subtype_pattern.h"

#include <catch2/catch_test_macros.hpp>

using SubtypePattern::FromCount;
using SubtypePattern::RepresentativeIndex;
using SubtypePattern::Resolve;
using SubtypePattern::Slot;

TEST_CASE("stack count brackets match the client", "[subtype]") {
	REQUIRE(FromCount(0) == 0);
	REQUIRE(FromCount(1) == 0);
	REQUIRE(FromCount(2) == 1);
	REQUIRE(FromCount(3) == 2);
	REQUIRE(FromCount(4) == 3);
	REQUIRE(FromCount(5) == 4);
	REQUIRE(FromCount(9) == 4);
	REQUIRE(FromCount(10) == 5);
	REQUIRE(FromCount(24) == 5);
	REQUIRE(FromCount(25) == 6);
	REQUIRE(FromCount(49) == 6);
	REQUIRE(FromCount(50) == 7);
	REQUIRE(FromCount(100) == 7);
	REQUIRE(FromCount(65535) == 7);
}

TEST_CASE("pattern slots map to unique image cache keys", "[subtype]") {
	REQUIRE(Slot::None().kind == Slot::Kind::None);
	REQUIRE(Slot::Representative().kind == Slot::Kind::Representative);
	REQUIRE(Slot::At(3).kind == Slot::Kind::Index);
	REQUIRE(Slot::At(3).index == 3);

	REQUIRE(Slot::None().CacheKey(5) != Slot::At(0).CacheKey(5));
	REQUIRE(Slot::None().CacheKey(5) != Slot::Representative().CacheKey(5));
	REQUIRE(Slot::Representative().CacheKey(5) != Slot::At(2).CacheKey(5));
	REQUIRE(Slot::At(0).CacheKey(5) != Slot::At(1).CacheKey(5));
	REQUIRE(Slot::At(0).CacheKey(5) != Slot::At(0).CacheKey(6));
	REQUIRE(Slot::At(7).CacheKey(65535) == 67108359u);
}

TEST_CASE("representative stack preview follows the pattern width", "[subtype]") {
	REQUIRE(RepresentativeIndex(0) == 0);
	REQUIRE(RepresentativeIndex(2) == 0);
	REQUIRE(RepresentativeIndex(3) == 2);
	REQUIRE(RepresentativeIndex(8) == 2);
}

TEST_CASE("pattern indices resolve to sprite grid coordinates", "[subtype]") {
	auto offset = Resolve(0, 1, 1);
	REQUIRE(offset.x == 0);
	REQUIRE(offset.y == 0);

	offset = Resolve(2, 2, 0);
	REQUIRE(offset.x == 0);
	REQUIRE(offset.y == 0);

	offset = Resolve(5, 3, 2);
	REQUIRE(offset.x == 2);
	REQUIRE(offset.y == 1);

	offset = Resolve(7, 4, 2);
	REQUIRE(offset.x == 3);
	REQUIRE(offset.y == 1);

	offset = Resolve(7, 8, 1);
	REQUIRE(offset.x == 7);
	REQUIRE(offset.y == 0);

	offset = Resolve(5, 1, 4);
	REQUIRE(offset.x == 0);
	REQUIRE(offset.y == 1);

	offset = Resolve(3, 0, 0);
	REQUIRE(offset.x == 0);
	REQUIRE(offset.y == 0);
}
