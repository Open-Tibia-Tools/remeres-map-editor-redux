#ifndef RME_RENDERING_UTILITIES_PATTERN_CALCULATOR_H_
#define RME_RENDERING_UTILITIES_PATTERN_CALCULATOR_H_

#include "rendering/core/game_sprite.h"
#include "item_definitions/core/item_definition_store.h"
#include "game/item.h"
#include "map/tile.h"
#include <bit>
#include <cstdint>

struct SpritePatterns {
	int x = 0;
	int y = 0;
	int z = 0;
	int frame = 0;
	int subtype = -1;
};

class PatternCalculator {
private:
	static constexpr int calculatePatternOffset(int coord, uint8_t pattern_size) {
		if (pattern_size <= 1) {
			return 0;
		}
		if (std::has_single_bit(static_cast<uint32_t>(pattern_size))) {
			return coord & (pattern_size - 1);
		}
		return coord % pattern_size;
	}

public:
	static SpritePatterns Calculate(const GameSprite* spr, const ItemDefinitionView& it, const Item* item, const Tile* tile, const Position& pos, long elapsed_time = -1) {
		SpritePatterns patterns;

		if (!spr) {
			return patterns;
		}

		patterns.x = calculatePatternOffset(pos.x, spr->pattern_x);
		patterns.y = calculatePatternOffset(pos.y, spr->pattern_y);
		patterns.z = calculatePatternOffset(pos.z, spr->pattern_z);

		patterns.frame = (spr->animator) ? spr->animator->getFrame(elapsed_time) : 0;

		if (it.isSplash() || it.isFluidContainer()) {
			patterns.subtype = item->getSubtype();
		} else if (it.hasFlag(ItemFlag::IsHangable)) {
			if (tile && tile->hasHookSouth()) {
				patterns.x = 1;
			} else if (tile && tile->hasHookEast()) {
				patterns.x = 2;
			} else {
				patterns.x = 0;
			}
		} else if (it.hasFlag(ItemFlag::Stackable)) {
			uint16_t itemSubtype = item->getSubtype();
			if (itemSubtype <= 1) {
				patterns.subtype = 0;
			} else if (itemSubtype <= 2) {
				patterns.subtype = 1;
			} else if (itemSubtype <= 3) {
				patterns.subtype = 2;
			} else if (itemSubtype <= 4) {
				patterns.subtype = 3;
			} else if (itemSubtype < 10) {
				patterns.subtype = 4;
			} else if (itemSubtype < 25) {
				patterns.subtype = 5;
			} else if (itemSubtype < 50) {
				patterns.subtype = 6;
			} else {
				patterns.subtype = 7;
			}
		}

		return patterns;
	}
};

#endif
