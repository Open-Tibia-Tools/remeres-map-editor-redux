#ifndef RME_RENDERING_UTILITIES_PATTERN_CALCULATOR_H_
#define RME_RENDERING_UTILITIES_PATTERN_CALCULATOR_H_

#include "rendering/core/game_sprite.h"
#include "item_definitions/core/item_definition_store.h"
#include "game/item.h"
#include "game/subtype_pattern.h"
#include "map/tile.h"
#include "brushes/brush_enums.h"
#include <bit>
#include <cstdint>
#include <optional>

struct SpritePatterns {
	int x = 0;
	int y = 0;
	int z = 0;
	int frame = 0;
	std::optional<int> subtype = std::nullopt;
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
	static SpritePatterns Calculate(const GameSprite* spr, const ItemDefinitionView& it, const Item* item, const Tile* tile, const Position& pos, std::optional<int64_t> elapsed_time = std::nullopt) {
		SpritePatterns patterns;

		if (!spr) {
			return patterns;
		}

		if (spr->is_simple && !it.hasFlag(ItemFlag::Stackable) && !it.hasFlag(ItemFlag::IsHangable) && !it.isSplash() && !it.isFluidContainer()) {
			return patterns;
		}

		patterns.x = calculatePatternOffset(pos.x, spr->pattern_x);
		patterns.y = calculatePatternOffset(pos.y, spr->pattern_y);
		patterns.z = calculatePatternOffset(pos.z, spr->pattern_z);

		patterns.frame = (spr->isAnimated() && elapsed_time.has_value()) ? spr->getAnimationFrame(*elapsed_time, pos.x, pos.y, pos.z) : 0;

		if (it.isSplash() || it.isFluidContainer()) {
			const uint16_t fluid = item ? item->getSubtype() : 0;
			patterns.subtype = fluid;
			const auto offset = SubtypePattern::Resolve(Item::ServerFluidToSpritePattern(fluid), spr->pattern_x, spr->pattern_y);
			patterns.x = offset.x;
			patterns.y = offset.y;

		} else if (it.hasFlag(ItemFlag::IsHangable)) {
			if (tile && tile->hasHookSouth()) {
				patterns.x = 1;
			} else if (tile && tile->hasHookEast()) {
				patterns.x = 2;
			} else if (tile && tile->hasWall()) {
				Item* wall = tile->getWall();
				BorderType alignment = wall ? wall->getWallAlignment() : BORDER_NONE;
				if (alignment == WALL_HORIZONTAL || alignment == WALL_SOUTH_T || alignment == WALL_NORTH_T) {
					patterns.x = 1;
				} else if (alignment == WALL_VERTICAL || alignment == WALL_WEST_T || alignment == WALL_EAST_T) {
					patterns.x = 2;
				} else {
					patterns.x = 0;
				}
			} else {
				patterns.x = 0;
			}
		} else if (it.hasFlag(ItemFlag::Stackable)) {
			const uint16_t itemSubtype = item ? item->getSubtype() : 0;
			const uint8_t slot = SubtypePattern::FromCount(itemSubtype);
			patterns.subtype = slot;
			const auto offset = SubtypePattern::Resolve(slot, spr->pattern_x, spr->pattern_y);
			patterns.x = offset.x;
			patterns.y = offset.y;
		}

		return patterns;
	}
};

#endif
