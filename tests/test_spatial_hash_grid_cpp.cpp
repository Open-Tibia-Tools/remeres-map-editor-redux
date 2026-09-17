#include <cstdint>
#include <iostream>
#include <vector>
#include <memory>
#include <array>
#include <algorithm>
#include <cassert>
#include <random>
#include <set>

// Provide definitions needed by spatial_hash_grid.h without heavy wx/boost dependencies
#define RME_MAIN_H_
constexpr int MAP_LAYERS = 16;

class BaseMap {};

class MapNode {
public:
	uint32_t floor_mask = 0;
	explicit MapNode(uint32_t mask = 0) : floor_mask(mask) {}

	[[nodiscard]] const void* getFloor(uint32_t z) const noexcept {
		if (z >= MAP_LAYERS) return nullptr;
		return (floor_mask & (1u << z)) ? reinterpret_cast<const void*>(0x1234) : nullptr;
	}
};

// Include the real production spatial_hash_grid.h!
#include "map/spatial_hash_grid.h"

// Implement the out-of-line methods from map_region.cpp
void SpatialHashGrid::getCellCoordsFromKey(uint64_t key, int& cx, int& cy) {
	uint32_t raw_cy = static_cast<uint32_t>(key >> 32) ^ 0x80000000u;
	uint32_t raw_cx = static_cast<uint32_t>(key) ^ 0x80000000u;
	cy = static_cast<int32_t>(raw_cy);
	cx = static_cast<int32_t>(raw_cx);
}

void SpatialHashGrid::clear() {
	cells_.clear();
	last_key_ = 0;
	last_idx_ = 0;
	last_valid_ = false;
}

SpatialHashGrid::GridCell::GridCell() = default;
SpatialHashGrid::GridCell::~GridCell() = default;
SpatialHashGrid::SpatialHashGrid(BaseMap& map) : map(map) {}
SpatialHashGrid::~SpatialHashGrid() { clear(); }

// Helper harness class inheriting from SpatialHashGrid to directly populate cells_
class TestSpatialGrid : public SpatialHashGrid {
public:
	explicit TestSpatialGrid(BaseMap& map) : SpatialHashGrid(map) {}

	GridCell& getOrCreateCell(int cell_x, int cell_y) {
		uint64_t key = makeKeyFromCell(cell_x, cell_y);
		size_t idx = findOrInsertCell(key);
		return *cells_[idx].cell;
	}

	void populateChunk(int cx, int cy, int map_z) {
		int cell_x = cx >> 2;
		int cell_y = cy >> 2;
		int chunk_ix = cx & 3;
		int chunk_iy = cy & 3;

		GridCell& cell = getOrCreateCell(cell_x, cell_y);

		// Place a node with map_z on the first local node of the chunk
		int node_y = (chunk_iy << 2);
		int node_x = (chunk_ix << 2);
		int row_base = node_y << NODES_PER_CELL_SHIFT;
		int idx = row_base + node_x;

		if (!cell.nodes[idx]) {
			cell.nodes[idx] = std::make_unique<MapNode>();
		}
		cell.nodes[idx]->floor_mask |= (1u << map_z);
	}

	[[nodiscard]] const std::vector<CellEntry>& getCells() const {
		return cells_;
	}
};

