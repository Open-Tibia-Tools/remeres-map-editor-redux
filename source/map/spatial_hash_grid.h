#ifndef RME_MAP_SPATIAL_HASH_GRID_H
#define RME_MAP_SPATIAL_HASH_GRID_H

#include "app/main.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <utility>
#include <algorithm>
#include <limits>
#include <array>

class MapNode;
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

	static constexpr auto cell_key_less = [](const CellEntry& entry, uint64_t k) { return entry.key < k; };

	// Binary search for a cell by key. Returns index, or cells_.size() if not found.
	[[nodiscard]] size_t findCellIndex(uint64_t key) const {
		auto it = std::lower_bound(cells_.begin(), cells_.end(), key, cell_key_less);
		if (it != cells_.end() && it->key == key) {
			return static_cast<size_t>(it - cells_.begin());
		}
		return cells_.size();
	}

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
