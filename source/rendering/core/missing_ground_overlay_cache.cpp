#include "rendering/core/missing_ground_overlay_cache.h"

#include "app/definitions.h"
#include "map/basemap.h"
#include "map/missing_ground_chunk.h"

#include <algorithm>
#include <cstdlib>

namespace rme::rendering {

    MissingGroundOverlayCache::~MissingGroundOverlayCache()
    {
        detachTracker();
    }

    void MissingGroundOverlayCache::attachTracker(SpatialChangeTracker& tracker)
    {
        if (const auto guard = tracker_lifetime_.lock(); guard && guard->tracker == &tracker) {
            return;
        }
        detachTracker();
        tracker_lifetime_ = tracker.getLifetimeToken();
        listener_id_
            = tracker.addRegionListener([this](int32_t min_cx, int32_t min_cy, int32_t max_cx, int32_t max_cy, int32_t z, bool all_dirty) {
                  if (!enabled_) {
                      return;
                  }
                  if (all_dirty) {
                      pending_all_dirty_ = true;
                      pending_dirty_regions_.clear();
                      return;
                  }
                  if (!pending_all_dirty_) {
                      const std::optional<int32_t> floor_z = (z >= 0 && z < MAP_LAYERS) ? std::optional<int32_t> {z} : std::nullopt;
                      pending_dirty_regions_.push_back(DirtyRegion {min_cx, min_cy, max_cx, max_cy, floor_z});
                  }
              });
        invalidateAll();
    }

    void MissingGroundOverlayCache::detachTracker() noexcept
    {
        if (const auto guard = tracker_lifetime_.lock(); guard && guard->tracker != nullptr && listener_id_ != 0) {
            guard->tracker->removeRegionListener(listener_id_);
        }
        tracker_lifetime_.reset();
        listener_id_ = 0;
    }

    void MissingGroundOverlayCache::advanceFrame(int current_floor)
    {
        ++current_frame_;
        if (chunks_.empty()) {
            return;
        }
        if ((current_frame_ - last_prune_frame_) >= PRUNE_INTERVAL_FRAMES || chunks_.size() > MAX_CACHED_CHUNKS) {
            last_prune_frame_ = current_frame_;
            prune(current_floor);
        }
    }

    void MissingGroundOverlayCache::syncDirtyState(
        SpatialChangeTracker& tracker, const BaseMap& map, int map_width, int map_height, bool overlay_enabled
    )
    {
        if (!overlay_enabled) {
            enabled_ = false;
            pending_all_dirty_ = false;
            pending_dirty_regions_.clear();
            if (!chunks_.empty()) {
                clear();
            }
            return;
        }

        const auto current_guard = tracker_lifetime_.lock();
        const bool tracker_changed = (!current_guard || current_guard->tracker != &tracker);
        const bool map_changed = (tracked_map_ != &map || tracked_map_width_ != map_width || tracked_map_height_ != map_height);

        if (tracker_changed) {
            attachTracker(tracker);
        }

        const bool newly_enabled = !enabled_;
        enabled_ = true;
        tracked_map_ = &map;
        tracked_map_width_ = map_width;
        tracked_map_height_ = map_height;

        if (newly_enabled || map_changed || pending_all_dirty_ || tracker.isAllDirty()) {
            invalidateAll();
            pending_all_dirty_ = false;
            pending_dirty_regions_.clear();
            return;
        }

        if (pending_dirty_regions_.empty()) {
            return;
        }

        if (!chunks_.empty()) {
            for (const DirtyRegion& region : pending_dirty_regions_) {
                applyDirtyRegion(region);
            }
        }
        pending_dirty_regions_.clear();
    }

