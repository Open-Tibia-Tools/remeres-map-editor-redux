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
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>

class GameSprite;
struct RenderFrameContext;
struct HardwareBudget;

struct AnimatedSpriteFrame {
	GameSprite* sprite = nullptr;
	int frame = 0;
};

// One animated sprite occurrence: a contiguous run of instances in the chunk buffer.
struct AnimatedSpriteRun {
	uint32_t first_instance = 0;
	uint32_t instance_count = 0;
	uint32_t sprite_slot = 0; // index into CachedChunk::animated_sprites
	int32_t draw_x = 0;
	int32_t draw_y = 0;
	int16_t pattern_x = 0;
	int16_t pattern_y = 0;
	int16_t pattern_z = 0;
	int16_t subtype = -1;
};

struct CachedChunk {
	ChunkCoord coord;
	GLuint vbo = 0;
	size_t vbo_capacity = 0; // in bytes
	uint32_t instance_count = 0;
	uint32_t terrain_instance_count = 0;
	uint32_t item_instance_count = 0;
	uint64_t last_accessed_frame = 0;
	bool is_dirty = true;
	bool is_empty = false;
	// False when the chunk was baked at a zoom where items are not drawn, so the
	// item range is missing and the chunk must be re-baked if items come back.
	bool baked_with_items = true;
	// False when the last bake had to skip sprites that were not uploaded yet. Such a chunk
	// is re-baked so the sprites appear as soon as the sprite preloader has loaded them.
	bool is_complete = true;
	// Bounded retries: a sprite that can never be loaded must not cause an endless re-bake
	// loop, and the last attempt is allowed to load inline so no sprite can stay missing.
	uint32_t rebake_attempts = 0;
	// Steady-clock deadline for the next retry. Time based, not frame based, so the recovery
	// does not depend on the repaint cadence.
	uint64_t rebake_at_ms = 0;
	bool force_load = false;
	// Average duration of one frame of the creature's sprite animation in the chunk, or 0
	// when the chunk holds no creature. Creatures animate, so a visible chunk that has one
	// keeps the animation repaint loop alive.
	uint16_t creature_animation_step_ms = 0;
	// Shortest average frame duration among the chunk's animated sprites, or 0 when none.
	// Constant per animator, so it is resolved once per bake instead of scanning every
	// AnimatedSpriteFrame on every frame to pace the repaint loop.
	uint16_t animated_step_ms = 0;
	std::vector<AnimatedSpriteFrame> animated_sprites;
	std::vector<AnimatedSpriteRun> animated_runs;
	// CPU copy of the instance buffer, kept only while animated_runs is non-empty so frames can be patched in place.
	std::vector<TileInstance> animated_shadow;
	std::array<uint64_t, 4> dynamic_tile_mask = {0, 0, 0, 0};

	[[nodiscard]] bool hasDynamicTiles() const noexcept {
		return (dynamic_tile_mask[0] | dynamic_tile_mask[1] | dynamic_tile_mask[2] | dynamic_tile_mask[3]) != 0;
	}

	void clearDynamicTiles() noexcept {
		dynamic_tile_mask.fill(0);
	}

	void markDynamicTile(int tx, int ty) noexcept {
		const int idx = ty * 16 + tx;
		dynamic_tile_mask[idx >> 6] |= (uint64_t{1} << (idx & 63));
	}

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
		terrain_instance_count(other.terrain_instance_count),
		item_instance_count(other.item_instance_count),
		last_accessed_frame(other.last_accessed_frame),
		is_dirty(other.is_dirty),
		is_empty(other.is_empty),
		baked_with_items(other.baked_with_items),
		is_complete(other.is_complete),
		rebake_attempts(other.rebake_attempts),
		rebake_at_ms(other.rebake_at_ms),
		force_load(other.force_load),
		creature_animation_step_ms(other.creature_animation_step_ms),
		animated_step_ms(other.animated_step_ms),
		animated_sprites(std::move(other.animated_sprites)),
		animated_runs(std::move(other.animated_runs)),
		animated_shadow(std::move(other.animated_shadow)),
		dynamic_tile_mask(other.dynamic_tile_mask) {
		other.vbo = 0;
		other.vbo_capacity = 0;
		other.instance_count = 0;
		other.terrain_instance_count = 0;
		other.item_instance_count = 0;
		other.dynamic_tile_mask.fill(0);
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
			terrain_instance_count = other.terrain_instance_count;
			item_instance_count = other.item_instance_count;
			last_accessed_frame = other.last_accessed_frame;
			is_dirty = other.is_dirty;
			is_empty = other.is_empty;
			baked_with_items = other.baked_with_items;
			is_complete = other.is_complete;
			rebake_attempts = other.rebake_attempts;
			rebake_at_ms = other.rebake_at_ms;
			force_load = other.force_load;
			creature_animation_step_ms = other.creature_animation_step_ms;
			animated_step_ms = other.animated_step_ms;
			animated_sprites = std::move(other.animated_sprites);
			animated_runs = std::move(other.animated_runs);
			animated_shadow = std::move(other.animated_shadow);
			dynamic_tile_mask = other.dynamic_tile_mask;
			other.vbo = 0;
			other.vbo_capacity = 0;
			other.instance_count = 0;
			other.terrain_instance_count = 0;
			other.item_instance_count = 0;
			other.dynamic_tile_mask.fill(0);
		}
		return *this;
	}

	CachedChunk(const CachedChunk&) = delete;
	CachedChunk& operator=(const CachedChunk&) = delete;
};

