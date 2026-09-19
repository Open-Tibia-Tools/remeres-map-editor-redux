#ifndef RME_MAP_SPATIAL_HASH_GRID_H
#define RME_MAP_SPATIAL_HASH_GRID_H

#include "app/main.h"
#include "map/map_region.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <utility>
#include <algorithm>
#include <limits>
#include <array>

class BaseMap;

class SpatialHashGrid {
public:
	static constexpr int CELL_SHIFT = 6; // 64 tiles
	static constexpr int CELL_SIZE = 1 << CELL_SHIFT;
	static constexpr int NODE_SHIFT = 2; // 4 tiles
	static constexpr int NODES_PER_CELL_SHIFT = CELL_SHIFT - NODE_SHIFT; // 4
	static constexpr int NODES_PER_CELL = 1 << NODES_PER_CELL_SHIFT; // 16 nodes (4x4 tiles each)
	static constexpr int TILES_PER_NODE = 16; // 4x4 tiles per node
	static constexpr int NODES_IN_CELL = NODES_PER_CELL * NODES_PER_CELL; // 256 nodes per cell

	struct GridCell {
		std::array<std::unique_ptr<MapNode>, NODES_IN_CELL> nodes;
		GridCell();
		~GridCell();
	};

	// CellEntry: the flat sorted storage element
	struct CellEntry {
		uint64_t key;
		std::unique_ptr<GridCell> cell;
	};

	// SortedGridCell: lightweight view for external consumers (search, serialization)
	struct SortedGridCell {
		uint64_t key;
		int cx, cy;
		GridCell* cell;
	};

	SpatialHashGrid(BaseMap& map);
	~SpatialHashGrid();

	// Returns observer pointer (non-owning)
	MapNode* getLeaf(int x, int y);
	const MapNode* getLeaf(int x, int y) const;
	// Forces leaf creation. Throws std::bad_alloc on memory failure.
	MapNode* getLeafForce(int x, int y);
	MapNode* getLeafForceInCell(size_t cell_idx, int x, int y);
	void preallocateCells(std::vector<uint64_t>& keys);

	void clear();
	void clearVisible(uint32_t mask);

	// Returns a snapshot of cells sorted by key, for external iteration (search, serialization)
	std::vector<SortedGridCell> getSortedCells() const;
	static void getCellCoordsFromKey(uint64_t key, int& cx, int& cy);
	static uint64_t makeKeyFromCell(int cx, int cy) {
		static_assert(sizeof(int) == 4, "Key packing assumes exactly 32-bit integers");
		return (static_cast<uint64_t>(static_cast<uint32_t>(cy) ^ 0x80000000u) << 32) | (static_cast<uint32_t>(cx) ^ 0x80000000u);
	}
	static uint64_t makeKey(int x, int y) {
		return makeKeyFromCell(x >> CELL_SHIFT, y >> CELL_SHIFT);
	}

	// Cell count for strategy decisions
	[[nodiscard]] size_t cellCount() const {
		return cells_.size();
	}

	static constexpr auto cell_key_less = [](const CellEntry& entry, uint64_t k) { return entry.key < k; };

	// Binary search for a cell by key. Returns index, or cells_.size() if not found.
	[[nodiscard]] size_t findCellIndex(uint64_t key) const {
		auto it = std::lower_bound(cells_.begin(), cells_.end(), key, cell_key_less);
		if (it != cells_.end() && it->key == key) {
			return static_cast<size_t>(it - cells_.begin());
		}
		return cells_.size();
	}

	[[nodiscard]] const GridCell* getCell(size_t index) const noexcept {
		return (index < cells_.size()) ? cells_[index].cell.get() : nullptr;
	}

	template <typename CellType = GridCell>
	[[nodiscard]] static bool chunkHasFloor(const CellType& cell, int chunk_ix, int chunk_iy, int map_z) noexcept {
		if (map_z < 0 || map_z >= MAP_LAYERS) {
			return false;
		}
		const int start_lx = chunk_ix << 2; // chunk_ix * 4
		const int start_ly = chunk_iy << 2; // chunk_iy * 4
		for (int dy = 0; dy < 4; ++dy) {
			const int row_base = (start_ly + dy) << NODES_PER_CELL_SHIFT; // * 16
			for (int dx = 0; dx < 4; ++dx) {
				const MapNode* node = cell.nodes[row_base + start_lx + dx].get();
				if (node && node->getFloor(map_z)) {
					return true;
				}
			}
		}
		return false;
	}

