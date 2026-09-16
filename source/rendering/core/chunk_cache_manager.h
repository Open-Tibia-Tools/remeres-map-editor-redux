//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_CHUNK_CACHE_MANAGER_H_
#define RME_RENDERING_CORE_CHUNK_CACHE_MANAGER_H_

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "rendering/core/chunk_mega_buffer.h"
#include "rendering/core/multi_draw_indirect_renderer.h"
#include "rendering/core/shader_program.h"
#include "map/spatial_change_tracker.h"
#include <unordered_map>
#include <vector>

class Map;
class AtlasManager;
class SpriteBatch;
class TileRenderer;
struct RenderFrameContext;
struct RenderView;
struct DrawingOptions;

struct DynamicTileInfo {
	uint8_t rel_x = 0;
	uint8_t rel_y = 0;
};

struct CachedChunk {
	ChunkCoord coord;
	SlabSlice slice;
	uint64_t last_accessed_frame = 0;
	bool is_dirty = true;
	bool is_empty = false;
	std::vector<DynamicTileInfo> dynamic_tiles;
};

/**
 * High-performance Chunk Cache Manager.
 *
 * Implements persistent mega-buffer slab allocation for cached 16x16 macro-chunks,
 * dynamic MDI (MultiDrawIndirect) submission, shader SSBO indirection,
 * and floor-aware smart eviction.
 */
class ChunkCacheManager {
public:
	static constexpr int CHUNK_SIZE = 16;
	static constexpr uint64_t EVICTION_FRAME_THRESHOLD = 300;
	static constexpr uint64_t PRUNE_INTERVAL_FRAMES = 120;

	ChunkCacheManager();
	~ChunkCacheManager();

	ChunkCacheManager(const ChunkCacheManager&) = delete;
	ChunkCacheManager& operator=(const ChunkCacheManager&) = delete;
	ChunkCacheManager(ChunkCacheManager&&) noexcept = default;
	ChunkCacheManager& operator=(ChunkCacheManager&&) noexcept = default;

	bool initialize();
	void release();

	/**
	 * Synchronize dirty state from the map's SpatialChangeTracker.
	 */
	void updateDirtyState(SpatialChangeTracker& change_tracker);

	/**
	 * Invalidate all cached chunks across all floors.
	 */
	void invalidateAll();

	/**
	 * Invalidate a specific chunk coordinate.
	 */
	void invalidateChunk(int32_t cx, int32_t cy, int32_t z);

	/**
	 * Render all cached static geometry for visible chunks on floor map_z.
	 */
	void renderFloor(
		int map_z,
		const Map& map,
		const RenderFrameContext& ctx,
		const glm::mat4& projection,
		const AtlasManager& atlas
	);

	/**
	 * Render dynamic overlays (animated items, creatures, markers) for visible chunks on floor map_z.
	 * Only tiles that contain actual dynamic elements are visited.
	 */
	void renderDynamicOverlays(
		int map_z,
		const Map& map,
		const RenderFrameContext& ctx,
		SpriteBatch& sprite_batch,
		const TileRenderer& tile_renderer
	);

	/**
	 * Evict distant/stale chunks outside the active floor range.
	 */
	void prune(int current_floor);

	[[nodiscard]] size_t getCachedChunkCount() const noexcept {
		return cached_chunks_.size();
	}
	[[nodiscard]] bool isValid() const noexcept {
		return mega_buffer_.isValid() && shader_initialized_;
	}

private:
	void bakeChunk(CachedChunk& chunk, const Map& map, const RenderFrameContext& ctx);
	CachedChunk& getOrCreateChunk(const ChunkCoord& coord);

	ChunkMegaBuffer mega_buffer_;
	MultiDrawIndirectRenderer mdi_renderer_;
	ShaderProgram shader_;
	bool shader_initialized_ = false;

	std::unordered_map<ChunkCoord, CachedChunk, ChunkCoordHash> cached_chunks_;
	std::vector<TileInstance> bake_buffer_;
	uint64_t current_frame_ = 0;
};

#endif
