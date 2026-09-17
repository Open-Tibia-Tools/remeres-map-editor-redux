//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_LIGHT_CACHE_H_
#define RME_RENDERING_CORE_LIGHT_CACHE_H_

#include "rendering/core/light_types.h"
#include "rendering/core/light_gatherer.h"
#include "map/spatial_change_tracker.h"
#include <unordered_map>
#include <unordered_set>
#include <array>

class BaseMap;
class GraphicManager;

namespace rme::lighting {

inline constexpr float constexpr_sqrt(float x) {
	if (x <= 0.0f) return 0.0f;
	float curr = x;
	float prev = 0.0f;
	for (int i = 0; i < 20; ++i) {
		if (curr == prev) break;
		prev = curr;
		curr = 0.5f * (curr + x / curr);
	}
	return curr;
}

inline constexpr auto generateDistanceTable() {
	std::array<float, 513> table {};
	for (size_t i = 0; i < table.size(); ++i) {
		table[i] = constexpr_sqrt(static_cast<float>(i));
	}
	return table;
}

inline constexpr auto s_distance_table = generateDistanceTable();

class LightCache {
public:
	static constexpr uint64_t EVICTION_FRAME_THRESHOLD = 600;
	static constexpr uint64_t PRUNE_INTERVAL_FRAMES = 120;

	LightCache();
	~LightCache();

	LightCache(const LightCache&) = delete;
	LightCache& operator=(const LightCache&) = delete;
	LightCache(LightCache&&) = delete;
	LightCache& operator=(LightCache&&) = delete;

	/**
	 * Get or create a chunk lightmap. If dirty or invalid, bakes it immediately.
	 */
	CachedLightChunk& getOrBakeChunk(
		int32_t cx, int32_t cy, int32_t z,
		const BaseMap& map,
		int32_t start_floor, int32_t superend_floor,
		const LightConfig& config,
		GraphicManager& gfx,
		uint64_t current_frame
	);

	/**
	 * Synchronize dirty state from map's SpatialChangeTracker.
	 * Propagates 3x3 neighbor chunk invalidation.
	 * Returns true if any chunks were invalidated.
	 */
	bool updateDirtyState(SpatialChangeTracker& tracker);

	/**
	 * Invalidate all cached chunks (e.g. ambient light changed, map reload).
	 */
	void invalidateAll() noexcept;

	/**
	 * Invalidate a specific tile coordinate and its 3x3 neighbor chunks.
	 */
	void invalidateTile(int32_t x, int32_t y, int32_t z);

	/**
	 * Prune stale chunks outside active floor.
	 */
	void prune(int current_floor, uint64_t current_frame);

	[[nodiscard]] size_t getCachedChunkCount() const noexcept {
		return chunks_.size();
	}

private:
	void bakeChunk(
		CachedLightChunk& chunk,
		const BaseMap& map,
		int32_t start_floor, int32_t superend_floor,
		const LightConfig& config,
		GraphicManager& gfx
	);

	std::unordered_map<ChunkCoord, CachedLightChunk, ChunkCoordHash> chunks_;
	LightGatherer gatherer_;
	std::array<bool, CHUNK_PIXELS> ground_occlusion_mask_ {};

	SpatialChangeTracker* tracked_tracker_ = nullptr;
	uint32_t listener_id_ = 0;
	std::unordered_set<ChunkCoord, ChunkCoordHash> pending_dirty_chunks_;
	bool pending_all_dirty_ = false;
};

} // namespace rme::lighting

#endif // RME_RENDERING_CORE_LIGHT_CACHE_H_