	/**
	 * Sparsely visits all populated 16x16 chunks on floor map_z intersecting [min_cx, max_cx] x [min_cy, max_cy].
	 * Employs BOLT Hybrid Strategy: iterates sorted cells or performs row binary searches.
	 * Guarantees zero empty chunks visited and zero duplicate chunk visits.
	 */
	template <typename Func>
	void visitPopulatedChunks(int min_cx, int min_cy, int max_cx, int max_cy, int map_z, Func&& func) const {
		if (cells_.empty() || min_cx > max_cx || min_cy > max_cy) {
			return;
		}

		// Convert chunk coordinates (16 tiles) to cell coordinates (64 tiles)
		const int start_cell_x = min_cx >> 2;
		const int end_cell_x = max_cx >> 2;
		const int start_cell_y = min_cy >> 2;
		const int end_cell_y = max_cy >> 2;

		const size_t cell_region_w = static_cast<size_t>(end_cell_x - start_cell_x + 1);
		const size_t cell_region_h = static_cast<size_t>(end_cell_y - start_cell_y + 1);
		const size_t cell_region_area = cell_region_w * cell_region_h;

		auto processCell = [&](const GridCell& cell, int cell_x, int cell_y) {
			const int cell_base_cx = cell_x << 2;
			const int cell_base_cy = cell_y << 2;

			const int local_min_cx = std::max(0, min_cx - cell_base_cx);
			const int local_max_cx = std::min(3, max_cx - cell_base_cx);
			const int local_min_cy = std::max(0, min_cy - cell_base_cy);
			const int local_max_cy = std::min(3, max_cy - cell_base_cy);

			for (int ciy = local_min_cy; ciy <= local_max_cy; ++ciy) {
				for (int cix = local_min_cx; cix <= local_max_cx; ++cix) {
					if (chunkHasFloor(cell, cix, ciy, map_z)) {
						func(cell_base_cx + cix, cell_base_cy + ciy);
					}
				}
			}
		};

		// BOLT HYBRID DECISION:
		// If the query area in cells is larger than twice the total cell count,
		// it is vastly faster to scan cells_ linearly than to do row binary searches.
		if (cell_region_area > 2 * cells_.size()) {
			// Sparse Path: O(TotalCells)
			for (const auto& entry : cells_) {
				int cell_x, cell_y;
				getCellCoordsFromKey(entry.key, cell_x, cell_y);
				if (cell_x >= start_cell_x && cell_x <= end_cell_x &&
					cell_y >= start_cell_y && cell_y <= end_cell_y) {
					processCell(*entry.cell, cell_x, cell_y);
				}
			}
		} else {
			// Bounded Row Search Path: O(Rows * log(TotalCells))
			for (int cell_y = start_cell_y; cell_y <= end_cell_y; ++cell_y) {
				const uint64_t row_start_key = makeKeyFromCell(start_cell_x, cell_y);
				const uint64_t row_end_key = makeKeyFromCell(end_cell_x, cell_y);

				auto it = std::lower_bound(cells_.cbegin(), cells_.cend(), row_start_key, cell_key_less);
				while (it != cells_.cend() && it->key <= row_end_key) {
					int cell_x, cy;
					getCellCoordsFromKey(it->key, cell_x, cy);
					if (cy != cell_y) {
						break;
					}
					if (cell_x >= start_cell_x && cell_x <= end_cell_x) {
						processCell(*it->cell, cell_x, cell_y);
					}
					++it;
				}
			}
		}
	}

