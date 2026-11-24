#include "rendering/ui/selection_floor_scope.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

namespace {

	struct LegacyThreadRange {
		int start_x, start_y, start_z;
		int end_x, end_y, end_z;
		bool operator==(const LegacyThreadRange&) const = default;
	};

	struct LegacyResult {
		std::vector<LegacyThreadRange> threads;
		int numtiles = 0;
	};

	// Verbatim copy of the range math of SelectionController::ExecuteBoundboxSelection before the
	// extraction (g_settings reads replaced by parameters). Pins the box selection behavior.
	LegacyResult LegacyBoxSelection(int start_x, int start_y, int end_x, int end_y, int floor, int selection_type, bool compensated, bool show_all_floors, FloorVisibilityMode mode, int worker_threads) {
		if (start_x > end_x) {
			std::swap(start_x, end_x);
		}
		if (start_y > end_y) {
			std::swap(start_y, end_y);
		}

		int numtiles = 0;
		int threadcount = std::max(worker_threads, 1);

		int s_x = 0, s_y = 0, s_z = 0;
		int e_x = 0, e_y = 0, e_z = 0;

		switch (selection_type) {
			case 0: {
				s_z = e_z = floor;
				s_x = start_x;
				s_y = start_y;
				e_x = end_x;
				e_y = end_y;
				numtiles = (e_x - s_x + 1) * (e_y - s_y + 1);
				break;
			}
			case 1: {
				s_x = start_x;
				s_y = start_y;
				s_z = MAP_MAX_LAYER;
				e_x = end_x;
				e_y = end_y;
				e_z = floor;

				if (compensated) {
					s_x -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
					s_y -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);

					e_x -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
					e_y -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
				}

				numtiles = (s_z - e_z + 1) * (e_x - s_x + 1) * (e_y - s_y + 1);
				break;
			}
			case 2: {
				s_x = start_x;
				s_y = start_y;
				const auto range = BuildFloorVisibilityRange(floor, show_all_floors, mode);
				s_z = std::max({ range.start_floor, range.end_floor, floor });
				e_z = std::max(std::min(range.start_floor, range.end_floor), floor);
				e_x = end_x;
				e_y = end_y;

				if (compensated) {
					s_x -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
					s_y -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);

					e_x -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
					e_y -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
				}

				numtiles = (s_z - e_z + 1) * (e_x - s_x + 1) * (e_y - s_y + 1);
				break;
			}
		}

		if (numtiles < 500) {
			threadcount = 1;
		}
		int width = e_x - s_x;
		if (width < threadcount) {
			threadcount = std::min(1, width);
		}
		int remainder = width;
		int cleared = 0;
		LegacyResult result;
		result.numtiles = numtiles;
		if (width == 0) {
			result.threads.push_back({ s_x, s_y, s_z, s_x, e_y, e_z });
		} else {
			for (int i = 0; i < threadcount; ++i) {
				int chunksize = width / threadcount;
				if (i == threadcount - 1) {
					chunksize = remainder;
				}
				result.threads.push_back({ s_x + cleared, s_y, s_z, s_x + cleared + chunksize, e_y, e_z });
				cleared += chunksize;
				remainder -= chunksize;
			}
		}
		return result;
	}

	LegacyResult NewBoxSelection(int start_x, int start_y, int end_x, int end_y, int floor, int selection_type, bool compensated, bool show_all_floors, FloorVisibilityMode mode, int worker_threads) {
		const SelectionTileRect rect {
			.min_x = std::min(start_x, end_x),
			.min_y = std::min(start_y, end_y),
			.max_x = std::max(start_x, end_x),
			.max_y = std::max(start_y, end_y),
		};
		const SelectionFloorScope scope = ComputeSelectionFloorScope(rect, floor, static_cast<SelectionFloorScopeType>(selection_type), compensated, BuildFloorVisibilityRange(floor, show_all_floors, mode));
		LegacyResult result;
		result.numtiles = scope.tile_count;
		for (const SelectionBand& band : ComputeSelectionBands(scope, worker_threads)) {
			result.threads.push_back({ band.start_x, scope.start_y, scope.start_z, band.end_x, scope.end_y, scope.end_z });
		}
		return result;
	}

	// Per-floor x/y offset of the iterated rectangle relative to the requested one, replaying the
	// SelectionThread loop (shift +1 after every floor <= GROUND_LAYER when compensated).
	std::vector<std::pair<int, int>> ReplayThreadOffsets(const SelectionFloorScope& scope, const SelectionTileRect& rect, bool compensated) {
		std::vector<std::pair<int, int>> offsets;
		int offset = scope.StartOffset(rect);
		for (int z = scope.start_z; z >= scope.end_z; --z) {
			offsets.emplace_back(z, offset);
			if (z <= GROUND_LAYER && compensated) {
				++offset;
			}
		}
		return offsets;
	}

} // namespace

