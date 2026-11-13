#ifndef RME_GAME_SUBTYPE_PATTERN_H_
#define RME_GAME_SUBTYPE_PATTERN_H_

#include <cstdint>

namespace SubtypePattern {

	// How an item subtype selects a sprite pattern.
	struct Slot {
		enum class Kind : uint8_t {
			None,           // The subtype does not change the sprite.
			Representative, // Preview of a stack without a concrete count.
			Index           // Explicit pattern index 0..7.
		};

		Kind kind = Kind::None;
		uint8_t index = 0;

		static_assert(static_cast<uint8_t>(Kind::Index) <= 0x3, "SubtypePattern::Slot::Kind must fit in two key bits");

		static constexpr Slot None() {
			return {};
		}

		static constexpr Slot Representative() {
			return { Kind::Representative, 0 };
		}

		static constexpr Slot At(uint8_t pattern_index) {
			return { Kind::Index, pattern_index };
		}

		// Stable image cache key for this slot and an item id.
		// The fields do not overlap: item id in bits 10+, kind in bits 8-9, index in bits 0-7.
		constexpr uint32_t CacheKey(uint16_t item_id) const {
			return (static_cast<uint32_t>(item_id) << 10)
				| (static_cast<uint32_t>(kind) << 8)
				| index;
		}
	};

	// Representative stack pattern for previews without a concrete count.
	constexpr uint8_t RepresentativeIndex(int pattern_x) {
		return (pattern_x >= 3) ? 2 : 0;
	}

	// Tibia maps a stack count to one of eight stack sprites.
	// The brackets match the client and PatternCalculator.
	constexpr uint8_t FromCount(uint16_t count) {
		if (count <= 1) {
			return 0;
		}
		if (count <= 2) {
			return 1;
		}
		if (count <= 3) {
			return 2;
		}
		if (count <= 4) {
			return 3;
		}
		if (count < 10) {
			return 4;
		}
		if (count < 25) {
			return 5;
		}
		if (count < 50) {
			return 6;
		}
		return 7;
	}

	struct Offset {
		int x = 0;
		int y = 0;
	};

	// Converts a pattern index into sprite pattern_x / pattern_y coordinates.
	constexpr Offset Resolve(uint8_t pattern_index, int pattern_x, int pattern_y) {
		Offset offset;
		if (pattern_x <= 0) {
			return offset;
		}
		offset.x = pattern_index % pattern_x;
		if (pattern_y > 0) {
			offset.y = (pattern_index / pattern_x) % pattern_y;
		}
		return offset;
	}

} // namespace SubtypePattern

#endif // RME_GAME_SUBTYPE_PATTERN_H_