void run_edge_case_tests() {
	std::cout << "[TEST] 1. Boundary edge cases..." << std::endl;
	BaseMap dummy_map;
	TestSpatialGrid grid(dummy_map);

	// Case 1.1: Empty grid
	{
		int visit_count = 0;
		grid.visitPopulatedChunks(-100, -100, 100, 100, 7, [&](int, int) {
			visit_count++;
		});
		assert(visit_count == 0 && "Empty grid must yield 0 visits");
		std::cout << "  -> Empty grid: PASS (0 visits)" << std::endl;
	}

	// Case 1.2: Inverted bounds
	{
		grid.populateChunk(0, 0, 7);
		int visit_count = 0;
		grid.visitPopulatedChunks(10, 0, 5, 0, 7, [&](int, int) { visit_count++; });
		grid.visitPopulatedChunks(0, 10, 0, 5, 7, [&](int, int) { visit_count++; });
		grid.visitPopulatedChunks(10, 10, 5, 5, 7, [&](int, int) { visit_count++; });
		assert(visit_count == 0 && "Inverted bounds must yield 0 visits");
		std::cout << "  -> Inverted bounds (min > max): PASS (0 visits)" << std::endl;
	}

	// Case 1.3: Cell boundaries spanning cx = 3, 4, 7, 8
	// Note: Chunk cx = 3 is in cell 0 (ix=3); cx = 4 is in cell 1 (ix=0).
	//       Chunk cx = 7 is in cell 1 (ix=3); cx = 8 is in cell 2 (ix=0).
	{
		TestSpatialGrid bgrid(dummy_map);
		// Populate boundary chunks
		bgrid.populateChunk(3, 0, 7);
		bgrid.populateChunk(4, 0, 7);
		bgrid.populateChunk(7, 0, 7);
		bgrid.populateChunk(8, 0, 7);

		std::vector<std::pair<int, int>> visited;
		bgrid.visitPopulatedChunks(3, 0, 8, 0, 7, [&](int cx, int cy) {
			visited.emplace_back(cx, cy);
		});

		assert(visited.size() == 4 && "Must visit exactly 4 boundary chunks");
		assert((visited[0] == std::pair{3, 0}));
		assert((visited[1] == std::pair{4, 0}));
		assert((visited[2] == std::pair{7, 0}));
		assert((visited[3] == std::pair{8, 0}));

		// Also test query spanning cx = 3..4 only
		visited.clear();
		bgrid.visitPopulatedChunks(3, 0, 4, 0, 7, [&](int cx, int cy) {
			visited.emplace_back(cx, cy);
		});
		assert(visited.size() == 2 && "Must visit exactly 2 boundary chunks (3 and 4)");
		assert((visited[0] == std::pair{3, 0}));
		assert((visited[1] == std::pair{4, 0}));

		// Also test vertical boundary: cy = 3, 4, 7, 8
		bgrid.populateChunk(0, 3, 7);
		bgrid.populateChunk(0, 4, 7);
		bgrid.populateChunk(0, 7, 7);
		bgrid.populateChunk(0, 8, 7);

		visited.clear();
		bgrid.visitPopulatedChunks(0, 3, 0, 8, 7, [&](int cx, int cy) {
			visited.emplace_back(cx, cy);
		});
		assert(visited.size() == 4 && "Must visit exactly 4 vertical boundary chunks");
		assert((visited[0] == std::pair{0, 3}));
		assert((visited[1] == std::pair{0, 4}));
		assert((visited[2] == std::pair{0, 7}));
		assert((visited[3] == std::pair{0, 8}));

		std::cout << "  -> Chunk coordinates spanning cell boundaries (cx/cy = 3, 4, 7, 8): PASS" << std::endl;
	}

	// Case 1.4: Negative coordinates and out of bounds queries
	{
		TestSpatialGrid ngrid(dummy_map);
		// Chunks at negative coordinates
		ngrid.populateChunk(-5, -5, 7); // cell (-2, -2), ix=3, iy=3
		ngrid.populateChunk(-4, -4, 7); // cell (-1, -1), ix=0, iy=0
		ngrid.populateChunk(-1, -1, 7); // cell (-1, -1), ix=3, iy=3
		ngrid.populateChunk(0, 0, 7);   // cell (0, 0), ix=0, iy=0
		ngrid.populateChunk(5, 5, 7);   // cell (1, 1), ix=1, iy=1

		// Query entirely in negative void outside populated area
		int neg_void_visits = 0;
		ngrid.visitPopulatedChunks(-1000, -1000, -500, -500, 7, [&](int, int) { neg_void_visits++; });
		assert(neg_void_visits == 0 && "Far negative void must have 0 visits");

		// Query entirely in positive void outside populated area
		int pos_void_visits = 0;
		ngrid.visitPopulatedChunks(500, 500, 1000, 1000, 7, [&](int, int) { pos_void_visits++; });
		assert(pos_void_visits == 0 && "Far positive void must have 0 visits");

		// Query spanning negative to positive across all chunks
		std::vector<std::pair<int, int>> visited;
		ngrid.visitPopulatedChunks(-10, -10, 10, 10, 7, [&](int cx, int cy) {
			visited.emplace_back(cx, cy);
		});
		assert(visited.size() == 5 && "Must visit all 5 populated chunks across negative/positive range");
		assert((visited[0] == std::pair{-5, -5}));
		assert((visited[1] == std::pair{-4, -4}));
		assert((visited[2] == std::pair{-1, -1}));
		assert((visited[3] == std::pair{0, 0}));
		assert((visited[4] == std::pair{5, 5}));

		// Floor out of bounds: map_z = -1 and map_z = 16
		int bad_z_visits = 0;
		ngrid.visitPopulatedChunks(-10, -10, 10, 10, -1, [&](int, int) { bad_z_visits++; });
		ngrid.visitPopulatedChunks(-10, -10, 10, 10, 16, [&](int, int) { bad_z_visits++; });
		ngrid.visitPopulatedChunks(-10, -10, 10, 10, 999, [&](int, int) { bad_z_visits++; });
		assert(bad_z_visits == 0 && "Out of bounds map_z must have 0 visits");

		std::cout << "  -> Negative coordinates, void frustums, out-of-bounds map_z: PASS" << std::endl;
	}
}

