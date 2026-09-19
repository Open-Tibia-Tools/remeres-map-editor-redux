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
class GameSprite;
struct RenderFrameContext;
struct HardwareBudget;

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
	bool has_animated_terrain = false;
	const GameSprite* sample_animated_sprite = nullptr;
	int last_baked_frame = -1;
	long last_baked_anim_time = 0;
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
		has_animated_terrain(other.has_animated_terrain),
		sample_animated_sprite(other.sample_animated_sprite),
		last_baked_frame(other.last_baked_frame),
		last_baked_anim_time(other.last_baked_anim_time),
		dynamic_tiles(std::move(other.dynamic_tiles)) {
		other.vbo = 0;
		other.vbo_capacity = 0;
		other.instance_count = 0;
		other.has_animated_terrain = false;
		other.sample_animated_sprite = nullptr;
		other.last_baked_frame = -1;
		other.last_baked_anim_time = 0;
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
			has_animated_terrain = other.has_animated_terrain;
			sample_animated_sprite = other.sample_animated_sprite;
			last_baked_frame = other.last_baked_frame;
			last_baked_anim_time = other.last_baked_anim_time;
			dynamic_tiles = std::move(other.dynamic_tiles);
			other.vbo = 0;
			other.vbo_capacity = 0;
			other.instance_count = 0;
			other.has_animated_terrain = false;
			other.sample_animated_sprite = nullptr;
			other.last_baked_frame = -1;
			other.last_baked_anim_time = 0;
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
 * instanced rendering, shader Texture Buffer (TBO) indirection via SpriteAtlasLUT,
 * and floor-aware smart eviction.
 */
class ChunkCacheManager {
public:
	static constexpr int CHUNK_SIZE = 16;
	static constexpr uint64_t PRUNE_INTERVAL_FRAMES = 120;         // 2.0 seconds at 60 FPS
	static constexpr int VIEWPORT_MARGIN_CHUNKS = 32;              // 512 tiles

	ChunkCacheManager();
	~ChunkCacheManager();

	ChunkCacheManager(const ChunkCacheManager&) = delete;
	ChunkCacheManager& operator=(const ChunkCacheManager&) = delete;
	ChunkCacheManager(ChunkCacheManager&&) noexcept = default;
	ChunkCacheManager& operator=(ChunkCacheManager&&) noexcept = default;

	bool initialize();
	void release();

	[[nodiscard]] size_t getMaxCachedChunks() const noexcept { return max_cached_chunks_; }
	[[nodiscard]] size_t getTargetCachedChunks() const noexcept { return target_cached_chunks_; }
	[[nodiscard]] uint64_t getFarFloorThreshold() const noexcept { return far_floor_frame_threshold_; }
	void applyBudget(const HardwareBudget& budget);

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
		AtlasManager& atlas
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
	 * Advance frame counter and trigger periodic prune. Called once per frame by the renderer.
	 */
	void advanceFrame(int current_floor);

	/**
	 * Evict distant/stale chunks outside the active floor range or viewport margin.
	 */
	void prune(int current_floor, int min_cx = 0, int max_cx = 0, int min_cy = 0, int max_cy = 0, bool has_bounds = false);

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
	void evictOldest(size_t count_to_remove);

	GLuint vao_ = 0;
	ShaderProgram shader_;
	bool shader_initialized_ = false;

	std::unordered_map<ChunkCoord, CachedChunk, ChunkCoordHash> cached_chunks_;
	std::vector<TileInstance> bake_buffer_;
	uint64_t current_frame_ = 0;

	std::vector<CachedChunk*> active_visible_chunks_;
	int active_floor_ = -1;

	size_t max_cached_chunks_ = 65536;
	size_t target_cached_chunks_ = 49152;
	uint64_t far_floor_frame_threshold_ = 60;
};

#endif
