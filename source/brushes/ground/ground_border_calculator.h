//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_GROUND_BORDER_CALCULATOR_H
#define RME_GROUND_BORDER_CALCULATOR_H

#include "app/main.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/border_update_policy.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

class AutoBorder;
class BaseMap;
class Tile;

/**
 * @brief What one border recalculation saw and decided, for the Autoborder Debug window.
 *
 * Filled only when a trace is passed to GroundBorderCalculator::calculate; recording
 * never changes the result.
 */
struct GroundBorderTrace {
	struct Cluster {
		const AutoBorder* border = nullptr;
		uint32_t alignment = 0; // Neighbour bits, 1 << index of the 8 neighbours.
		int32_t z = 0;
		bool generated = true; // False when a scoped update leaves it to its own brush.
	};

	struct ExistingItem {
		uint32_t id = 0;
		bool is_border = false;
		bool is_border_layer = false;
		// Removal decision of a scoped update; empty for non-borders and for
		// updates that rewrite every brush border.
		std::optional<BorderUpdate::RewriteVerdict> verdict;
		bool removed = false;
	};

	struct GeneratedPiece {
		uint32_t id = 0;
		bool skipped = false; // Already shown on the tile by a kept item.
	};

	struct SpecificCase {
		const GroundBrush::BorderBlock* block = nullptr;
		std::size_t case_index = 0;
		bool owned_by_active = true; // False: a scoped update skips the whole block.
		std::optional<BorderUpdate::SpecificCaseOutcome> outcome; // Empty when skipped or not traced.
	};

	bool classic = false;
	const GroundBrush* tile_brush = nullptr;
	std::array<const GroundBrush*, 8> neighbours {};
	// Brushes whose ownership protects shared ids (scoped updates only), without the active brush.
	std::vector<const GroundBrush*> context_brushes;
	std::vector<Cluster> clusters; // Sorted by z, lowest first.
	std::vector<ExistingItem> before;
	std::vector<GeneratedPiece> generated; // In final stack order, bottom first.
	std::size_t insertion_index = 0;
	std::vector<SpecificCase> specific_cases;
	bool optional_border_before = false;
};

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
	 * @param trace When set, receives the decisions of this recalculation.
	 */
	static void calculate(BaseMap* map, Tile* tile, const BorderUpdate::Options& options = {}, GroundBorderTrace* trace = nullptr);

private:
	// Apply side for every non-classic Options value. The classic path stays inline
	// in calculate() so the default behaviour is untouched.
	static void applyWithOptions(
		Tile* tile,
		const GroundBrush* tile_brush,
		std::span<const std::pair<bool, GroundBrush*>, 8> neighbours,
		std::vector<GroundBrush::BorderCluster>& borderList,
		std::vector<const GroundBrush::BorderBlock*>& specificList,
		const BorderUpdate::Options& options,
		GroundBorderTrace* trace
	);
};

#endif // RME_GROUND_BORDER_CALCULATOR_H
