#include "app/main.h"
#include "rendering/drawers/tiles/tile_color_calculator.h"
#include "map/tile.h"
#include "game/item.h"
#include "rendering/core/drawing_options.h"
#include "app/definitions.h"
#include <array>

void TileColorCalculator::Calculate(const Tile* tile, const DrawingOptions& options, uint32_t current_house_id, int spawn_count, uint8_t& r, uint8_t& g, uint8_t& b) {
	bool showspecial = options.show_only_colors || options.show_special_tiles;

	if (options.show_blocking && tile->isBlocking() && (tile->ground || !tile->items.empty())) {
		// g * 2/3 approx g * 171 / 256
		g = (g * 171) >> 8;
		b = (b * 171) >> 8;
	}

	if (options.highlight_items && !tile->items.empty() && !tile->items.back()->isBorder()) {
		int item_count = static_cast<int>(tile->items.size());
		// Fixed point factors (x/256)
		// 0.75 -> 192, 0.6 -> 154, 0.48 -> 123, 0.40 -> 102, 0.33 -> 84
		static constexpr std::array<int, 5> factor = { 192, 154, 123, 102, 84 };
		int idx = std::clamp(item_count, 1, 5) - 1;
		g = (g * factor[idx]) >> 8;
		r = (r * factor[idx]) >> 8;
	}

	if (options.show_spawns && spawn_count > 0) {
		// Precomputed 0.7^n * 256 for n=1..9
		static constexpr std::array<int, 9> spawn_factor = { 179, 125, 88, 61, 43, 30, 21, 15, 10 };
		int f = spawn_factor[std::clamp(spawn_count, 1, 9) - 1];
		g = (g * f) >> 8;
		b = (b * f) >> 8;
	}

	if (options.show_houses && tile->isHouseTile()) {
		uint32_t house_id = tile->getHouseID();

		// Get unique house color
		uint8_t hr = 255, hg = 255, hb = 255;
		GetHouseColor(house_id, hr, hg, hb);

		// Apply the house unique color tint to the tile
		r = static_cast<uint8_t>((r * hr + r) >> 8);
		g = static_cast<uint8_t>((g * hg + g) >> 8);
		b = static_cast<uint8_t>((b * hb + b) >> 8);

		if (static_cast<int>(house_id) == current_house_id) {
			// Pulse Effect on top of the unique color
			// We want to make it pulse brighter/intense
			// options.highlight_pulse [0.0, 1.0]

			// Simple intensity boost
			// When pulse is high, we brighten the color towards white
			if (options.highlight_pulse > 0.0f) {
				float boost = options.highlight_pulse * 0.6f; // Max 60% boost towards white

				r = static_cast<uint8_t>(std::min(255, static_cast<int>(r + (255 - r) * boost)));
				g = static_cast<uint8_t>(std::min(255, static_cast<int>(g + (255 - g) * boost)));
				b = static_cast<uint8_t>(std::min(255, static_cast<int>(b + (255 - b) * boost)));
			}
		}
	} else if (showspecial && tile->isPZ()) {
		r >>= 1;
		b >>= 1;
	}

	if (showspecial && tile->getMapFlags() & TILESTATE_PVPZONE) {
		g = r >> 2;
		b = (b * 171) >> 8;
	}

	if (showspecial && tile->getMapFlags() & TILESTATE_NOLOGOUT) {
		b >>= 1;
	}

	if (showspecial && tile->getMapFlags() & TILESTATE_NOPVP) {
		g >>= 1;
	}
}

void TileColorCalculator::GetHouseColor(uint32_t house_id, uint8_t& r, uint8_t& g, uint8_t& b) {
	static thread_local uint32_t cached_house_id = 0xFFFFFFFF;
	static thread_local uint8_t cached_r = 0, cached_g = 0, cached_b = 0;

	if (cached_house_id == house_id) {
		r = cached_r;
		g = cached_g;
		b = cached_b;
		return;
	}

	// Use a simple seeded random to get consistent colors
	// Simple hash
	// (Cache removed as calculation is faster than hash map lookup)
	uint32_t hash = house_id;
	hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
	hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
	hash = (hash >> 16) ^ hash;

	// Generate color components
	r = (hash & 0xFF);
	g = ((hash >> 8) & 0xFF);
	b = ((hash >> 16) & 0xFF);

	// Ensure colors aren't too dark (keep at least one channnel reasonably high)
	if (r < 50 && g < 50 && b < 50) {
		r += 100;
		g += 100;
		b += 100;
	}

	cached_house_id = house_id;
	cached_r = r;
	cached_g = g;
	cached_b = b;
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