	template <typename Func>
	void visitLeaves(int min_x, int min_y, int max_x, int max_y, Func&& func) {
		if (max_x <= min_x || max_y <= min_y) {
			return;
		}

		int start_nx = min_x >> NODE_SHIFT;
		int start_ny = min_y >> NODE_SHIFT;
		int end_nx = (max_x - 1) >> NODE_SHIFT;
		int end_ny = (max_y - 1) >> NODE_SHIFT;

		int start_cx = start_nx >> NODES_PER_CELL_SHIFT;
		int start_cy = start_ny >> NODES_PER_CELL_SHIFT;
		int end_cx = end_nx >> NODES_PER_CELL_SHIFT;
		int end_cy = end_ny >> NODES_PER_CELL_SHIFT;

		visitLeavesImpl(start_nx, start_ny, end_nx, end_ny, start_cx, start_cy, end_cx, end_cy, std::forward<Func>(func));
	}
	template <typename Func>
	void visitLeaves(int min_x, int min_y, int max_x, int max_y, Func&& func) const {
		if (max_x <= min_x || max_y <= min_y) {
			return;
		}

		int start_nx = min_x >> NODE_SHIFT;
		int start_ny = min_y >> NODE_SHIFT;
		int end_nx = (max_x - 1) >> NODE_SHIFT;
		int end_ny = (max_y - 1) >> NODE_SHIFT;

		int start_cx = start_nx >> NODES_PER_CELL_SHIFT;
		int start_cy = start_ny >> NODES_PER_CELL_SHIFT;
		int end_cx = end_nx >> NODES_PER_CELL_SHIFT;
		int end_cy = end_ny >> NODES_PER_CELL_SHIFT;

		visitLeavesConstImpl(start_nx, start_ny, end_nx, end_ny, start_cx, start_cy, end_cx, end_cy, std::forward<Func>(func));
	}

	// Iterator support for MapIterator â€” const-only to protect sorted invariant
	auto begin() const {
		return cells_.cbegin();
	}
	auto end() const {
		return cells_.cend();
	}

protected:
	BaseMap& map;
	std::vector<CellEntry> cells_; // Sorted by key â€” contiguous, cache-friendly

	mutable uint64_t last_key_ = 0;
	mutable size_t last_idx_ = 0; // Index into cells_ for 1-element cache
	mutable bool last_valid_ = false;

	struct RowCellInfo {
		GridCell* cell;
		int cell_start_nx;
		int local_start_nx;
		int local_end_nx;
	};

	struct ConstRowCellInfo {
		const GridCell* cell;
		int cell_start_nx;
		int local_start_nx;
		int local_end_nx;
	};


	// Find or insert a cell, returning its index.
	// Allocates GridCell immediately on insertion â€” no null entries left behind.
	// Maintains sorted order via insertion at the correct position.
	size_t findOrInsertCell(uint64_t key) {
		auto it = std::lower_bound(cells_.begin(), cells_.end(), key, cell_key_less);
		if (it != cells_.end() && it->key == key) {
			return static_cast<size_t>(it - cells_.begin());
		}
		// Insert at sorted position with a fully allocated GridCell
		auto inserted = cells_.insert(it, CellEntry { key, std::make_unique<GridCell>() });
		// Invalidate cache since vector may have reallocated
		last_valid_ = false;
		return static_cast<size_t>(inserted - cells_.begin());
	}

