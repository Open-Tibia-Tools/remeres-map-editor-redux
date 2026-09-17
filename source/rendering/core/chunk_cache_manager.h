//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_CHUNK_CACHE_MANAGER_H_
#define RME_RENDERING_CORE_CHUNK_CACHE_MANAGER_H_

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "rendering/core/shader_program.h"
#include "rendering/core/tile_instance.h"
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
	GLuint vbo = 0;
	size_t vbo_capacity = 0; // in bytes
	uint32_t instance_count = 0;
	uint64_t last_accessed_frame = 0;
	bool is_dirty = true;
	bool is_empty = false;
	std::vector<DynamicTileInfo> dynamic_tiles;

	CachedChunk() = default;
	~CachedChunk() {
		if (vbo != 0) {
			glDeleteBuffers(1, &vbo);
			vbo = 0;
		}
	}

	CachedChunk(CachedChunk&& other) noexcept :
		coord(other.coord),
		vbo(other.vbo),
		vbo_capacity(other.vbo_capacity),
		instance_count(other.instance_count),
		last_accessed_frame(other.last_accessed_frame),
		is_dirty(other.is_dirty),
		is_empty(other.is_empty),
		dynamic_tiles(std::move(other.dynamic_tiles)) {
		other.vbo = 0;
		other.vbo_capacity = 0;
		other.instance_count = 0;
	}

	CachedChunk& operator=(CachedChunk&& other) noexcept {
		if (this != &other) {
			if (vbo != 0) {
				glDeleteBuffers(1, &vbo);
			}
			coord = other.coord;
			vbo = other.vbo;
			vbo_capacity = other.vbo_capacity;
			instance_count = other.instance_count;
			last_accessed_frame = other.last_accessed_frame;
			is_dirty = other.is_dirty;
			is_empty = other.is_empty;
			dynamic_tiles = std::move(other.dynamic_tiles);
			other.vbo = 0;
			other.vbo_capacity = 0;
			other.instance_count = 0;
		}
		return *this;
	}

	CachedChunk(const CachedChunk&) = delete;
	CachedChunk& operator=(const CachedChunk&) = delete;
};

/**
 * High-performance Chunk Cache Manager.
 *
 * Implements per-chunk VBO caching (parity with Imgui Map Editor architecture),
 * instanced rendering, shader SSBO indirection via SpriteAtlasLUT,
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
		return vao_ != 0 && shader_initialized_;
	}

private:
	void bakeChunk(CachedChunk& chunk, const Map& map, const RenderFrameContext& ctx);
	void uploadChunk(CachedChunk& chunk, const std::vector<TileInstance>& instances);
	CachedChunk& getOrCreateChunk(const ChunkCoord& coord);

	GLuint vao_ = 0;
	ShaderProgram shader_;
	bool shader_initialized_ = false;

	std::unordered_map<ChunkCoord, CachedChunk, ChunkCoordHash> cached_chunks_;
	std::vector<TileInstance> bake_buffer_;
	uint64_t current_frame_ = 0;
};

#endif
