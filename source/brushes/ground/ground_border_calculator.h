//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_GROUND_BORDER_CALCULATOR_H
#define RME_GROUND_BORDER_CALCULATOR_H

#include "app/main.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/border_update_policy.h"

#include <span>
#include <utility>
#include <vector>

class BaseMap;
class Tile;

/**
 * @brief Handles the calculation of ground borders.
 */
class GroundBorderCalculator {
public:
	/**
	 * @brief Calculates and applies borders for a specific tile.
	 *
	 * @param map The map where the tile resides.
	 * @param tile The tile to calculate borders for.
	 * @param options Scope and placement of the update. The default is the classic
	 *        behaviour: every brush border is rebuilt and new borders go to the bottom.
	 */
	static void calculate(BaseMap* map, Tile* tile, const BorderUpdate::Options& options = {});

private:
	// Apply side for every non-classic Options value. The classic path stays inline
	// in calculate() so the default behaviour is untouched.
	static void applyWithOptions(
		Tile* tile,
		const GroundBrush* tile_brush,
		std::span<const std::pair<bool, GroundBrush*>, 8> neighbours,
		std::vector<GroundBrush::BorderCluster>& borderList,
		std::vector<const GroundBrush::BorderBlock*>& specificList,
		const BorderUpdate::Options& options
	);
};

#endif // RME_GROUND_BORDER_CALCULATOR_H