TEST_CASE("floor scope and bands match the legacy box selection math", "[selection][floor_scope]") {
	const std::vector<std::pair<std::pair<int, int>, std::pair<int, int>>> rects = {
		{ { 100, 100 }, { 100, 100 } },
		{ { 100, 100 }, { 101, 100 } },
		{ { 120, 90 }, { 100, 100 } },
		{ { 0, 0 }, { 40, 25 } },
		{ { 500, 600 }, { 437, 611 } },
		{ { 3, 3 }, { 3, 80 } },
	};

	for (const auto& [start, end] : rects) {
		for (int floor = 0; floor <= MAP_MAX_LAYER; ++floor) {
			for (int type = 0; type <= 2; ++type) {
				for (const bool compensated : { false, true }) {
					for (const bool show_all_floors : { false, true }) {
						for (const FloorVisibilityMode mode : { FloorVisibilityMode::ClientVisible, FloorVisibilityMode::AllVisible }) {
							for (const int workers : { 0, 1, 3, 8 }) {
								const LegacyResult legacy = LegacyBoxSelection(start.first, start.second, end.first, end.second, floor, type, compensated, show_all_floors, mode, workers);
								const LegacyResult current = NewBoxSelection(start.first, start.second, end.first, end.second, floor, type, compensated, show_all_floors, mode, workers);
								INFO("rect " << start.first << "," << start.second << " - " << end.first << "," << end.second << " floor " << floor << " type " << type << " compensated " << compensated << " all " << show_all_floors << " workers " << workers);
								REQUIRE(current.numtiles == legacy.numtiles);
								REQUIRE(current.threads == legacy.threads);
							}
						}
					}
				}
			}
		}
	}
}

TEST_CASE("current floor scope covers only the current floor", "[selection][floor_scope]") {
	const SelectionTileRect rect { .min_x = 10, .min_y = 20, .max_x = 14, .max_y = 22 };
	const SelectionFloorScope scope = ComputeSelectionFloorScope(rect, 5, SelectionFloorScopeType::CurrentFloor, true, BuildFloorVisibilityRange(5, true, FloorVisibilityMode::ClientVisible));
	REQUIRE(scope.start_z == 5);
	REQUIRE(scope.end_z == 5);
	REQUIRE(scope.start_x == 10);
	REQUIRE(scope.end_y == 22);
	REQUIRE(scope.tile_count == 15);
	REQUIRE(scope.StartOffset(rect) == 0);
}

TEST_CASE("all floors above ground: compensated offsets end at zero on the current floor", "[selection][floor_scope]") {
	const SelectionTileRect rect { .min_x = 100, .min_y = 200, .max_x = 110, .max_y = 205 };
	const int floor = 5;
	const SelectionFloorScope scope = ComputeSelectionFloorScope(rect, floor, SelectionFloorScopeType::AllFloors, true, {});
	REQUIRE(scope.start_z == MAP_MAX_LAYER);
	REQUIRE(scope.end_z == floor);
	REQUIRE(scope.start_x == 98);
	REQUIRE(scope.start_y == 198);
	REQUIRE(scope.end_x == 108);
	REQUIRE(scope.tile_count == 11 * 11 * 6);

	const auto offsets = ReplayThreadOffsets(scope, rect, true);
	REQUIRE(offsets.front() == std::pair { MAP_MAX_LAYER, -2 });
	REQUIRE(offsets[MAP_MAX_LAYER - GROUND_LAYER] == std::pair { GROUND_LAYER, -2 });
	REQUIRE(offsets[MAP_MAX_LAYER - 6] == std::pair { 6, -1 });
	REQUIRE(offsets.back() == std::pair { floor, 0 });
}

TEST_CASE("all floors without compensation never shift", "[selection][floor_scope]") {
	const SelectionTileRect rect { .min_x = 100, .min_y = 200, .max_x = 110, .max_y = 205 };
	const SelectionFloorScope scope = ComputeSelectionFloorScope(rect, 3, SelectionFloorScopeType::AllFloors, false, {});
	REQUIRE(scope.start_x == 100);
	for (const auto& [z, offset] : ReplayThreadOffsets(scope, rect, false)) {
		REQUIRE(offset == 0);
	}
}

TEST_CASE("visible floors underground use the client visible range", "[selection][floor_scope]") {
	const SelectionTileRect rect { .min_x = 50, .min_y = 50, .max_x = 60, .max_y = 55 };
	const int floor = 9;
	const auto range = BuildFloorVisibilityRange(floor, true, FloorVisibilityMode::ClientVisible);
	const SelectionFloorScope scope = ComputeSelectionFloorScope(rect, floor, SelectionFloorScopeType::VisibleFloors, true, range);
	REQUIRE(scope.start_z == 11);
	REQUIRE(scope.end_z == floor);
	REQUIRE(scope.start_x == 50);
	for (const auto& [z, offset] : ReplayThreadOffsets(scope, rect, true)) {
		REQUIRE(offset == 0);
	}
}

