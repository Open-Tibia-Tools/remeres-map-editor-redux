//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/spatial_change_tracker.h"
#include <algorithm>

SpatialChangeTracker::~SpatialChangeTracker() {
    if (lifetime_token_) {
        lifetime_token_->tracker = nullptr;
    }
    lifetime_token_.reset();
}

SpatialChangeTracker::SpatialChangeTracker(SpatialChangeTracker&& other) noexcept :
	all_dirty_(other.all_dirty_),
	generation_(other.generation_),
	dirty_chunks_(std::move(other.dirty_chunks_)),
	listeners_(std::move(other.listeners_)),
	region_listeners_(std::move(other.region_listeners_)),
	next_listener_id_(other.next_listener_id_),
	lifetime_token_(std::move(other.lifetime_token_)) {
    if (lifetime_token_) {
        lifetime_token_->tracker = this;
    }
    other.all_dirty_ = false;
	other.listeners_.clear();
	other.region_listeners_.clear();
}

SpatialChangeTracker& SpatialChangeTracker::operator=(SpatialChangeTracker&& other) noexcept {
	if (this != &other) {
        if (lifetime_token_) {
            lifetime_token_->tracker = nullptr;
        }
        lifetime_token_.reset();
		all_dirty_ = other.all_dirty_;
		generation_ = other.generation_;
		dirty_chunks_ = std::move(other.dirty_chunks_);
		listeners_ = std::move(other.listeners_);
		region_listeners_ = std::move(other.region_listeners_);
		next_listener_id_ = other.next_listener_id_;
		lifetime_token_ = std::move(other.lifetime_token_);
        if (lifetime_token_) {
            lifetime_token_->tracker = this;
        }
        other.all_dirty_ = false;
		other.listeners_.clear();
		other.region_listeners_.clear();
	}
	return *this;
}

void SpatialChangeTracker::markTileDirty(int32_t x, int32_t y, int32_t z) {
	if (z < 0 || z >= MAP_LAYERS) {
		return;
	}

	++generation_;

	if (all_dirty_) {
		notifyListeners();
		return;
	}

	const int32_t cx = x >> CHUNK_SHIFT;
	const int32_t cy = y >> CHUNK_SHIFT;

	dirty_chunks_.insert(ChunkCoord{ cx, cy, z });

	notifyListeners();
	notifyRegionListeners(cx, cy, cx, cy, z, false);
}

void SpatialChangeTracker::markNodeDirty(int32_t nx, int32_t ny, int32_t z) {
	if (z < 0 || z >= MAP_LAYERS) {
		return;
	}

	const int32_t tile_x = nx << NODE_SHIFT;
	const int32_t tile_y = ny << NODE_SHIFT;
	++generation_;

	if (all_dirty_) {
		notifyListeners();
		return;
	}

	const int32_t cx = tile_x >> CHUNK_SHIFT;
	const int32_t cy = tile_y >> CHUNK_SHIFT;

	dirty_chunks_.insert(ChunkCoord{ cx, cy, z });

	notifyListeners();
	notifyRegionListeners(cx, cy, cx, cy, z, false);
}

void SpatialChangeTracker::markChunkDirty(int32_t cx, int32_t cy, int32_t z) {
	if (z < 0 || z >= MAP_LAYERS) {
		return;
	}

	++generation_;

	if (all_dirty_) {
		notifyListeners();
		return;
	}

	dirty_chunks_.insert(ChunkCoord{ cx, cy, z });

	notifyListeners();
	notifyRegionListeners(cx, cy, cx, cy, z, false);
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

	notifyListeners();
	notifyRegionListeners(min_cx, min_cy, max_cx, max_cy, z, false);
}

void SpatialChangeTracker::markAllDirty() {
	all_dirty_ = true;
	dirty_chunks_.clear();
	++generation_;
	notifyListeners();
	notifyRegionListeners(0, 0, 0, 0, 0, true);
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
}

std::unordered_set<ChunkCoord, ChunkCoordHash> SpatialChangeTracker::takeDirtyChunks() noexcept {
	all_dirty_ = false;
	auto result = std::move(dirty_chunks_);
	dirty_chunks_.clear();
	return result;
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

uint32_t SpatialChangeTracker::addRegionListener(RegionInvalidationListener listener) {
	const uint32_t id = next_listener_id_++;
	region_listeners_.push_back(RegionListenerEntry{ id, std::move(listener) });
	return id;
}

void SpatialChangeTracker::removeRegionListener(uint32_t listener_id) {
	std::erase_if(region_listeners_, [listener_id](const RegionListenerEntry& entry) {
		return entry.id == listener_id;
	});
}

void SpatialChangeTracker::notifyRegionListeners(int32_t min_cx, int32_t min_cy, int32_t max_cx, int32_t max_cy, int32_t z, bool all_dirty) {
	for (const auto& entry : region_listeners_) {
		if (entry.callback) {
			entry.callback(min_cx, min_cy, max_cx, max_cy, z, all_dirty);
		}
	}
}
