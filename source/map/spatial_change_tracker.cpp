//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/spatial_change_tracker.h"
#include <algorithm>

void SpatialChangeTracker::expandFloorRect(int32_t x, int32_t y, int32_t z) {
	if (z < 0 || z >= MAP_LAYERS) {
		return;
	}

	auto& current = dirty_rects_by_floor_[z];
	if (!current) {
		current = DirtyRect{ x, y, x, y };
	} else {
		current->min_x = std::min(current->min_x, x);
		current->min_y = std::min(current->min_y, y);
		current->max_x = std::max(current->max_x, x);
		current->max_y = std::max(current->max_y, y);
	}
}

void SpatialChangeTracker::markTileDirty(int32_t x, int32_t y, int32_t z) {
	if (z < 0 || z >= MAP_LAYERS) {
		return;
	}

	expandFloorRect(x, y, z);
	++generation_;

	if (all_dirty_) {
		notifyListeners();
		return;
	}

	const int32_t nx = x >> NODE_SHIFT;
	const int32_t ny = y >> NODE_SHIFT;
	const int32_t cx = x >> CHUNK_SHIFT;
	const int32_t cy = y >> CHUNK_SHIFT;

	dirty_nodes_.insert(NodeCoord{ nx, ny, z });
	dirty_chunks_.insert(ChunkCoord{ cx, cy, z });

	notifyListeners();
}

void SpatialChangeTracker::markNodeDirty(int32_t nx, int32_t ny, int32_t z) {
	if (z < 0 || z >= MAP_LAYERS) {
		return;
	}

	const int32_t tile_x = nx << NODE_SHIFT;
	const int32_t tile_y = ny << NODE_SHIFT;
	expandFloorRect(tile_x, tile_y, z);
	expandFloorRect(tile_x + (NODE_SIZE - 1), tile_y + (NODE_SIZE - 1), z);
	++generation_;

	if (all_dirty_) {
		notifyListeners();
		return;
	}

	const int32_t cx = tile_x >> CHUNK_SHIFT;
	const int32_t cy = tile_y >> CHUNK_SHIFT;

	dirty_nodes_.insert(NodeCoord{ nx, ny, z });
	dirty_chunks_.insert(ChunkCoord{ cx, cy, z });

	notifyListeners();
}

void SpatialChangeTracker::markChunkDirty(int32_t cx, int32_t cy, int32_t z) {
	if (z < 0 || z >= MAP_LAYERS) {
		return;
	}

	const int32_t tile_x = cx << CHUNK_SHIFT;
	const int32_t tile_y = cy << CHUNK_SHIFT;
	expandFloorRect(tile_x, tile_y, z);
	expandFloorRect(tile_x + (CHUNK_SIZE - 1), tile_y + (CHUNK_SIZE - 1), z);
	++generation_;

	if (all_dirty_) {
		notifyListeners();
		return;
	}

	dirty_chunks_.insert(ChunkCoord{ cx, cy, z });

	notifyListeners();
}

void SpatialChangeTracker::markRegionDirty(int32_t start_x, int32_t start_y, int32_t end_x, int32_t end_y, int32_t z) {
	if (z < 0 || z >= MAP_LAYERS) {
		return;
	}

	if (start_x > end_x) {
		std::swap(start_x, end_x);
	}
	if (start_y > end_y) {
		std::swap(start_y, end_y);
	}

	expandFloorRect(start_x, start_y, z);
	expandFloorRect(end_x, end_y, z);
	++generation_;

	if (all_dirty_) {
		notifyListeners();
		return;
	}

	const int32_t min_cx = start_x >> CHUNK_SHIFT;
	const int32_t max_cx = end_x >> CHUNK_SHIFT;
	const int32_t min_cy = start_y >> CHUNK_SHIFT;
	const int32_t max_cy = end_y >> CHUNK_SHIFT;

	for (int32_t cx = min_cx; cx <= max_cx; ++cx) {
		for (int32_t cy = min_cy; cy <= max_cy; ++cy) {
			dirty_chunks_.insert(ChunkCoord{ cx, cy, z });
		}
	}

	const int32_t min_nx = start_x >> NODE_SHIFT;
	const int32_t max_nx = end_x >> NODE_SHIFT;
	const int32_t min_ny = start_y >> NODE_SHIFT;
	const int32_t max_ny = end_y >> NODE_SHIFT;

	for (int32_t nx = min_nx; nx <= max_nx; ++nx) {
		for (int32_t ny = min_ny; ny <= max_ny; ++ny) {
			dirty_nodes_.insert(NodeCoord{ nx, ny, z });
		}
	}

	notifyListeners();
}

void SpatialChangeTracker::markAllDirty() {
	all_dirty_ = true;
	dirty_chunks_.clear();
	dirty_nodes_.clear();
	for (auto& rect : dirty_rects_by_floor_) {
		rect.reset();
	}
	++generation_;
	notifyListeners();
}

bool SpatialChangeTracker::isChunkDirty(int32_t cx, int32_t cy, int32_t z) const noexcept {
	if (all_dirty_) {
		return true;
	}
	return dirty_chunks_.contains(ChunkCoord{ cx, cy, z });
}

void SpatialChangeTracker::clearDirty() noexcept {
	all_dirty_ = false;
	dirty_chunks_.clear();
	dirty_nodes_.clear();
	for (auto& rect : dirty_rects_by_floor_) {
		rect.reset();
	}
}

std::unordered_set<ChunkCoord, ChunkCoordHash> SpatialChangeTracker::takeDirtyChunks() noexcept {
	all_dirty_ = false;
	dirty_nodes_.clear();
	for (auto& rect : dirty_rects_by_floor_) {
		rect.reset();
	}
	auto result = std::move(dirty_chunks_);
	dirty_chunks_.clear();
	return result;
}

std::array<std::optional<DirtyRect>, MAP_LAYERS> SpatialChangeTracker::takeDirtyRects() noexcept {
	auto rects = dirty_rects_by_floor_;
	for (auto& rect : dirty_rects_by_floor_) {
		rect.reset();
	}
	return rects;
}

uint32_t SpatialChangeTracker::addListener(InvalidationListener listener) {
	const uint32_t id = next_listener_id_++;
	listeners_.push_back(ListenerEntry{ id, std::move(listener) });
	return id;
}

void SpatialChangeTracker::removeListener(uint32_t listener_id) {
	std::erase_if(listeners_, [listener_id](const ListenerEntry& entry) {
		return entry.id == listener_id;
	});
}

void SpatialChangeTracker::notifyListeners() {
	for (const auto& entry : listeners_) {
		if (entry.callback) {
			entry.callback(*this);
		}
	}
}
