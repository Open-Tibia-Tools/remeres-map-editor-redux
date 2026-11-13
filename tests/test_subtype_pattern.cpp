#include "game/subtype_pattern.h"

#include <cstdio>
#include <cstdlib>

namespace {

	void Check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::fprintf(stderr, "Assertion failed: %s at tests/test_subtype_pattern.cpp:%d\n", expression, line);
			std::exit(1);
		}
	}

} // namespace

#define CHECK(condition) Check(static_cast<bool>(condition), #condition, __LINE__)

int main() {
	using SubtypePattern::FromCount;
	using SubtypePattern::Resolve;
	using SubtypePattern::Slot;

	// Stack count brackets.
	CHECK(FromCount(0) == 0);
	CHECK(FromCount(1) == 0);
	CHECK(FromCount(2) == 1);
	CHECK(FromCount(3) == 2);
	CHECK(FromCount(4) == 3);
	CHECK(FromCount(5) == 4);
	CHECK(FromCount(9) == 4);
	CHECK(FromCount(10) == 5);
	CHECK(FromCount(24) == 5);
	CHECK(FromCount(25) == 6);
	CHECK(FromCount(49) == 6);
	CHECK(FromCount(50) == 7);
	CHECK(FromCount(100) == 7);
	CHECK(FromCount(65535) == 7);

	// Typed slots.
	CHECK(Slot::None().kind == Slot::Kind::None);
	CHECK(Slot::Representative().kind == Slot::Kind::Representative);
	CHECK(Slot::At(3).kind == Slot::Kind::Index);
	CHECK(Slot::At(3).index == 3);

	// Cache keys: item id, kind and index must not overlap.
	CHECK(Slot::None().CacheKey(5) != Slot::At(0).CacheKey(5));
	CHECK(Slot::None().CacheKey(5) != Slot::Representative().CacheKey(5));
	CHECK(Slot::Representative().CacheKey(5) != Slot::At(2).CacheKey(5));
	CHECK(Slot::At(0).CacheKey(5) != Slot::At(1).CacheKey(5));
	CHECK(Slot::At(0).CacheKey(5) != Slot::At(0).CacheKey(6));
	CHECK(Slot::At(7).CacheKey(65535) == 67108359u);

	// Representative stack preview.
	CHECK(SubtypePattern::RepresentativeIndex(0) == 0);
	CHECK(SubtypePattern::RepresentativeIndex(2) == 0);
	CHECK(SubtypePattern::RepresentativeIndex(3) == 2);
	CHECK(SubtypePattern::RepresentativeIndex(8) == 2);

	// Pattern resolution.
	auto offset = Resolve(0, 1, 1);
	CHECK(offset.x == 0 && offset.y == 0);

	offset = Resolve(2, 2, 0);
	CHECK(offset.x == 0 && offset.y == 0);

	offset = Resolve(5, 3, 2);
	CHECK(offset.x == 2 && offset.y == 1);

	offset = Resolve(7, 4, 2);
	CHECK(offset.x == 3 && offset.y == 1);

	offset = Resolve(7, 8, 1);
	CHECK(offset.x == 7 && offset.y == 0);

	offset = Resolve(5, 1, 4);
	CHECK(offset.x == 0 && offset.y == 1);

	offset = Resolve(3, 0, 0);
	CHECK(offset.x == 0 && offset.y == 0);

	return 0;
}