    void MissingGroundOverlayCache::applyDirtyRegion(const DirtyRegion& region) noexcept
    {
        const int32_t min_cx = region.min_cx - 1;
        const int32_t max_cx = region.max_cx + 1;
        const int32_t min_cy = region.min_cy - 1;
        const int32_t max_cy = region.max_cy + 1;

        const uint64_t width = static_cast<uint64_t>(static_cast<int64_t>(max_cx) - min_cx + 1);
        const uint64_t height = static_cast<uint64_t>(static_cast<int64_t>(max_cy) - min_cy + 1);
        const bool single_floor = region.floor_z.has_value();
        const uint64_t floor_span = single_floor ? 1ull : static_cast<uint64_t>(MAP_LAYERS);

        if (width <= 128 && height <= 128 && (width * height * floor_span) <= chunks_.size()) {
            const int32_t z_start = single_floor ? *region.floor_z : 0;
            const int32_t z_end = single_floor ? *region.floor_z : (MAP_LAYERS - 1);
            for (int32_t z = z_start; z <= z_end; ++z) {
                for (int32_t cy = min_cy; cy <= max_cy; ++cy) {
                    for (int32_t cx = min_cx; cx <= max_cx; ++cx) {
                        if (const auto it = chunks_.find(ChunkCoord {cx, cy, z}); it != chunks_.end()) {
                            it->second.is_valid = false;
                        }
                    }
                }
            }
            return;
        }

        for (auto& [coord, chunk] : chunks_) {
            if (single_floor && coord.z != *region.floor_z) {
                continue;
            }
            if (coord.cx >= min_cx && coord.cx <= max_cx && coord.cy >= min_cy && coord.cy <= max_cy) {
                chunk.is_valid = false;
            }
        }
    }

    const CachedMissingGroundChunk&
    MissingGroundOverlayCache::getOrEvaluateChunk(int32_t cx, int32_t cy, int32_t z, const BaseMap& map, int map_width, int map_height)
    {
        const ChunkCoord coord {cx, cy, z};
        auto [it, inserted] = chunks_.try_emplace(coord);
        CachedMissingGroundChunk& chunk = it->second;
        if (inserted) {
            chunk.coord = coord;
        }
        chunk.last_accessed_frame = current_frame_;
        if (!chunk.is_valid) {
            chunk.has_any = MissingGround::EvaluateChunk(map, cx, cy, z, map_width, map_height, chunk.mask.words, scratch_roles_);
            chunk.is_valid = true;
        }
        return chunk;
    }

    void MissingGroundOverlayCache::invalidateAll() noexcept
    {
        for (auto& [coord, chunk] : chunks_) {
            chunk.is_valid = false;
        }
    }

    void MissingGroundOverlayCache::clear() noexcept
    {
        chunks_.clear();
        scratch_roles_.clear();
        eviction_candidates_.clear();
        tracked_map_ = nullptr;
        tracked_map_width_.reset();
        tracked_map_height_.reset();
    }

    void MissingGroundOverlayCache::prune(int current_floor)
    {
        const bool is_surface_view = (current_floor <= GROUND_LAYER);

        for (auto it = chunks_.begin(); it != chunks_.end();) {
            const auto& [coord, chunk] = *it;
            const uint64_t age = current_frame_ - chunk.last_accessed_frame;
            const bool is_far_floor
                = is_surface_view ? (coord.z > GROUND_LAYER + 2) : (coord.z <= GROUND_LAYER || std::abs(coord.z - current_floor) > 2);

            if (!chunk.has_any && is_far_floor && age > FAR_FLOOR_FRAME_THRESHOLD) {
                it = chunks_.erase(it);
                continue;
            }
            ++it;
        }

        if (chunks_.size() > MAX_CACHED_CHUNKS) {
            evictOldest(chunks_.size() - TARGET_CACHED_CHUNKS);
        }
    }

    void MissingGroundOverlayCache::evictOldest(size_t count_to_remove)
    {
        if (count_to_remove == 0 || chunks_.empty()) {
            return;
        }

        eviction_candidates_.clear();
        if (eviction_candidates_.capacity() < chunks_.size()) {
            eviction_candidates_.reserve(chunks_.size());
        }
        for (const auto& [coord, chunk] : chunks_) {
            if (chunk.last_accessed_frame < current_frame_) {
                eviction_candidates_.emplace_back(chunk.last_accessed_frame, coord);
            }
        }
        if (eviction_candidates_.empty()) {
            return;
        }

        const size_t num_evict = std::min(count_to_remove, eviction_candidates_.size());
        std::partial_sort(
            eviction_candidates_.begin(), eviction_candidates_.begin() + num_evict, eviction_candidates_.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; }
        );
        for (size_t i = 0; i < num_evict; ++i) {
            chunks_.erase(eviction_candidates_[i].second);
        }
    }

} // namespace rme::rendering
