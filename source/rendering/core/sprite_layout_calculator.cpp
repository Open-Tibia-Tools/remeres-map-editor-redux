//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/sprite_layout_calculator.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/image.h"
#include "app/definitions.h"
#include <algorithm>

namespace {

	size_t resolvePlainSpriteIndex(const GameSprite& sprite, int x, int y, int layer, int subtype, int pattern_x, int pattern_y, int pattern_z, int frame) {
		if (sprite.numsprites == 0) {
			return 0;
		}

		size_t index = 0;
		if (subtype >= 0 && sprite.height <= 1 && sprite.width <= 1) {
			index = static_cast<size_t>(subtype);
		} else {
			index = sprite.getIndex(x, y, layer, pattern_x, pattern_y, pattern_z, frame);
		}

		if (index >= sprite.numsprites) {
			index = sprite.numsprites == 1 ? 0 : index % sprite.numsprites;
		}

		return index;
	}

	size_t resolveOutfitSpriteIndex(const GameSprite& sprite, int x, int y, int dir, int addon, int pattern_z, int frame) {
		if (sprite.numsprites == 0) {
			return 0;
		}

		size_t index = sprite.getIndex(x, y, 0, dir, addon, pattern_z, frame);
		if (index >= sprite.numsprites) {
			index = sprite.numsprites == 1 ? 0 : index % sprite.numsprites;
		}
		return index;
	}

} // namespace

void SpriteLayoutCalculator::FinalizeLayoutMetrics(SpriteLayoutMetrics& metrics) {
	if (metrics.num_columns > 0) {
		metrics.total_width = 0;
		for (size_t i = 0; i < metrics.num_columns; ++i) {
			metrics.total_width += metrics.column_widths[i];
		}
		metrics.left_offset = metrics.total_width - metrics.column_widths[metrics.num_columns - 1];
	}
	if (metrics.num_rows > 0) {
		metrics.total_height = 0;
		for (size_t i = 0; i < metrics.num_rows; ++i) {
			metrics.total_height += metrics.row_heights[i];
		}
		metrics.top_offset = metrics.total_height - metrics.row_heights[metrics.num_rows - 1];
	}
}

SpriteLayoutMetrics SpriteLayoutCalculator::BuildPlainLayoutMetrics(const GameSprite& sprite, const PlainLayoutCacheKey& key) {
	SpriteLayoutMetrics metrics;
	const uint8_t cols = std::min<uint8_t>(sprite.width, static_cast<uint8_t>(MAX_SPRITE_PARTS));
	const uint8_t rows = std::min<uint8_t>(sprite.height, static_cast<uint8_t>(MAX_SPRITE_PARTS));
	metrics.num_columns = cols;
	metrics.num_rows = rows;
	for (size_t i = 0; i < cols; ++i) {
		metrics.column_widths[i] = TILE_SIZE;
	}
	for (size_t i = 0; i < rows; ++i) {
		metrics.row_heights[i] = TILE_SIZE;
	}

	for (int cx = 0; cx < cols; ++cx) {
		for (int cy = 0; cy < rows; ++cy) {
			for (int layer = 0; layer < sprite.layers; ++layer) {
				const size_t index = resolvePlainSpriteIndex(sprite, cx, cy, layer, key.subtype, key.pattern_x, key.pattern_y, key.pattern_z, key.frame);
				if (index >= sprite.spriteList.size() || !sprite.spriteList[index]) {
					continue;
				}

				const auto dimensions = sprite.spriteList[index]->getDimensions();
				metrics.column_widths[cx] = std::max<int>(metrics.column_widths[cx], static_cast<int>(dimensions.width));
				metrics.row_heights[cy] = std::max<int>(metrics.row_heights[cy], static_cast<int>(dimensions.height));
			}
		}
	}

	FinalizeLayoutMetrics(metrics);
	return metrics;
}

SpriteLayoutMetrics SpriteLayoutCalculator::BuildOutfitLayoutMetrics(const GameSprite& sprite, const OutfitLayoutCacheKey& key) {
	SpriteLayoutMetrics metrics;
	const uint8_t cols = std::min<uint8_t>(sprite.width, static_cast<uint8_t>(MAX_SPRITE_PARTS));
	const uint8_t rows = std::min<uint8_t>(sprite.height, static_cast<uint8_t>(MAX_SPRITE_PARTS));
	metrics.num_columns = cols;
	metrics.num_rows = rows;
	for (size_t i = 0; i < cols; ++i) {
		metrics.column_widths[i] = TILE_SIZE;
	}
	for (size_t i = 0; i < rows; ++i) {
		metrics.row_heights[i] = TILE_SIZE;
	}

	for (int cx = 0; cx < cols; ++cx) {
		for (int cy = 0; cy < rows; ++cy) {
			const size_t index = resolveOutfitSpriteIndex(sprite, cx, cy, key.dir, key.addon, key.pattern_z, key.frame);
			if (index >= sprite.spriteList.size() || !sprite.spriteList[index]) {
				continue;
			}

			const auto dimensions = sprite.spriteList[index]->getDimensions();
			metrics.column_widths[cx] = std::max<int>(metrics.column_widths[cx], static_cast<int>(dimensions.width));
			metrics.row_heights[cy] = std::max<int>(metrics.row_heights[cy], static_cast<int>(dimensions.height));
		}
	}

	FinalizeLayoutMetrics(metrics);
	return metrics;
}
