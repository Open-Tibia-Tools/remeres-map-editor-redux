//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_SPRITE_LAYOUT_CALCULATOR_H_
#define RME_RENDERING_CORE_SPRITE_LAYOUT_CALCULATOR_H_

#include <cstdint>
#include <cstddef>
#include <array>
#include "app/definitions.h"

class GameSprite;

struct SpriteLayoutMetrics {
	uint8_t num_columns = 0;
	uint8_t num_rows = 0;
	std::array<int, MAX_SPRITE_PARTS> column_widths {};
	std::array<int, MAX_SPRITE_PARTS> row_heights {};
	int total_width = 0;
	int total_height = 0;
	int left_offset = 0;
	int top_offset = 0;
};

struct PlainLayoutCacheKey {
	int subtype = 0;
	int pattern_x = 0;
	int pattern_y = 0;
	int pattern_z = 0;
	int frame = 0;

	bool operator==(const PlainLayoutCacheKey&) const = default;
};

struct OutfitLayoutCacheKey {
	int dir = 0;
	int addon = 0;
	int pattern_z = 0;
	int frame = 0;

	bool operator==(const OutfitLayoutCacheKey&) const = default;
};

class SpriteLayoutCalculator {
public:
	[[nodiscard]] static SpriteLayoutMetrics BuildPlainLayoutMetrics(const GameSprite& sprite, const PlainLayoutCacheKey& key);
	[[nodiscard]] static SpriteLayoutMetrics BuildOutfitLayoutMetrics(const GameSprite& sprite, const OutfitLayoutCacheKey& key);
	static void FinalizeLayoutMetrics(SpriteLayoutMetrics& metrics);
};

#endif
