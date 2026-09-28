#include "rendering/drawers/tiles/tile_color_calculator.h"
#include "map/tile.h"
#include "game/item.h"
#include "rendering/core/drawing_options.h"
#include "app/definitions.h"
#include <algorithm>
#include <array>

void TileColorCalculator::Calculate(const Tile* tile, const DrawingOptions& options, uint8_t& r, uint8_t& g, uint8_t& b) {
	if (options.highlight_items && !tile->items.empty() && !tile->items.back()->isBorder()) {
		int item_count = static_cast<int>(tile->items.size());
		// Fixed point factors (x/256)
		// 0.75 -> 192, 0.6 -> 154, 0.48 -> 123, 0.40 -> 102, 0.33 -> 84
		static constexpr std::array<int, 5> factor = { 192, 154, 123, 102, 84 };
		int idx = std::clamp(item_count, 1, 5) - 1;
		g = (g * factor[idx]) >> 8;
		r = (r * factor[idx]) >> 8;
	}

	if (options.show_only_colors) {
		if (tile->isPZ()) {
			r >>= 1;
			b >>= 1;
		}
		if (tile->getMapFlags() & TILESTATE_PVPZONE) {
			g = r >> 2;
			b = (b * 171) >> 8;
		}
		if (tile->getMapFlags() & TILESTATE_NOLOGOUT) {
			b >>= 1;
		}
		if (tile->getMapFlags() & TILESTATE_NOPVP) {
			g >>= 1;
		}
		if (tile->getMapFlags() & TILESTATE_REFRESH) {
			r = static_cast<uint8_t>((r * 180) >> 8);
			b >>= 1;
			g = static_cast<uint8_t>(std::min(255, static_cast<int>(g) + 48));
		}
	}
}

void TileColorCalculator::GetMinimapColor(const Tile* tile, uint8_t& r, uint8_t& g, uint8_t& b) {
	// Optimization: Use lookup table to avoid division/modulo operations per tile
	static constexpr auto table = []() {
		struct {
			uint8_t r[256], g[256], b[256];
		} t {};
		for (int i = 0; i < 256; ++i) {
			t.r[i] = static_cast<uint8_t>(i / 36 % 6 * 51);
			t.g[i] = static_cast<uint8_t>(i / 6 % 6 * 51);
			t.b[i] = static_cast<uint8_t>(i % 6 * 51);
		}
		return t;
	}();

	uint8_t color = tile->getMiniMapColor();
	r = table.r[color];
	g = table.g[color];
	b = table.b[color];
}