TEST_CASE("visible floors above ground stop at ground level and compensate", "[selection][floor_scope]") {
	const SelectionTileRect rect { .min_x = 50, .min_y = 50, .max_x = 60, .max_y = 55 };
	const int floor = 4;
	const auto range = BuildFloorVisibilityRange(floor, true, FloorVisibilityMode::ClientVisible);
	const SelectionFloorScope scope = ComputeSelectionFloorScope(rect, floor, SelectionFloorScopeType::VisibleFloors, true, range);
	REQUIRE(scope.start_z == GROUND_LAYER);
	REQUIRE(scope.end_z == floor);
	REQUIRE(scope.start_x == 47);
	const auto offsets = ReplayThreadOffsets(scope, rect, true);
	REQUIRE(offsets.size() == 4);
	REQUIRE(offsets.front() == std::pair { GROUND_LAYER, -3 });
	REQUIRE(offsets.back() == std::pair { floor, 0 });
}

TEST_CASE("visible floors with show-all off is the current floor only", "[selection][floor_scope]") {
	const SelectionTileRect rect { .min_x = 1, .min_y = 1, .max_x = 2, .max_y = 2 };
	const auto range = BuildFloorVisibilityRange(GROUND_LAYER, false, FloorVisibilityMode::ClientVisible);
	const SelectionFloorScope scope = ComputeSelectionFloorScope(rect, GROUND_LAYER, SelectionFloorScopeType::VisibleFloors, true, range);
	REQUIRE(scope.start_z == GROUND_LAYER);
	REQUIRE(scope.end_z == GROUND_LAYER);
	REQUIRE(scope.tile_count == 4);
}

TEST_CASE("current floor offset matches the replayed thread offset", "[selection][floor_scope]") {
	const SelectionTileRect rect { .min_x = 100, .min_y = 200, .max_x = 110, .max_y = 205 };

	for (int floor = 0; floor <= MAP_MAX_LAYER; ++floor) {
		for (int type = 0; type <= 2; ++type) {
			for (const bool compensated : { false, true }) {
				for (const bool show_all_floors : { false, true }) {
					for (const FloorVisibilityMode mode : { FloorVisibilityMode::ClientVisible, FloorVisibilityMode::AllVisible }) {
						const auto range = BuildFloorVisibilityRange(floor, show_all_floors, mode);
						const SelectionFloorScope scope = ComputeSelectionFloorScope(rect, floor, static_cast<SelectionFloorScopeType>(type), compensated, range);
						INFO("floor " << floor << " type " << type << " compensated " << compensated << " all " << show_all_floors);
						REQUIRE(ComputeSelectionFloorShift(static_cast<SelectionFloorScopeType>(type), compensated, floor) == -scope.StartOffset(rect));
					}
				}
			}
		}
	}
}

TEST_CASE("visible floors without all floors keeps the compensated pre-shift", "[selection][floor_scope]") {
	const SelectionTileRect rect { .min_x = 100, .min_y = 200, .max_x = 110, .max_y = 205 };
	const int floor = 3; // Above ground: 4 tiles of compensation.
	const auto range = BuildFloorVisibilityRange(floor, false, FloorVisibilityMode::ClientVisible);
	const SelectionFloorScope scope = ComputeSelectionFloorScope(rect, floor, SelectionFloorScopeType::VisibleFloors, true, range);

	REQUIRE(scope.start_z == floor);
	REQUIRE(scope.end_z == floor);
	REQUIRE(scope.start_x == 96);
	REQUIRE(scope.StartOffset(rect) == -4);

	const auto compensated = ReplayThreadOffsets(scope, rect, true);
	REQUIRE(compensated.size() == 1);
	REQUIRE(compensated.front() == std::pair { floor, -4 });

	const SelectionFloorScope uncompensated = ComputeSelectionFloorScope(rect, floor, SelectionFloorScopeType::VisibleFloors, false, range);
	const auto offsets = ReplayThreadOffsets(uncompensated, rect, false);
	REQUIRE(offsets.size() == 1);
	REQUIRE(offsets.front() == std::pair { floor, 0 });
}

TEST_CASE("bands split the width and share border columns", "[selection][floor_scope]") {
	SelectionFloorScope scope;
	scope.start_x = 0;
	scope.end_x = 100;
	scope.tile_count = 10000;
	const auto bands = ComputeSelectionBands(scope, 4);
	REQUIRE(bands.size() == 4);
	REQUIRE(bands.front().start_x == 0);
	REQUIRE(bands[0].end_x == bands[1].start_x);
	REQUIRE(bands.back().end_x == 100);

	scope.tile_count = 499;
	REQUIRE(ComputeSelectionBands(scope, 4).size() == 1);

	scope.end_x = 0;
	const auto single = ComputeSelectionBands(scope, 4);
	REQUIRE(single.size() == 1);
	REQUIRE(single.front().start_x == 0);
	REQUIRE(single.front().end_x == 0);
}
