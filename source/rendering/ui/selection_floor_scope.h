//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_UI_SELECTION_FLOOR_SCOPE_H_
#define RME_RENDERING_UI_SELECTION_FLOOR_SCOPE_H_

#include "app/definitions.h"
#include "rendering/core/floor_visibility_mode.h"

#include <algorithm>
#include <vector>

// Floor range and per-floor offsets of an area selection (box and lasso share this, so the two
// tools always cover the same tiles for the same rectangle).
//
// Values mirror Config::SELECTION_TYPE (SELECT_CURRENT_FLOOR, SELECT_ALL_FLOORS, SELECT_VISIBLE_FLOORS).
enum class SelectionFloorScopeType : int {
	CurrentFloor = 0,
	AllFloors = 1,
	VisibleFloors = 2,
};

// Inclusive tile rectangle on the current floor (min <= max on both axes).
struct SelectionTileRect {
	int min_x = 0;
	int min_y = 0;
	int max_x = 0;
	int max_y = 0;
};

// What SelectionThread iterates: floors from start_z down to end_z, starting with the rectangle
// [start_x, end_x] x [start_y, end_y]. With COMPENSATED_SELECT the thread then moves the rectangle
// by +1 tile on x and y after every floor at or above GROUND_LAYER.
struct SelectionFloorScope {
	int start_x = 0;
	int start_y = 0;
	int start_z = 0;
	int end_x = 0;
	int end_y = 0;
	int end_z = 0;
	int tile_count = 0;

	// Offset of the start floor's rectangle relative to the requested rectangle (0 or negative).
	[[nodiscard]] constexpr int StartOffset(const SelectionTileRect& rect) const noexcept {
		return start_x - rect.min_x;
	}
};

// One worker's x range; bands share their border column, as the box selection always did.
struct SelectionBand {
	int start_x = 0;
	int end_x = 0;
};

[[nodiscard]] constexpr SelectionFloorScope ComputeSelectionFloorScope(const SelectionTileRect& rect, int floor, SelectionFloorScopeType type, bool compensated, const FloorVisibilityRange& visible_range) noexcept {
	const int compensation = floor < GROUND_LAYER ? GROUND_LAYER - floor : 0;

	SelectionFloorScope scope;
	switch (type) {
		case SelectionFloorScopeType::CurrentFloor: {
			scope.start_z = scope.end_z = floor;
			scope.start_x = rect.min_x;
			scope.start_y = rect.min_y;
			scope.end_x = rect.max_x;
			scope.end_y = rect.max_y;
			scope.tile_count = (scope.end_x - scope.start_x + 1) * (scope.end_y - scope.start_y + 1);
			break;
		}
		case SelectionFloorScopeType::AllFloors: {
			scope.start_x = rect.min_x;
			scope.start_y = rect.min_y;
			scope.start_z = MAP_MAX_LAYER;
			scope.end_x = rect.max_x;
			scope.end_y = rect.max_y;
			scope.end_z = floor;

			if (compensated) {
				scope.start_x -= compensation;
				scope.start_y -= compensation;

				scope.end_x -= compensation;
				scope.end_y -= compensation;
			}

			scope.tile_count = (scope.start_z - scope.end_z + 1) * (scope.end_x - scope.start_x + 1) * (scope.end_y - scope.start_y + 1);
			break;
		}
		case SelectionFloorScopeType::VisibleFloors: {
			scope.start_x = rect.min_x;
			scope.start_y = rect.min_y;
			scope.start_z = std::max({ visible_range.start_floor, visible_range.end_floor, floor });
			scope.end_z = std::max(std::min(visible_range.start_floor, visible_range.end_floor), floor);
			scope.end_x = rect.max_x;
			scope.end_y = rect.max_y;

			if (compensated) {
				scope.start_x -= compensation;
				scope.start_y -= compensation;

				scope.end_x -= compensation;
				scope.end_y -= compensation;
			}

			scope.tile_count = (scope.start_z - scope.end_z + 1) * (scope.end_x - scope.start_x + 1) * (scope.end_y - scope.start_y + 1);
			break;
		}
	}
	return scope;
}

// Distance the selection rectangle may extend past the map edge on the current floor so the
// compensated lower floors still cover the whole area. Box and lasso selection do not clamp.
[[nodiscard]] constexpr int ComputeSelectionFloorShift(SelectionFloorScopeType type, bool compensated, int floor) noexcept {
	if (!compensated || floor >= GROUND_LAYER) {
		return 0;
	}
	switch (type) {
		case SelectionFloorScopeType::AllFloors:
		case SelectionFloorScopeType::VisibleFloors:
			return GROUND_LAYER - floor;
		case SelectionFloorScopeType::CurrentFloor:
		default:
			return 0;
	}
}

// Splits the scope into worker bands along x, exactly as the box selection always did.
[[nodiscard]] inline std::vector<SelectionBand> ComputeSelectionBands(const SelectionFloorScope& scope, int worker_threads) {
	int threadcount = std::max(worker_threads, 1);
	if (scope.tile_count < 500) {
		// No point in threading for such a small set.
		threadcount = 1;
	}

	const int width = scope.end_x - scope.start_x;
	if (width < threadcount) {
		threadcount = std::min(1, width);
	}

	std::vector<SelectionBand> bands;
	if (width == 0) {
		bands.push_back({ .start_x = scope.start_x, .end_x = scope.start_x });
		return bands;
	}

	int remainder = width;
	int cleared = 0;
	for (int i = 0; i < threadcount; ++i) {
		int chunksize = width / threadcount;
		// The last band takes all the remainder
		if (i == threadcount - 1) {
			chunksize = remainder;
		}
		bands.push_back({ .start_x = scope.start_x + cleared, .end_x = scope.start_x + cleared + chunksize });
		cleared += chunksize;
		remainder -= chunksize;
	}
	return bands;
}

#endif
