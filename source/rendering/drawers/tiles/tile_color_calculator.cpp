#include "rendering/drawers/tiles/tile_color_calculator.h"
#include "map/tile.h"
#include "game/item.h"
#include "rendering/core/drawing_options.h"
#include "app/definitions.h"
#include <array>

void TileColorCalculator::Calculate(const Tile* tile, const DrawingOptions& options, uint8_t& r, uint8_t& g, uint8_t& b) {
	if (options.show_only_colors) {
		if (tile->isPZ()) {
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
			r >>= 1;
			b >>= 1;
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
