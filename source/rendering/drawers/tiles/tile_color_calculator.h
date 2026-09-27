#ifndef RME_RENDERING_TILE_COLOR_CALCULATOR_H_
#define RME_RENDERING_TILE_COLOR_CALCULATOR_H_

#include <cstdint>

class Tile;
struct DrawingOptions;

class TileColorCalculator {
public:
	static void Calculate(const Tile* tile, const DrawingOptions& options, uint8_t& r, uint8_t& g, uint8_t& b);
	static void GetMinimapColor(const Tile* tile, uint8_t& r, uint8_t& g, uint8_t& b);
};

#endif
