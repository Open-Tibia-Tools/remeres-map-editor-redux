#ifndef RME_RENDERING_CORE_MISSING_GROUND_OVERLAY_CACHE_H_
#define RME_RENDERING_CORE_MISSING_GROUND_OVERLAY_CACHE_H_

#include "map/missing_ground.h"
#include "map/spatial_change_tracker.h"
#include "rendering/core/chunk_tile_mask.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

class BaseMap;

namespace rme::rendering {

    struct MissingGroundOverlayCacheTestAccess;

    struct CachedMissingGroundChunk {
        ChunkCoord coord {};
        ChunkTileMask mask {};
        uint64_t last_accessed_frame = 0;
        bool is_valid = false;
        bool has_any = false;
    };

    /**
     * Spatial per-chunk cache of 16x16 missing-ground overlay bitmasks.
     * Observes SpatialChangeTracker with O(1) region notifications, invalidating a 1-chunk halo
     * around edited regions on floor z without touching ChunkCacheManager VBOs.
     */
    class MissingGroundOverlayCache {
    public:
        static constexpr size_t MAX_CACHED_CHUNKS = 32768;
        static constexpr size_t TARGET_CACHED_CHUNKS = 24576;
        static constexpr uint64_t FAR_FLOOR_FRAME_THRESHOLD = 60;
        static constexpr uint64_t PRUNE_INTERVAL_FRAMES = 120;

        MissingGroundOverlayCache() = default;
        ~MissingGroundOverlayCache();

        MissingGroundOverlayCache(const MissingGroundOverlayCache&) = delete;
        MissingGroundOverlayCache& operator=(const MissingGroundOverlayCache&) = delete;
        MissingGroundOverlayCache(MissingGroundOverlayCache&&) = delete;
        MissingGroundOverlayCache& operator=(MissingGroundOverlayCache&&) = delete;

        void advanceFrame(int current_floor);

        void syncDirtyState(SpatialChangeTracker& tracker, const BaseMap& map, int map_width, int map_height, bool overlay_enabled);

        [[nodiscard]] const CachedMissingGroundChunk&
        getOrEvaluateChunk(int32_t cx, int32_t cy, int32_t z, const BaseMap& map, int map_width, int map_height);

        void invalidateAll() noexcept;
        void clear() noexcept;

    private:
        friend struct MissingGroundOverlayCacheTestAccess;

        struct DirtyRegion {
            int32_t min_cx = 0;
            int32_t min_cy = 0;
            int32_t max_cx = 0;
            int32_t max_cy = 0;
            std::optional<int32_t> floor_z;
        };

        void attachTracker(SpatialChangeTracker& tracker);
        void detachTracker() noexcept;
        void applyDirtyRegion(const DirtyRegion& region) noexcept;
        void prune(int current_floor);
        void evictOldest(size_t count_to_remove);

        std::unordered_map<ChunkCoord, CachedMissingGroundChunk, ChunkCoordHash> chunks_;
        std::vector<MissingGround::RoleFlags> scratch_roles_;
        std::vector<DirtyRegion> pending_dirty_regions_;
        std::vector<std::pair<uint64_t, ChunkCoord>> eviction_candidates_;

        std::weak_ptr<const SpatialChangeTracker::LifetimeToken> tracker_lifetime_;
        uint32_t listener_id_ = 0;
        const BaseMap* tracked_map_ = nullptr;
        std::optional<int> tracked_map_width_;
        std::optional<int> tracked_map_height_;
        uint64_t current_frame_ = 0;
        uint64_t last_prune_frame_ = 0;
        bool enabled_ = false;
        bool pending_all_dirty_ = false;
    };

} // namespace rme::rendering

#endif // RME_RENDERING_CORE_MISSING_GROUND_OVERLAY_CACHE_H_