// Aggregated over one PRUNE_INTERVAL_FRAMES window and logged by advanceFrame().
struct ChunkRenderStats {
	uint32_t frames = 0;
	double frame_ms_total = 0.0;
	double frame_ms_max = 0.0;
	double bake_ms_total = 0.0;
	double bake_ms_max = 0.0;
	uint32_t baked_new = 0;
	uint32_t baked_edit = 0;
	uint32_t baked_anim = 0;
	uint32_t baked_deferred = 0;
	uint32_t anim_patches = 0;
	uint64_t draws = 0;
	uint64_t instances = 0;
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
	// Per-frame bake time budget. A pan or zoom-out can make dozens of chunks
	// dirty at once; baking them all in one frame caused multi-hundred-ms
	// hitches. Chunks left over stay dirty and drain on following frames while
	// keeping their previous VBO content.
	static constexpr double MAX_BAKE_MS_PER_FRAME = 8.0;
	// A chunk whose bake skipped not-yet-loaded sprites is retried this many times, then one
	// final bake with inline loading is allowed so a sprite can never stay missing.
	static constexpr uint32_t MAX_INCOMPLETE_REBAKES = 2;
	// Milliseconds to wait before retrying an incomplete chunk: enough for the preloader's
	// workers to decode the sheets they were asked for, and independent of the frame rate so
	// the hole cannot linger when the editor is repainting slowly (or not at all).
	static constexpr uint64_t INCOMPLETE_REBAKE_DELAY_MS = 60;
	// Budget for finishing the chunks that ended a bake incomplete, applied once per frame before
	// anything is drawn: base plus a per-chunk allowance, capped. The loop stops as soon as the
	// preloader has no work left, so a pan (one or two new chunks) costs almost nothing while a
	// relocation or a map load pays what the sprites actually need.
	static constexpr uint64_t INCOMPLETE_DRAIN_BASE_MS = 12;
	static constexpr uint64_t INCOMPLETE_DRAIN_PER_CHUNK_MS = 4;
	static constexpr uint64_t INCOMPLETE_DRAIN_CAP_MS = 220;
	// More incomplete visible chunks than this in one frame means a whole screen of new content
	// arrived at once (a relocation, or the first frame of a map) rather than a pan revealing a
	// couple of chunks, and is handled as a burst.
	static constexpr size_t INCOMPLETE_BURST_CHUNKS = 8;
	// On the first frame of a freshly opened map every visible chunk is new and their sprites are
	// the ones a player would see first: it is worth a short loading pause to have the map appear
	// complete (instead of filling in, most visibly on water and its borders) - the same work the
	// editor used to do inline, but once and without flicker.
	static constexpr uint64_t COLD_START_DRAIN_CAP_MS = 3000;

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
	 * Render static terrain & borders for visible chunks on floor map_z.
	 * Bakes dirty chunks and populates active_visible_chunks_.
	 */
	void renderFloorTerrain(
		int map_z,
		const Map& map,
		const RenderFrameContext& ctx,
		const glm::mat4& projection,
		AtlasManager& atlas
	);

	/**
	 * Render static items & structures for visible chunks on floor map_z.
	 * Uses active_visible_chunks_ gathered during renderFloorTerrain.
	 */
	void renderFloorItems(
		int map_z,
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

	// True when the last rendered frame drew something that animates with time (sprite frame
	// runs or creatures inside the animation LOD range). The editor only needs to keep
	// repainting while this holds; a static view can sit still instead of redrawing at 60 Hz.
	[[nodiscard]] bool hasVisibleAnimation() const noexcept {
		return visible_animation_step_ms_ != 0;
	}

	// Average duration in milliseconds of the shortest animation frame on screen, i.e. how
	// often the view can actually change. Zero when nothing on screen animates. The repaint
	// loop uses this to repaint at the animation's own pace instead of at 60 Hz.
	[[nodiscard]] uint32_t visibleAnimationStepMs() const noexcept {
		return visible_animation_step_ms_;
	}

	// Advances the graphics clock every frame and drains the preloader. Called from the canvas.
	[[nodiscard]] static uint64_t steadyNowMs() noexcept {
		return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	// True after a frame that drew chunks still waiting for sprites. The canvas then keeps the
	// repaint loop alive so the retry can re-bake them even when nothing animates.
	[[nodiscard]] bool hasIncompleteChunks() const noexcept {
		return incomplete_chunks_ != 0;
	}

private:
	void bakeChunk(CachedChunk& chunk, const Map& map, const RenderFrameContext& ctx, bool allow_upload);

	uint32_t incomplete_chunks_ = 0;
	// Set by advanceFrame: true when the cache was empty at the start of this frame, i.e. the
	// first frame of a freshly opened map.
	bool cold_start_frame_ = false;
	// Visible chunks whose last bake skipped not-yet-loaded sprites; finished after the bake pass.
	std::vector<CachedChunk*> incomplete_visible_;
	uint32_t visible_animation_step_ms_ = 0;
	void uploadChunk(CachedChunk& chunk, const std::vector<TileInstance>& instances);
	// Returns false when the chunk needs a full re-bake instead.
	bool refreshAnimatedFrames(CachedChunk& chunk, long elapsed_time);
	CachedChunk& getOrCreateChunk(const ChunkCoord& coord);
	void evictOldest(size_t count_to_remove);
	void recordFrameStats();

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

	ChunkRenderStats stats_;
	double frame_bake_ms_ = 0.0;
	std::chrono::steady_clock::time_point last_frame_start_ {};
};

#endif