void run_differential_oracle_stress_test() {
	std::cout << "\n[TEST] 2. Differential Oracle Stress Test (0 false positives, 0 missed chunks)..." << std::endl;
	BaseMap dummy_map;
	TestSpatialGrid grid(dummy_map);

	std::mt19937 rng(42);

	// Populate a complex world with scattered chunks across negative and positive cells
	std::set<std::tuple<int, int, int>> ground_truth; // (cx, cy, z)

	for (int i = 0; i < 80; ++i) {
		int cx = std::uniform_int_distribution<int>(-100, 100)(rng);
		int cy = std::uniform_int_distribution<int>(-100, 100)(rng);
		int z  = std::uniform_int_distribution<int>(0, 15)(rng);

		grid.populateChunk(cx, cy, z);
		ground_truth.insert({cx, cy, z});
	}

	std::cout << "  -> Generated world with " << grid.getCells().size() << " cells and "
	          << ground_truth.size() << " populated (cx, cy, z) chunk-floors." << std::endl;

	// Perform 1000 randomized queries testing:
	// - Small viewports (zoomed in, triggering Bounded Row Search)
	// - Huge viewports (low zoom 5%, triggering Sparse linear scan)
	// - Boundary query viewports
	int sparse_path_count = 0;
	int row_path_count = 0;

	for (int q = 0; q < 1000; ++q) {
		int min_cx = std::uniform_int_distribution<int>(-120, 120)(rng);
		int min_cy = std::uniform_int_distribution<int>(-120, 120)(rng);
		int w = std::uniform_int_distribution<int>(1, 160)(rng);
		int h = std::uniform_int_distribution<int>(1, 100)(rng);
		int max_cx = min_cx + w;
		int max_cy = min_cy + h;
		int map_z = std::uniform_int_distribution<int>(0, 15)(rng);

		// Determine which BOLT path will be taken
		const int start_cell_x = min_cx >> 2;
		const int end_cell_x = max_cx >> 2;
		const int start_cell_y = min_cy >> 2;
		const int end_cell_y = max_cy >> 2;
		const size_t cell_w = static_cast<size_t>(end_cell_x - start_cell_x + 1);
		const size_t cell_h = static_cast<size_t>(end_cell_y - start_cell_y + 1);
		const size_t cell_area = cell_w * cell_h;

		if (cell_area > 2 * grid.getCells().size()) {
			sparse_path_count++;
		} else {
			row_path_count++;
		}

		// Collect ground truth for this query
		std::set<std::pair<int, int>> expected;
		for (const auto& [cx, cy, z] : ground_truth) {
			if (z == map_z && cx >= min_cx && cx <= max_cx && cy >= min_cy && cy <= max_cy) {
				expected.insert({cx, cy});
			}
		}

		// Run visitPopulatedChunks
		std::vector<std::pair<int, int>> visited;
		grid.visitPopulatedChunks(min_cx, min_cy, max_cx, max_cy, map_z, [&](int cx, int cy) {
			visited.emplace_back(cx, cy);
		});

		// Check 1: 0 duplicates
		std::set<std::pair<int, int>> visited_set(visited.begin(), visited.end());
		assert(visited.size() == visited_set.size() && "Detected duplicate chunk visit!");

		// Check 2: 0 false positives
		for (const auto& chunk : visited) {
			assert(expected.contains(chunk) && "FALSE POSITIVE: chunk visited but not populated on map_z!");
		}

		// Check 3: 0 missed chunks
		for (const auto& chunk : expected) {
			assert(visited_set.contains(chunk) && "MISSED CHUNK: populated chunk on map_z was not visited!");
		}

		// Check 4: Exact size match
		assert(visited.size() == expected.size() && "Size mismatch between visited and expected!");
	}

	std::cout << "  -> Verified 1,000 queries against Ground Truth Oracle." << std::endl;
	std::cout << "     * Sparse Path executions: " << sparse_path_count << std::endl;
	std::cout << "     * Bounded Row Path executions: " << row_path_count << std::endl;
	std::cout << "     * False Positives: 0" << std::endl;
	std::cout << "     * Missed Chunks: 0" << std::endl;
	std::cout << "     * Duplicates: 0" << std::endl;
	std::cout << "  -> Differential Oracle: PASS" << std::endl;
}

