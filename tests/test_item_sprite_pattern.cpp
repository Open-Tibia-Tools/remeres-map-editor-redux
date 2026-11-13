#include "game/item_sprite_pattern.h"

#include <cstdio>
#include <cstdlib>

namespace {

	void Check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::fprintf(stderr, "Assertion failed: %s at tests/test_item_sprite_pattern.cpp:%d\n", expression, line);
			std::exit(1);
		}
	}

} // namespace

#define CHECK(condition) Check(static_cast<bool>(condition), #condition, __LINE__)

int main() {
	using ItemSpritePattern::FromCount;
	using ItemSpritePattern::PatternSlot;
	using ItemSpritePattern::Resolve;

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
	CHECK(PatternSlot::None().kind == PatternSlot::Kind::None);
	CHECK(PatternSlot::Representative().kind == PatternSlot::Kind::Representative);
	CHECK(PatternSlot::At(3).kind == PatternSlot::Kind::Index);
	CHECK(PatternSlot::At(3).index == 3);

	// Cache keys: item id, kind and index must not overlap.
	CHECK(PatternSlot::None().CacheKey(5) != PatternSlot::At(0).CacheKey(5));
	CHECK(PatternSlot::None().CacheKey(5) != PatternSlot::Representative().CacheKey(5));
	CHECK(PatternSlot::Representative().CacheKey(5) != PatternSlot::At(2).CacheKey(5));
	CHECK(PatternSlot::At(0).CacheKey(5) != PatternSlot::At(1).CacheKey(5));
	CHECK(PatternSlot::At(0).CacheKey(5) != PatternSlot::At(0).CacheKey(6));
	CHECK(PatternSlot::At(7).CacheKey(65535) == 67108359u);

	// Representative stack preview.
	CHECK(ItemSpritePattern::RepresentativeIndex(0) == 0);
	CHECK(ItemSpritePattern::RepresentativeIndex(2) == 0);
	CHECK(ItemSpritePattern::RepresentativeIndex(3) == 2);
	CHECK(ItemSpritePattern::RepresentativeIndex(8) == 2);

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