	// Single unified traversal using binary search per row instead of hash lookups.
	template <typename Func>
	void visitLeavesImpl(int start_nx, int start_ny, int end_nx, int end_ny, int start_cx, int start_cy, int end_cx, int end_cy, Func&& func) {
		if (cells_.empty()) {
			return;
		}

		static thread_local std::vector<RowCellInfo> row_cells;
		row_cells.clear();
		row_cells.reserve(end_cx - start_cx + 1);

		for (int cy = start_cy; cy <= end_cy; ++cy) {
			row_cells.clear();

			// Binary search for the first cell in this row
			uint64_t row_start_key = makeKeyFromCell(start_cx, cy);
			uint64_t row_end_key = makeKeyFromCell(end_cx, cy);

			auto it = std::lower_bound(cells_.begin(), cells_.end(), row_start_key, cell_key_less);

			// Scan linearly through matching cells in this row (contiguous in sorted order!)
			while (it != cells_.end() && it->key <= row_end_key) {
				int cx, cell_cy;
				getCellCoordsFromKey(it->key, cx, cell_cy);
				if (cell_cy != cy) {
					break; // Moved past this row
				}
				if (cx >= start_cx && cx <= end_cx) {
					int cell_start_nx = cx << NODES_PER_CELL_SHIFT;
					row_cells.push_back({ .cell = it->cell.get(), .cell_start_nx = cell_start_nx, .local_start_nx = std::max(start_nx, cell_start_nx) - cell_start_nx, .local_end_nx = std::min(end_nx, cell_start_nx + NODES_PER_CELL - 1) - cell_start_nx });
				}
				++it;
			}

			if (row_cells.empty()) {
				continue;
			}

			int row_start_ny = std::max(start_ny, cy << NODES_PER_CELL_SHIFT);
			int row_end_ny = std::min(end_ny, ((cy + 1) << NODES_PER_CELL_SHIFT) - 1);

			for (int ny = row_start_ny; ny <= row_end_ny; ++ny) {
				int local_ny = ny & (NODES_PER_CELL - 1);
				int idx_base = local_ny * NODES_PER_CELL;

				for (const auto& row_cell : row_cells) {
					for (int lnx = row_cell.local_start_nx; lnx <= row_cell.local_end_nx; ++lnx) {
						if (MapNode* node = row_cell.cell->nodes[idx_base + lnx].get()) {
							func(node, (row_cell.cell_start_nx + lnx) << NODE_SHIFT, ny << NODE_SHIFT);
						}
					}
				}
			}
		}
	}

	template <typename Func>
	void visitLeavesConstImpl(int start_nx, int start_ny, int end_nx, int end_ny, int start_cx, int start_cy, int end_cx, int end_cy, Func&& func) const {
		if (cells_.empty()) {
			return;
		}

		struct ConstRowCellInfo {
			const GridCell* cell;
			int cell_start_nx;
			int local_start_nx;
			int local_end_nx;
		};

		static thread_local std::vector<ConstRowCellInfo> row_cells;
		row_cells.clear();
		row_cells.reserve(end_cx - start_cx + 1);

		for (int cy = start_cy; cy <= end_cy; ++cy) {
			row_cells.clear();

			const uint64_t row_start_key = makeKeyFromCell(start_cx, cy);
			const uint64_t row_end_key = makeKeyFromCell(end_cx, cy);

			auto it = std::lower_bound(cells_.cbegin(), cells_.cend(), row_start_key, cell_key_less);
			while (it != cells_.cend() && it->key <= row_end_key) {
				int cx, cell_cy;
				getCellCoordsFromKey(it->key, cx, cell_cy);
				if (cell_cy != cy) {
					break;
				}
				if (cx >= start_cx && cx <= end_cx) {
					const int cell_start_nx = cx << NODES_PER_CELL_SHIFT;
					row_cells.push_back({ .cell = it->cell.get(), .cell_start_nx = cell_start_nx, .local_start_nx = std::max(start_nx, cell_start_nx) - cell_start_nx, .local_end_nx = std::min(end_nx, cell_start_nx + NODES_PER_CELL - 1) - cell_start_nx });
				}
				++it;
			}

			if (row_cells.empty()) {
				continue;
			}

			const int row_start_ny = std::max(start_ny, cy << NODES_PER_CELL_SHIFT);
			const int row_end_ny = std::min(end_ny, ((cy + 1) << NODES_PER_CELL_SHIFT) - 1);

			for (int ny = row_start_ny; ny <= row_end_ny; ++ny) {
				const int local_ny = ny & (NODES_PER_CELL - 1);
				const int idx_base = local_ny * NODES_PER_CELL;

				for (const auto& row_cell : row_cells) {
					for (int lnx = row_cell.local_start_nx; lnx <= row_cell.local_end_nx; ++lnx) {
						if (const MapNode* node = row_cell.cell->nodes[idx_base + lnx].get()) {
							func(node, (row_cell.cell_start_nx + lnx) << NODE_SHIFT, ny << NODE_SHIFT);
						}
					}
				}
			}
		}
	}

	friend class BaseMap;
	friend class MapIterator;
};

#endif