void run_multi_node_and_extreme_coordinate_tests() {
	std::cout << "\n[TEST] 3. Multi-Node per Chunk & Extreme Coordinate Stress Tests..." << std::endl;
	BaseMap dummy_map;
	TestSpatialGrid grid(dummy_map);

	// Case 3.1: Chunk where ALL 16 nodes have floors
	{
		int cell_x = 5;
		int cell_y = 5;
		int cx = 20; // 5 << 2
		int cy = 20;
		auto& cell = grid.getOrCreateCell(cell_x, cell_y);

		// Populate all 16 nodes of chunk (20, 20)
		for (int ny = 0; ny < 4; ++ny) {
			for (int nx = 0; nx < 4; ++nx) {
				int node_idx = (ny * 16) + nx;
				cell.nodes[node_idx] = std::make_unique<MapNode>();
				cell.nodes[node_idx]->floor_mask |= (1u << 7);
			}
		}

		int visit_count = 0;
		grid.visitPopulatedChunks(20, 20, 20, 20, 7, [&](int vcx, int vcy) {
			assert(vcx == 20 && vcy == 20);
			visit_count++;
		});
		assert(visit_count == 1 && "Chunk with 16 populated nodes must be visited exactly ONCE (no duplicates)");
		std::cout << "  -> Chunk with 16 nodes populated: PASS (visited exactly 1 time)" << std::endl;
	}

	// Case 3.2: Threshold boundary transition between Sparse and Bounded Row paths
	{
		TestSpatialGrid tgrid(dummy_map);
		// Populate 10 cells in a line: (0,0), (1,0), ..., (9,0)
		for (int i = 0; i < 10; ++i) {
			tgrid.populateChunk(i << 2, 0, 7);
		}
		// total cells = 10. Threshold 2 * cells = 20.
		// A query of width 20, height 1 -> area = 20 (area <= 20 -> row search)
		// A query of width 21, height 1 -> area = 21 (area > 20 -> sparse search)

		int row_visits = 0;
		// cell_w = 20: start_cell_x = 0, end_cell_x = 19 (chunk cx: 0 .. 19*4+3 = 79)
		tgrid.visitPopulatedChunks(0, 0, 79, 0, 7, [&](int, int) { row_visits++; });
		assert(row_visits == 10 && "Row path at threshold boundary must visit 10 chunks");

		int sparse_visits = 0;
		// cell_w = 21: start_cell_x = 0, end_cell_x = 20 (chunk cx: 0 .. 20*4+3 = 83)
		tgrid.visitPopulatedChunks(0, 0, 83, 0, 7, [&](int, int) { sparse_visits++; });
		assert(sparse_visits == 10 && "Sparse path at threshold boundary must visit 10 chunks");

		std::cout << "  -> BOLT Threshold transition (cell_area == 2*N vs 2*N+1): PASS (exact parity)" << std::endl;
	}

	// Case 3.3: Large coordinate range
	{
		TestSpatialGrid egrid(dummy_map);
		egrid.populateChunk(-100000, -100000, 7);
		egrid.populateChunk(100000, 100000, 7);

		std::vector<std::pair<int, int>> visited;
		egrid.visitPopulatedChunks(-100001, -100001, 100001, 100001, 7, [&](int cx, int cy) {
			visited.emplace_back(cx, cy);
		});
		assert(visited.size() == 2 && "Must visit both extreme chunks");
		assert((visited[0] == std::pair{-100000, -100000}));
		assert((visited[1] == std::pair{100000, 100000}));
		std::cout << "  -> Extreme coordinates [-100000, 100000]: PASS" << std::endl;
	}
}

int main() {
	std::cout << "==========================================================" << std::endl;
	std::cout << "Remere's Map Editor Redux: Empirical Challenge Test Suite" << std::endl;
	std::cout << "Component: SpatialHashGrid::visitPopulatedChunks" << std::endl;
	std::cout << "==========================================================" << std::endl;

	try {
		run_edge_case_tests();
		run_differential_oracle_stress_test();
		run_multi_node_and_extreme_coordinate_tests();

		std::cout << "\n==========================================================" << std::endl;
		std::cout << "ALL EMPIRICAL CHALLENGE TESTS PASSED (100% GREEN)!" << std::endl;
		std::cout << "==========================================================" << std::endl;
		return 0;
	} catch (const std::exception& e) {
		std::cerr << "FAILED with exception: " << e.what() << std::endl;
		return 1;
	}
}
