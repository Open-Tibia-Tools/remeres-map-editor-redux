//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/chunk_cache_manager.h"
#include "rendering/core/hardware_profile.h"
#include "app/definitions.h"
#include "rendering/core/atlas_manager.h"
#include "rendering/core/graphics.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/render_frame_context.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/shared_geometry.h"
#include "rendering/drawers/tiles/tile_renderer.h"
#include "rendering/drawers/tiles/tile_color_calculator.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/creature.h"
#include "game/outfit.h"
#include "rendering/utilities/pattern_calculator.h"
#include "rendering/core/sprite_preloader.h"
#include "util/phase_profiler.h"
#include <thread>
#include <bitset>
#include "rendering/shaders/chunk_shader.h"
#include "rendering/indicators/technical_item_registry.h"
#include "rendering/core/render_depth.h"
#include <spdlog/spdlog.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace {

	using ChunkFloors = std::array<std::array<const Floor*, 4>, 4>;

	// A 16x16 chunk always lies inside one 64x64 grid cell, so a single cell lookup resolves its 4x4 nodes.
	bool gatherChunkFloors(const Map& map, int32_t cx, int32_t cy, int32_t z, ChunkFloors& floors) {
		floors = {};
		if (z < 0 || z >= MAP_LAYERS) {
			return false;
		}
		const auto& grid = map.getGrid();
		const auto* cell = grid.getCell(grid.findCellIndex(SpatialHashGrid::makeKeyFromCell(cx >> 2, cy >> 2)));
		if (!cell) {
			return false;
		}

		const int chunk_ix = cx & 3;
		const int chunk_iy = cy & 3;
		if ((cell->floor_chunk_mask[z] & static_cast<uint16_t>(1u << ((chunk_iy << 2) | chunk_ix))) == 0) {
			return false;
		}

		bool any_floor = false;
		for (int ny = 0; ny < 4; ++ny) {
			const int row_base = ((chunk_iy << 2) + ny) << 4;
			for (int nx = 0; nx < 4; ++nx) {
				const MapNode* nd = cell->nodes[row_base + (chunk_ix << 2) + nx].get();
				if (nd) {
					floors[nx][ny] = nd->getFloor(z);
					any_floor = any_floor || floors[nx][ny] != nullptr;
				}
			}
		}
		return any_floor;
	}

	const TileLocation* chunkTileLocation(const ChunkFloors& floors, int tx, int ty) {
		const Floor* fl = floors[tx >> 2][ty >> 2];
		return fl ? &fl->locs[(tx & 3) * 4 + (ty & 3)] : nullptr;
	}

	bool isBakeableRegion(const AtlasRegion* reg) {
		return reg && reg->debug_sprite_id != AtlasRegion::INVALID_SENTINEL;
	}

	// Writes the frame-dependent fields; house_id must already be set.
	void writeRegionGeometry(TileInstance& inst, const AtlasRegion& reg, int draw_x, int draw_y) {
		inst.x = static_cast<float>(draw_x);
		inst.y = static_cast<float>(draw_y);
		const bool is_indicator = GetTileIndicatorType(inst.flags) != 0;
		inst.w = is_indicator ? 32.0f : static_cast<float>(reg.pixel_width);
		inst.h = is_indicator ? 32.0f : static_cast<float>(reg.pixel_height);
		inst.sprite_id = static_cast<float>(reg.debug_sprite_id);
	}

	// Visits the atlas regions of one sprite occurrence in bake order, with their draw positions.
	// With allow_upload=false the bake reports a not-yet-loaded sprite as a null region instead
	// of loading it inline, so asset loading happens on the sprite preloader's workers and never
	// blocks the render thread.
	template <typename Visitor>
	void forEachSpriteRegion(GameSprite* spr, const SpritePatterns& pat, int draw_base_x, int draw_base_y, bool allow_upload, Visitor&& visit) {
		const int raw_subtype = pat.subtype.value_or(-1);
		if (spr->width == 1 && spr->height == 1 && spr->layers == 1) {
			const AtlasRegion* reg = nullptr;
			if (spr->is_simple && !pat.subtype.has_value() && pat.x == 0 && pat.y == 0 && pat.z == 0 && pat.frame == 0) {
				reg = spr->getCachedDefaultRegion();
			}
			if (!reg) {
				reg = spr->getAtlasRegion(0, 0, 0, raw_subtype, pat.x, pat.y, pat.z, pat.frame, allow_upload);
			}
			visit(reg, draw_base_x, draw_base_y);
			return;
		}

		const auto composite_metrics = spr->getPlainLayoutMetrics(raw_subtype, pat.x, pat.y, pat.z, pat.frame);
		int x_offset = 0;
		for (int cx = 0; cx < composite_metrics.num_columns; ++cx) {
			int y_offset = 0;
			for (int cy = 0; cy < composite_metrics.num_rows; ++cy) {
				for (int cf = 0; cf < spr->layers; ++cf) {
					visit(spr->getAtlasRegion(cx, cy, cf, raw_subtype, pat.x, pat.y, pat.z, pat.frame, allow_upload), draw_base_x - x_offset, draw_base_y - y_offset);
				}
				y_offset += composite_metrics.row_heights[cy];
			}
			x_offset += composite_metrics.column_widths[cx];
		}
	}

} // namespace

ChunkCacheManager::ChunkCacheManager() {
	bake_buffer_.reserve(2048);
}

ChunkCacheManager::~ChunkCacheManager() {
	release();
}

bool ChunkCacheManager::initialize() {
	release();

	if (!SharedGeometry::Instance().initialize()) {
		spdlog::error("ChunkCacheManager: Failed to initialize shared geometry");
		return false;
	}

	if (!shader_.Load(rme::rendering::shaders::CHUNK_VERT_SHADER, rme::rendering::shaders::GetChunkFragShader())) {
		spdlog::error("ChunkCacheManager: Failed to compile chunk shader");
		return false;
	}
	shader_initialized_ = true;

	glCreateVertexArrays(1, &vao_);
	if (vao_ == 0) {
		spdlog::error("ChunkCacheManager: Failed to create VAO");
		release();
		return false;
	}

	// Binding 0: Static unit quad geometry
	glVertexArrayVertexBuffer(vao_, 0, SharedGeometry::Instance().getQuadVBO(), 0, 4 * sizeof(float));
	glVertexArrayElementBuffer(vao_, SharedGeometry::Instance().getQuadEBO());

	// Loc 0: Quad pos (vec2)
	glEnableVertexArrayAttrib(vao_, 0);
	glVertexArrayAttribFormat(vao_, 0, 2, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao_, 0, 0);

	// Loc 1: Quad texcoord (vec2)
	glEnableVertexArrayAttrib(vao_, 1);
	glVertexArrayAttribFormat(vao_, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
	glVertexArrayAttribBinding(vao_, 1, 0);

	// Binding 1: Instance data from chunk VBO
	glVertexArrayBindingDivisor(vao_, 1, 1);

	// Loc 2: aRect (vec4: x, y, w, h)
	glEnableVertexArrayAttrib(vao_, 2);
	glVertexArrayAttribFormat(vao_, 2, 4, GL_FLOAT, GL_FALSE, offsetof(TileInstance, x));
	glVertexArrayAttribBinding(vao_, 2, 1);

	// Loc 3: aSpriteId (float)
	glEnableVertexArrayAttrib(vao_, 3);
	glVertexArrayAttribFormat(vao_, 3, 1, GL_FLOAT, GL_FALSE, offsetof(TileInstance, sprite_id));
	glVertexArrayAttribBinding(vao_, 3, 1);

	// Loc 4: aFlags (float)
	glEnableVertexArrayAttrib(vao_, 4);
	glVertexArrayAttribFormat(vao_, 4, 1, GL_FLOAT, GL_FALSE, offsetof(TileInstance, flags));
	glVertexArrayAttribBinding(vao_, 4, 1);

	// Loc 5: aTint (vec4: r, g, b, a)
	glEnableVertexArrayAttrib(vao_, 5);
	glVertexArrayAttribFormat(vao_, 5, 4, GL_FLOAT, GL_FALSE, offsetof(TileInstance, r));
	glVertexArrayAttribBinding(vao_, 5, 1);

	// Loc 6: aHouseId (float)
	glEnableVertexArrayAttrib(vao_, 6);
	glVertexArrayAttribFormat(vao_, 6, 1, GL_FLOAT, GL_FALSE, offsetof(TileInstance, house_id));
	glVertexArrayAttribBinding(vao_, 6, 1);

	// Loc 7: aDepth (float)
	glEnableVertexArrayAttrib(vao_, 7);
	glVertexArrayAttribFormat(vao_, 7, 1, GL_FLOAT, GL_FALSE, offsetof(TileInstance, depth));
	glVertexArrayAttribBinding(vao_, 7, 1);

	applyBudget(HardwareProfileManager::get().getActiveBudget());

	spdlog::info("[ChunkCache] Initialized successfully (VAO: {}, Max capacity: {} chunks, Target: {} chunks)",
		vao_, max_cached_chunks_, target_cached_chunks_);
	return true;
}

void ChunkCacheManager::applyBudget(const HardwareBudget& budget) {
	max_cached_chunks_ = budget.max_cached_chunks;
	target_cached_chunks_ = budget.target_cached_chunks;
	far_floor_frame_threshold_ = budget.far_floor_threshold_frames;

	if (cached_chunks_.size() > target_cached_chunks_) {
		const size_t excess = cached_chunks_.size() - target_cached_chunks_;
		evictOldest(excess);
		spdlog::info("[ChunkCache] Applied budget (max: {}, target: {}): trimmed {} excess chunks | Remaining: {}",
			max_cached_chunks_, target_cached_chunks_, excess, cached_chunks_.size());
	} else {
		spdlog::info("[ChunkCache] Applied budget: max chunks {}, target chunks {}, far floor threshold {} frames",
			max_cached_chunks_, target_cached_chunks_, far_floor_frame_threshold_);
	}
}

void ChunkCacheManager::release() {
	if (vao_ != 0 || !cached_chunks_.empty()) {
		spdlog::info("[ChunkCache] Released (cleared {} cached chunk VBOs, destroyed VAO: {})",
			cached_chunks_.size(), vao_);
	}
	if (vao_ != 0) {
		glDeleteVertexArrays(1, &vao_);
		vao_ = 0;
	}
	cached_chunks_.clear();
	active_visible_chunks_.clear();
	active_floor_ = -1;
	shader_initialized_ = false;
	current_frame_ = 0;
}

void ChunkCacheManager::updateDirtyState(SpatialChangeTracker& change_tracker) {
	if (change_tracker.isAllDirty()) {
		spdlog::info("[ChunkCache] SpatialChangeTracker::isAllDirty() triggered InvalidateAll");
		invalidateAll();
		change_tracker.clearDirty();
		return;
	}

	auto dirty_chunks = change_tracker.takeDirtyChunks();
	if (!dirty_chunks.empty()) {
		spdlog::info("[ChunkCache] SpatialChangeTracker: {} dirty chunk(s) marked for re-bake", dirty_chunks.size());
	}
	for (const auto& coord : dirty_chunks) {
		auto it = cached_chunks_.find(coord);
		if (it != cached_chunks_.end()) {
			it->second.is_dirty = true;
		}
	}
}

void ChunkCacheManager::invalidateAll() {
	spdlog::info("[ChunkCache] InvalidateAll: marked {} cached chunks dirty", cached_chunks_.size());
	for (auto& [coord, chunk] : cached_chunks_) {
		chunk.is_dirty = true;
	}
}

void ChunkCacheManager::invalidateChunk(int32_t cx, int32_t cy, int32_t z) {
	auto it = cached_chunks_.find(ChunkCoord{ cx, cy, z });
	if (it != cached_chunks_.end()) {
		it->second.is_dirty = true;
	}
}

CachedChunk& ChunkCacheManager::getOrCreateChunk(const ChunkCoord& coord) {
	auto it = cached_chunks_.find(coord);
	if (it == cached_chunks_.end()) {
		CachedChunk chunk;
		chunk.coord = coord;
		chunk.is_dirty = true;
		chunk.is_empty = false;
		auto [new_it, _] = cached_chunks_.emplace(coord, std::move(chunk));
		return new_it->second;
	}
	return it->second;
}

void ChunkCacheManager::uploadChunk(CachedChunk& chunk, const std::vector<TileInstance>& instances) {
	if (instances.empty()) {
		chunk.instance_count = 0;
		chunk.terrain_instance_count = 0;
		chunk.item_instance_count = 0;
		chunk.is_empty = true;
		return;
	}

	chunk.is_empty = false;
	chunk.instance_count = static_cast<uint32_t>(instances.size());

	if (chunk.vbo == 0) {
		glCreateBuffers(1, &chunk.vbo);
		chunk.vbo_capacity = 0;
	}

	const size_t required_bytes = instances.size() * sizeof(TileInstance);
	if (required_bytes > chunk.vbo_capacity) {
		glNamedBufferData(chunk.vbo, static_cast<GLsizeiptr>(required_bytes), instances.data(), GL_STATIC_DRAW);
		chunk.vbo_capacity = required_bytes;
	} else {
		glNamedBufferSubData(chunk.vbo, 0, static_cast<GLsizeiptr>(required_bytes), instances.data());
	}
}

void ChunkCacheManager::bakeChunk(CachedChunk& chunk, const Map& map, const RenderFrameContext& ctx, bool allow_upload) {
	// Items are not drawn beyond the LOD cutoff when hide_items_when_zoomed is on, so
	// do not bake them: they would only cost bake time (the dominant burst cost) and
	// VBO memory. The chunk records whether items were baked so it can be re-baked if
	// items become visible again.
	const bool bake_items = !(ctx.view.zoom > 10.0f && ctx.options.hide_items_when_zoomed);
	// Set when the bake had to skip a sprite that is not uploaded yet (only possible while
	// allow_upload is false). Such a chunk is marked incomplete and retried later.
	bool skipped = false;
	// Creatures animate independently of sprite frame runs, so the chunk records the frame
	// step of the creature animation it baked (0 when it holds none).
	uint16_t creature_step_ms = 0;
	bake_buffer_.clear();
	scratch_sync_slots_.clear();
	chunk.clearDynamicTiles();
	chunk.animated_sprites.clear();
	chunk.animated_runs.clear();

	const int32_t base_x = chunk.coord.cx * CHUNK_SIZE;
	const int32_t base_y = chunk.coord.cy * CHUNK_SIZE;
	const int32_t z = chunk.coord.z;

	ChunkFloors floors;
	if (!gatherChunkFloors(map, chunk.coord.cx, chunk.coord.cy, z, floors)) {
		chunk.is_empty = true;
		chunk.is_dirty = false;
		chunk.instance_count = 0;
		chunk.baked_with_items = bake_items;
		chunk.is_complete = true;
		chunk.rebake_attempts = 0;
		chunk.force_load = false;
		chunk.animated_sprites.clear();
		chunk.animated_runs.clear();
		chunk.animated_step_ms = 0;
		chunk.animated_shadow.clear();
		return;
	}

	auto pushRegionInstance = [&](const AtlasRegion* reg, int draw_x, int draw_y, float rf, float gf, float bf, float af, float house_id = 0.0f, float depth = 0.0f, float flags = TILE_INSTANCE_FLAG_TEXTURE) {
		if (isBakeableRegion(reg)) {
			TileInstance inst;
			inst.flags = flags;
			inst.r = rf;
			inst.g = gf;
			inst.b = bf;
			inst.a = af;
			inst.house_id = house_id;
			inst.depth = depth;
			writeRegionGeometry(inst, *reg, draw_x, draw_y);
			bake_buffer_.push_back(inst);
		}
	};

	auto emitSpriteInstances = [&](GameSprite* spr, const SpritePatterns& pat, int draw_base_x, int draw_base_y, float rf, float gf, float bf, float af, float house_id = 0.0f, float depth = 0.0f, float flags = TILE_INSTANCE_FLAG_TEXTURE, bool upload = false) {
		forEachSpriteRegion(spr, pat, draw_base_x, draw_base_y, upload || allow_upload, [&](const AtlasRegion* reg, int draw_x, int draw_y) {
			if (!reg) {
				// Not uploaded yet. The caller queued it to the sprite preloader, so the chunk
				// is simply re-baked once it becomes available.
				skipped = true;
				return;
			}
			pushRegionInstance(reg, draw_x, draw_y, rf, gf, bf, af, house_id, depth, flags);
		});
	};

	// Map items and grounds animate; their runs are tracked so frame changes can be patched in place.
	auto pushSpriteInstances = [&](GameSprite* spr, const SpritePatterns& pat, int draw_base_x, int draw_base_y, float rf, float gf, float bf, float af, float house_id = 0.0f, float depth = 0.0f, float flags = TILE_INSTANCE_FLAG_TEXTURE, bool upload = false, int map_pos_x = 0, int map_pos_y = 0, int map_pos_z = 0) {
		const size_t first_instance = bake_buffer_.size();
		emitSpriteInstances(spr, pat, draw_base_x, draw_base_y, rf, gf, bf, af, house_id, depth, flags, upload);
		if (!spr->isAnimated()) {
			return;
		}

		const bool is_async = (spr->animation_data && spr->animation_data->is_async);
		uint32_t slot_idx = 0;
		if (!is_async) {
			auto it = std::ranges::find_if(scratch_sync_slots_, [spr](const auto& pair) {
				return pair.first == spr;
			});
			if (it != scratch_sync_slots_.end()) {
				slot_idx = it->second;
			} else {
				slot_idx = static_cast<uint32_t>(chunk.animated_sprites.size());
				chunk.animated_sprites.push_back(AnimatedSpriteFrame {
					.sprite = spr,
					.tile_x = 0,
					.tile_y = 0,
					.tile_z = 0,
					.frame = pat.frame
				});
				scratch_sync_slots_.emplace_back(spr, slot_idx);
			}
		} else {
			const int tx = map_pos_x;
			const int ty = map_pos_y;
			const int tz = map_pos_z;
			auto it = std::ranges::find_if(chunk.animated_sprites, [&](const AnimatedSpriteFrame& f) {
				return f.sprite == spr && f.tile_x == tx && f.tile_y == ty && f.tile_z == tz;
			});
			if (it != chunk.animated_sprites.end()) {
				slot_idx = static_cast<uint32_t>(std::distance(chunk.animated_sprites.begin(), it));
			} else {
				slot_idx = static_cast<uint32_t>(chunk.animated_sprites.size());
				chunk.animated_sprites.push_back(AnimatedSpriteFrame {
					.sprite = spr,
					.tile_x = tx,
					.tile_y = ty,
					.tile_z = tz,
					.frame = pat.frame
				});
			}
		}

		chunk.animated_runs.push_back(AnimatedSpriteRun {
			.first_instance = static_cast<uint32_t>(first_instance),
			.instance_count = static_cast<uint32_t>(bake_buffer_.size() - first_instance),
			.sprite_slot = slot_idx,
			.draw_x = draw_base_x,
			.draw_y = draw_base_y,
			.pattern_x = static_cast<int16_t>(pat.x),
			.pattern_y = static_cast<int16_t>(pat.y),
			.pattern_z = static_cast<int16_t>(pat.z),
			.subtype = static_cast<int16_t>(pat.subtype.value_or(-1)),
		});
	};

	auto pushCreatureInstances = [&](const Creature* creature, int screenx, int screeny, float depth = 0.0f) {
		if (!creature) {
			return;
		}

		float rf = 1.0f, gf = 1.0f, bf = 1.0f, af = 1.0f;
		if (!ctx.options.ingame && creature->isSelected()) {
			rf = 0.5f;
			gf = 0.5f;
			bf = 0.5f;
		}

		const Outfit& outfit = creature->getLookType();
		const Direction dir = creature->getDirection();

		if (outfit.lookItem != 0) {
			const auto definition = ctx.item_definitions.get(outfit.lookItem);
			if (definition) {
				GameSprite* ispr = ctx.gfx.getGameSprite(definition.clientId());
				if (ispr) {
					const auto [draw_offset_x, draw_offset_y] = ispr->getDrawOffset();
					const int item_x = screenx - draw_offset_x;
					const int item_y = screeny - draw_offset_y;
					const SpritePatterns pat { .x = 0, .y = 0, .z = 0, .frame = 0, .subtype = std::nullopt };
					emitSpriteInstances(ispr, pat, item_x, item_y, rf, gf, bf, af, 0.0f, depth);
				}
			}
			return;
		}

		const Outfit* drawOutfit = &outfit;
		if (drawOutfit->lookType == 0) {
			drawOutfit = &DEFAULT_UNKNOWN_CREATURE_OUTFIT;
		}
		GameSprite* spr = ctx.gfx.getCreatureSprite(drawOutfit->lookType);
		if (!spr && drawOutfit->lookType != DEFAULT_UNKNOWN_CREATURE_OUTFIT.lookType) {
			drawOutfit = &DEFAULT_UNKNOWN_CREATURE_OUTFIT;
			spr = ctx.gfx.getCreatureSprite(DEFAULT_UNKNOWN_CREATURE_OUTFIT.lookType);
		}
		if (!spr) {
			return;
		}

		// The creature's sprite animation determines how often this chunk can change.
		if (spr->isAnimated()) {
			int step = spr->getFrameStepMs();
			if (step <= 0) {
				step = rme::rendering::ITEM_FRAME_DURATION;
			}
			creature_step_ms = static_cast<uint16_t>(std::min(step, 65000));
		}

		int pattern_z = 0;
		GameSprite* mountSpr = nullptr;
		if (drawOutfit->lookMount != 0) {
			if ((mountSpr = ctx.gfx.getCreatureSprite(drawOutfit->lookMount))) {
				Outfit mountOutfit;
				mountOutfit.lookType = drawOutfit->lookMount;
				mountOutfit.lookMount = 0;
				mountOutfit.lookHead = drawOutfit->lookMountHead;
				mountOutfit.lookBody = drawOutfit->lookMountBody;
				mountOutfit.lookLegs = drawOutfit->lookMountLegs;
				mountOutfit.lookFeet = drawOutfit->lookMountFeet;

				const auto mount_draw_offset = mountSpr->getDrawOffset();
				const bool is_simple_mount = (mountSpr->width == 1 && mountSpr->height == 1);
				const int mount_base_x = screenx - mount_draw_offset.first;
				const int mount_base_y = screeny - mount_draw_offset.second;

				if (is_simple_mount) {
					const AtlasRegion* region = mountSpr->getAtlasRegion(0, 0, static_cast<int>(dir), 0, 0, mountOutfit, 0);
					pushRegionInstance(region, mount_base_x, mount_base_y, rf, gf, bf, af, 0.0f, depth);
				} else {
					const auto mount_metrics = mountSpr->getOutfitLayoutMetrics(static_cast<int>(dir), 0, 0, 0);
					int mount_x_offset = 0;
					for (int cx = 0; cx < mount_metrics.num_columns; ++cx) {
						int mount_y_offset = 0;
						for (int cy = 0; cy < mount_metrics.num_rows; ++cy) {
							const AtlasRegion* region = mountSpr->getAtlasRegion(cx, cy, static_cast<int>(dir), 0, 0, mountOutfit, 0);
							pushRegionInstance(region, mount_base_x - mount_x_offset, mount_base_y - mount_y_offset, rf, gf, bf, af, 0.0f, depth);
							mount_y_offset += mount_metrics.row_heights[cy];
						}
						mount_x_offset += mount_metrics.column_widths[cx];
					}
				}

				pattern_z = std::clamp(spr->pattern_z - 1, 0, 1);
			}
		}

		const auto sprite_draw_offset = spr->getDrawOffset();
		const int base_x = screenx - sprite_draw_offset.first;
		const int base_y = screeny - sprite_draw_offset.second;

		for (int pattern_y = 0; pattern_y < spr->pattern_y; pattern_y++) {
			if (pattern_y > 0) {
				if ((pattern_y - 1 >= 31) || !(drawOutfit->lookAddon & (1 << (pattern_y - 1)))) {
					continue;
				}
			}

			if (spr->width == 1 && spr->height == 1) {
				const AtlasRegion* region = spr->getAtlasRegion(0, 0, static_cast<int>(dir), pattern_y, pattern_z, *drawOutfit, 0);
				pushRegionInstance(region, base_x, base_y, rf, gf, bf, af, 0.0f, depth);
				continue;
			}

			const auto sprite_metrics = spr->getOutfitLayoutMetrics(static_cast<int>(dir), pattern_y, pattern_z, 0);
			int sprite_x_offset = 0;
			for (int cx = 0; cx < sprite_metrics.num_columns; ++cx) {
				int sprite_y_offset = 0;
				for (int cy = 0; cy < sprite_metrics.num_rows; ++cy) {
					const AtlasRegion* region = spr->getAtlasRegion(cx, cy, static_cast<int>(dir), pattern_y, pattern_z, *drawOutfit, 0);
					pushRegionInstance(region, base_x - sprite_x_offset, base_y - sprite_y_offset, rf, gf, bf, af, 0.0f, depth);
					sprite_y_offset += sprite_metrics.row_heights[cy];
				}
				sprite_x_offset += sprite_metrics.column_widths[cx];
			}
		}
	};

	// =========================================================================
	// Pass 1: Static & animated terrain ground and borders (water, grass, coastlines)
	// Traverses tiles in strict diagonal order (Painter's Algorithm: NW to SE).
	// =========================================================================
	for (int d = 0; d < 2 * CHUNK_SIZE - 1; ++d) {
		for (int tx = 0; tx <= d && tx < CHUNK_SIZE; ++tx) {
			const int ty = d - tx;
			if (ty >= CHUNK_SIZE) {
				continue;
			}

			const TileLocation* loc = chunkTileLocation(floors, tx, ty);
			const Tile* tile = loc ? loc->get() : nullptr;
			if (!tile) {
				continue;
			}
			if (ctx.options.show_only_modified && !tile->isModified()) {
				continue;
			}

			const int x = base_x + tx;
			const int y = base_y + ty;

			const bool as_minimap = ctx.options.show_as_minimap;
			const bool only_colors = as_minimap || ctx.options.show_only_colors;

			if ((tile->creature && ctx.options.show_creatures && !only_colors) || tile->hasInvalidZones() ||
				(tile->ground && tile->ground->isInvalidOTBMItem())) {
				chunk.markDynamicTile(tx, ty);
			}

			const float tile_house_id = tile->isHouseTile() ? static_cast<float>(tile->getHouseID()) : 0.0f;

			uint8_t gr = 255, gg = 255, gb = 255;
			if (!ctx.options.show_as_minimap && ctx.options.hasTileColorModifiers()) {
				TileColorCalculator::Calculate(tile, ctx.options, gr, gg, gb);
			}

			// 1. Static & animated terrain ground (water, grass, dirt, lava, etc.)
			if (tile->ground) {
				const ItemDefinitionView git = tile->ground->getDefinition();
				const uint16_t ground_client_id = git ? git.clientId() : 0;
				const uint16_t ground_server_id = tile->ground->getID();

				const float ground_depth = rme::rendering::calculateTileDepth(x, y, rme::rendering::RenderSublayer::Ground);
				const auto tech_type = (ctx.options.show_tech_items && !ctx.options.ingame)
					? rme::rendering::TechnicalItemRegistry::Classify(ground_server_id, ground_client_id)
					: rme::rendering::TileIndicatorType::None;
				if (tech_type != rme::rendering::TileIndicatorType::None) {
					const AtlasRegion* white_pixel = ctx.atlas.getWhitePixel();
					if (white_pixel) {
						pushRegionInstance(white_pixel, x * 32, y * 32, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, ground_depth, MakeTileIndicatorFlag(static_cast<uint8_t>(tech_type)));
					}
				} else if (git && !git.isMetaItem()) {
					GameSprite* gspr = ctx.gfx.getGameSprite(git.clientId());
					if (gspr) {
						const SpritePatterns g_pat = PatternCalculator::Calculate(gspr, git, tile->ground.get(), tile, Position(x, y, z), ctx.elapsed_time);
						if (!gspr->isSimpleAndLoaded()) {
							rme::collectTileSprites(gspr, g_pat.x, g_pat.y, g_pat.z, g_pat.frame);
						}

						uint8_t r = gr, g = gg, b = gb;
						if (tile->ground->isSelected()) {
							r >>= 1;
							g >>= 1;
							b >>= 1;
						}

						const auto [g_off_x, g_off_y] = gspr->getDrawOffset();
						const int ground_x = x * 32 - g_off_x;
						const int ground_y = y * 32 - g_off_y;

						// The ground is what makes a chunk visible at all: without it a chunk
						// (water in particular, whose ground is an animated sprite) renders as a
						// black hole until the preloader catches up. Resolve it with loading
						// allowed, and keep the (far more numerous) borders and items on the
						// non-blocking path.
						pushSpriteInstances(gspr, g_pat, ground_x, ground_y,
							static_cast<float>(r) * (1.0f / 255.0f),
							static_cast<float>(g) * (1.0f / 255.0f),
							static_cast<float>(b) * (1.0f / 255.0f),
							1.0f,
							tile_house_id,
							ground_depth,
							TILE_INSTANCE_FLAG_TEXTURE + TILE_INSTANCE_FLAG_IS_GROUND,
							/*upload=*/true,
							x, y, z);
					}
				}
			}

			// 2. Static & animated ground borders (coastlines, grass edges, shallow water, etc.)
			for (const auto& item : tile->items) {
				if (!item || !item->isBorder() || item->isInvalidOTBMItem()) {
					continue;
				}
				const ItemDefinitionView it = item->getDefinition();
				if (!it) {
					continue;
				}
				const uint16_t item_client_id = it.clientId();
				const uint16_t item_server_id = item->getID();

				const float border_depth = rme::rendering::calculateTileDepth(x, y, rme::rendering::RenderSublayer::Border);
				const auto tech_type = (ctx.options.show_tech_items && !ctx.options.ingame)
					? rme::rendering::TechnicalItemRegistry::Classify(item_server_id, item_client_id)
					: rme::rendering::TileIndicatorType::None;
				if (tech_type != rme::rendering::TileIndicatorType::None) {
					const AtlasRegion* white_pixel = ctx.atlas.getWhitePixel();
					if (white_pixel) {
						pushRegionInstance(white_pixel, x * 32, y * 32, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, border_depth, MakeTileIndicatorFlag(static_cast<uint8_t>(tech_type)));
					}
					continue;
				}

				GameSprite* ispr = ctx.gfx.getGameSprite(it.clientId());
				if (!ispr) {
					continue;
				}

				const SpritePatterns i_pat = PatternCalculator::Calculate(ispr, it, item.get(), tile, Position(x, y, z), ctx.elapsed_time);
				if (!ispr->isSimpleAndLoaded()) {
					rme::collectTileSprites(ispr, i_pat.x, i_pat.y, i_pat.z, i_pat.frame);
				}

				uint8_t r = gr, g = gg, b = gb;
				if (item->isSelected()) {
					r >>= 1;
					g >>= 1;
					b >>= 1;
				}

				const auto [draw_offset_x, draw_offset_y] = ispr->getDrawOffset();
				const int item_x = x * 32 - draw_offset_x;
				const int item_y = y * 32 - draw_offset_y;

				pushSpriteInstances(ispr, i_pat, item_x, item_y,
					static_cast<float>(r) * (1.0f / 255.0f),
					static_cast<float>(g) * (1.0f / 255.0f),
					static_cast<float>(b) * (1.0f / 255.0f),
					1.0f,
					tile_house_id,
					border_depth,
					TILE_INSTANCE_FLAG_TEXTURE + TILE_INSTANCE_FLAG_IS_GROUND,
					false,
					x, y, z);
			}
		}
	}
	chunk.terrain_instance_count = static_cast<uint32_t>(bake_buffer_.size());

	// =========================================================================
	// Pass 2: Static items & structures, point indicators, creatures
	// Traverses tiles in strict diagonal order (Painter's Algorithm: NW to SE).
	// Rendered after terrain and ground-level zone overlays.
	// =========================================================================
	for (int d = 0; d < 2 * CHUNK_SIZE - 1; ++d) {
		for (int tx = 0; tx <= d && tx < CHUNK_SIZE; ++tx) {
			const int ty = d - tx;
			if (ty >= CHUNK_SIZE) {
				continue;
			}

			const TileLocation* loc = chunkTileLocation(floors, tx, ty);
			const Tile* tile = loc ? loc->get() : nullptr;
			if (!tile) {
				continue;
			}
			if (ctx.options.show_only_modified && !tile->isModified()) {
				continue;
			}

			const int x = base_x + tx;
			const int y = base_y + ty;

			const bool as_minimap = ctx.options.show_as_minimap;
			const bool only_colors = as_minimap || ctx.options.show_only_colors;

			const float tile_house_id = tile->isHouseTile() ? static_cast<float>(tile->getHouseID()) : 0.0f;

			// 3. Static items & structures with elevation stacking. Skipped entirely when
			// items are not drawn at this zoom: the item pass is the dominant cost of a
			// pan/zoom bake burst.
			int elev = 0;
			if (bake_items) {
				for (const auto& item : tile->items) {
					if (!item || item->isBorder()) {
						continue;
					}
					if (item->isInvalidOTBMItem()) {
						chunk.markDynamicTile(tx, ty);
						continue;
					}
					const ItemDefinitionView it = item->getDefinition();
					const uint16_t item_client_id = it ? it.clientId() : 0;
					const uint16_t item_server_id = item->getID();

					rme::rendering::RenderSublayer sublayer = rme::rendering::RenderSublayer::CommonItem;
					if (item->isAlwaysOnBottom()) {
						sublayer = rme::rendering::RenderSublayer::BottomItem;
					} else if (it && it.hasFlag(ItemFlag::TopEffect)) {
						sublayer = rme::rendering::RenderSublayer::TopItem;
					}
					const float item_depth = rme::rendering::calculateTileDepth(x, y, sublayer, elev > 0 ? 1 : 0);

					const auto tech_type = (ctx.options.show_tech_items && !ctx.options.ingame)
						? rme::rendering::TechnicalItemRegistry::Classify(item_server_id, item_client_id)
						: rme::rendering::TileIndicatorType::None;
					if (tech_type != rme::rendering::TileIndicatorType::None) {
						const AtlasRegion* white_pixel = ctx.atlas.getWhitePixel();
						if (white_pixel) {
							pushRegionInstance(white_pixel, x * 32, y * 32, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, item_depth, MakeTileIndicatorFlag(static_cast<uint8_t>(tech_type)));
						}
						continue;
					}

					if (!it || it.isMetaItem() || (!ctx.options.show_items && it.hasFlag(ItemFlag::Pickupable))) {
						continue;
					}
					GameSprite* ispr = ctx.gfx.getGameSprite(it.clientId());
					if (!ispr) {
						continue;
					}

					const auto [draw_offset_x, draw_offset_y] = ispr->getDrawOffset();
					const int item_x = x * 32 - elev - draw_offset_x;
					const int item_y = y * 32 - elev - draw_offset_y;

					const SpritePatterns i_pat = PatternCalculator::Calculate(ispr, it, item.get(), tile, Position(x, y, z), ctx.elapsed_time);
					if (!ispr->isSimpleAndLoaded()) {
						rme::collectTileSprites(ispr, i_pat.x, i_pat.y, i_pat.z, i_pat.frame);
					}

					uint8_t r = 255, g = 255, b = 255, a = 255;
					if (item->isSelected()) {
						r >>= 1;
						g >>= 1;
						b >>= 1;
					}

					if (ctx.options.transparent_items && (!it.isGroundTile() || ispr->width > 1 || ispr->height > 1) && !it.isSplash() && (!it.hasFlag(ItemFlag::IsBorder) || ispr->width > 1 || ispr->height > 1)) {
						a >>= 1;
					}

					const float item_house_id = (ctx.options.extended_house_shader) ? tile_house_id : 0.0f;
					const bool is_blocking_item = item->isBlocking() &&
						!rme::rendering::TechnicalItemRegistry::IsInvisibleWall(item_server_id, item_client_id);
					const float item_flags = (ctx.options.extended_pathing_shader && is_blocking_item)
						? TILE_INSTANCE_FLAG_BLOCKING
						: TILE_INSTANCE_FLAG_TEXTURE;

					pushSpriteInstances(ispr, i_pat, item_x, item_y,
						static_cast<float>(r) * (1.0f / 255.0f),
						static_cast<float>(g) * (1.0f / 255.0f),
						static_cast<float>(b) * (1.0f / 255.0f),
						static_cast<float>(a) * (1.0f / 255.0f),
						item_house_id,
						item_depth,
						item_flags,
						false,
						x, y, z);

					if (ispr->hasElevation()) {
						elev += ispr->draw_height;
					}
				}
			} // bake_items

			// 3.5. Tile point indicators (rendered on top of items and ground)
			const AtlasRegion* white_pixel = ctx.atlas.getWhitePixel();
			if (white_pixel) {
				const float indicator_depth = rme::rendering::calculateTileDepth(x, y, rme::rendering::RenderSublayer::Overlay);
				if (tile->isHouseExit()) {
					const HouseExitList* exits = tile->getHouseExits();
					const uint32_t exit_house_id = (exits && !exits->empty()) ? exits->front() : 1;
					pushRegionInstance(white_pixel, x * 32, y * 32, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, indicator_depth, MakeTileIndicatorFlag(static_cast<uint8_t>(rme::rendering::TileIndicatorType::HouseEntry)));
				}
				if (loc->getTownCount() > 0) {
					pushRegionInstance(white_pixel, x * 32, y * 32, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, indicator_depth, MakeTileIndicatorFlag(static_cast<uint8_t>(rme::rendering::TileIndicatorType::TownTemple)));
				}
			}

			// 4. Creature on tile (Painter's Algorithm: NW-to-SE order ensures occlusion by adjacent South/East walls and structures)
			if (tile->creature && ctx.options.show_creatures && !only_colors) {
				const float creature_depth = rme::rendering::calculateTileDepth(x, y, rme::rendering::RenderSublayer::DynamicEntity);
				pushCreatureInstances(tile->creature.get(), x * 32, y * 32, creature_depth);
			}
		}
	}

	chunk.item_instance_count = static_cast<uint32_t>(bake_buffer_.size()) - chunk.terrain_instance_count;
	chunk.instance_count = static_cast<uint32_t>(bake_buffer_.size());

	// getFrameStepMs() is constant per animated sprite, so resolve the shortest animation frame
	// duration once per bake; the frame loop reads this cached value instead of re-walking
	// every AnimatedSpriteFrame of every visible chunk.
	uint16_t sprite_step_ms = 0;
	for (const AnimatedSpriteFrame& entry : chunk.animated_sprites) {
		if (entry.sprite && entry.sprite->isAnimated()) {
			const int step = entry.sprite->getFrameStepMs();
			if (step > 0 && (sprite_step_ms == 0 || static_cast<uint32_t>(step) < sprite_step_ms)) {
				sprite_step_ms = static_cast<uint16_t>(step);
			}
		}
	}
	chunk.animated_step_ms = sprite_step_ms;

	const std::vector<TileInstance>* upload_buffer = &bake_buffer_;
	if (chunk.animated_runs.empty()) {
		chunk.animated_shadow = {};
	} else {
		// Swap the bake buffer in instead of deep-copying it: animated_shadow takes ownership
		// and bake_buffer_ keeps the recycled capacity of the previous shadow for the next bake.
		chunk.animated_shadow.swap(bake_buffer_);
		upload_buffer = &chunk.animated_shadow;
	}

	uploadChunk(chunk, *upload_buffer);
	chunk.is_dirty = false;
	chunk.baked_with_items = bake_items;
	chunk.creature_animation_step_ms = creature_step_ms;
	if (skipped && !allow_upload) {
		chunk.is_complete = false;
		chunk.rebake_attempts = std::min(chunk.rebake_attempts + 1, MAX_INCOMPLETE_REBAKES);
		chunk.rebake_at_ms = steadyNowMs() + INCOMPLETE_REBAKE_DELAY_MS;
	} else {
		// Complete, or baked with loading allowed: nothing is left to wait for.
		chunk.is_complete = true;
		chunk.rebake_attempts = 0;
		chunk.force_load = false;
	}
}

bool ChunkCacheManager::refreshAnimatedFrames(CachedChunk& chunk, long elapsed_time) {
	const size_t slot_count = chunk.animated_sprites.size();
	if (slot_count == 0) {
		return true;
	}

	scratch_changed_slots_.assign(slot_count, 0);
	bool any_changed = false;
	for (size_t slot = 0; slot < slot_count; ++slot) {
		AnimatedSpriteFrame& entry = chunk.animated_sprites[slot];
		if (!entry.sprite || !entry.sprite->isAnimated()) {
			continue;
		}
		const int frame = entry.sprite->getAnimationFrame(elapsed_time, entry.tile_x, entry.tile_y, entry.tile_z);
		if (frame != entry.frame) {
			entry.frame = frame;
			scratch_changed_slots_[slot] = 1;
			any_changed = true;
		}
	}
	if (!any_changed) {
		return true;
	}

	size_t patch_begin = chunk.animated_shadow.size();
	size_t patch_end = 0;
	for (const AnimatedSpriteRun& run : chunk.animated_runs) {
		if (run.sprite_slot >= slot_count || !scratch_changed_slots_[run.sprite_slot]) {
			continue;
		}

		GameSprite* spr = chunk.animated_sprites[run.sprite_slot].sprite;
		const SpritePatterns pat {
			.x = run.pattern_x,
			.y = run.pattern_y,
			.z = run.pattern_z,
			.frame = chunk.animated_sprites[run.sprite_slot].frame,
			.subtype = run.subtype >= 0 ? std::optional<int>(run.subtype) : std::nullopt,
		};
		if (!spr->isSimpleAndLoaded()) {
			rme::collectTileSprites(spr, pat.x, pat.y, pat.z, pat.frame);
		}

		uint32_t written = 0;
		bool layout_changed = false;
		forEachSpriteRegion(spr, pat, run.draw_x, run.draw_y, true, [&](const AtlasRegion* reg, int draw_x, int draw_y) {
			if (!isBakeableRegion(reg)) {
				return;
			}
			if (written == run.instance_count) {
				layout_changed = true;
				return;
			}
			writeRegionGeometry(chunk.animated_shadow[run.first_instance + written], *reg, draw_x, draw_y);
			++written;
		});
		if (layout_changed || written != run.instance_count) {
			return false;
		}

		patch_begin = std::min<size_t>(patch_begin, run.first_instance);
		patch_end = std::max<size_t>(patch_end, run.first_instance + run.instance_count);
	}

	if (patch_begin < patch_end) {
		glNamedBufferSubData(chunk.vbo, static_cast<GLintptr>(patch_begin * sizeof(TileInstance)), static_cast<GLsizeiptr>((patch_end - patch_begin) * sizeof(TileInstance)), chunk.animated_shadow.data() + patch_begin);
	}
	++stats_.anim_patches;
	return true;
}

void ChunkCacheManager::recordFrameStats() {
	const auto now = std::chrono::steady_clock::now();
	if (last_frame_start_ != std::chrono::steady_clock::time_point {}) {
		const double frame_ms = std::chrono::duration<double, std::milli>(now - last_frame_start_).count();
		++stats_.frames;
		stats_.frame_ms_total += frame_ms;
		stats_.frame_ms_max = std::max(stats_.frame_ms_max, frame_ms);
		stats_.bake_ms_total += frame_bake_ms_;
		stats_.bake_ms_max = std::max(stats_.bake_ms_max, frame_bake_ms_);
	}
	last_frame_start_ = now;
	frame_bake_ms_ = 0.0;

	if (stats_.frames < PRUNE_INTERVAL_FRAMES) {
		return;
	}
	const double frames = static_cast<double>(stats_.frames);
	if (rme::prof::enabled()) {
		spdlog::info("[ChunkCacheStats] {} frames | frame ms avg {:.2f} max {:.2f} | bake ms avg {:.3f} max {:.2f} | bakes new {} edit {} anim {} deferred {} | anim patches {} | draws/frame {:.0f} | instances/frame {:.0f}",
			stats_.frames, stats_.frame_ms_total / frames, stats_.frame_ms_max,
			stats_.bake_ms_total / frames, stats_.bake_ms_max,
			stats_.baked_new, stats_.baked_edit, stats_.baked_anim, stats_.baked_deferred, stats_.anim_patches,
			static_cast<double>(stats_.draws) / frames, static_cast<double>(stats_.instances) / frames);
	} else {
		// Not profiling: the same numbers are still useful when debugging (debug level) but
		// must not write a log line every 2 s in a user's normal session.
		spdlog::debug("[ChunkCacheStats] {} frames | frame ms avg {:.2f} max {:.2f} | bake ms avg {:.3f} max {:.2f} | bakes new {} edit {} anim {} deferred {} | anim patches {} | draws/frame {:.0f} | instances/frame {:.0f}",
			stats_.frames, stats_.frame_ms_total / frames, stats_.frame_ms_max,
			stats_.bake_ms_total / frames, stats_.bake_ms_max,
			stats_.baked_new, stats_.baked_edit, stats_.baked_anim, stats_.baked_deferred, stats_.anim_patches,
			static_cast<double>(stats_.draws) / frames, static_cast<double>(stats_.instances) / frames);
	}
	stats_ = {};
}

void ChunkCacheManager::advanceFrame(int current_floor) {
	recordFrameStats();
	// Recomputed by the floors rendered this frame (see renderFloorTerrain).
	visible_animation_step_ms_ = 0;
	incomplete_chunks_ = 0;
	cold_start_frame_ = cached_chunks_.empty();
	++current_frame_;
	if (current_frame_ % PRUNE_INTERVAL_FRAMES == 0) {
		prune(current_floor);
	}
}

void ChunkCacheManager::renderFloorTerrain(
	int map_z,
	const Map& map,
	const RenderFrameContext& ctx,
	const glm::mat4& projection,
	AtlasManager& atlas
) {
	if (!isValid()) {
		return;
	}

	const ViewBounds bounds = ctx.view.getBoundsForFloor(map_z);
	const int min_cx = (bounds.start_x - 16) >> 4;
	const int max_cx = (bounds.end_x + 31) >> 4;
	const int min_cy = (bounds.start_y - 16) >> 4;
	const int max_cy = (bounds.end_y + 31) >> 4;

	const int offset = (map_z <= GROUND_LAYER)
		? (GROUND_LAYER - map_z) * TILE_SIZE
		: TILE_SIZE * (ctx.view.floor - map_z);
	const glm::vec3 translation(
		static_cast<float>(-ctx.view.view_scroll_x - offset),
		static_cast<float>(-ctx.view.view_scroll_y - offset),
		0.0f
	);
	const glm::mat4 floor_mvp = projection * glm::translate(glm::mat4(1.0f), translation);

	shader_.Use();
	shader_.SetMat4("uMVP", floor_mvp);
	shader_.SetInt("uAtlas", 0);
	shader_.SetInt("uAtlasLUT", SpriteAtlasLUT::TEXTURE_UNIT_INDEX);
	shader_.SetVec4("uGlobalTint", glm::vec4(1.0f));
	rme::rendering::shaders::SetChunkShaderUniforms(shader_, ctx.current_house_id, ctx.options);

	atlas.bind(0);
	atlas.bindLUT(SpriteAtlasLUT::TEXTURE_UNIT_INDEX);

	glBindVertexArray(vao_);

	// Reset active visible chunk list for this floor
	active_visible_chunks_.clear();
	active_floor_ = map_z;

	uint32_t baked_count = 0;
	uint32_t rendered_chunk_count = 0;
	uint32_t rendered_instance_count = 0;

	// Sparse Query: Touches ONLY populated chunks on map_z!
	const bool profile_enabled = rme::prof::enabled();
	incomplete_visible_.clear();
	auto drawChunk = [&](CachedChunk& chunk) {
		if (chunk.is_empty || chunk.terrain_instance_count == 0 || chunk.vbo == 0) {
			return;
		}
		rme::prof::Scope draw_scope(rme::prof::Phase::TerrainDraw);
		glVertexArrayVertexBuffer(vao_, 1, chunk.vbo, 0, sizeof(TileInstance));
		glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(chunk.terrain_instance_count));
		++rendered_chunk_count;
		rendered_instance_count += chunk.terrain_instance_count;
	};
	const auto query_start = std::chrono::steady_clock::now();
	map.visitPopulatedChunks(min_cx, min_cy, max_cx, max_cy, map_z, [&](int cx, int cy) {
		const ChunkCoord coord{ cx, cy, map_z };
		const size_t cached_before = cached_chunks_.size();
		CachedChunk& chunk = getOrCreateChunk(coord);
		const bool is_new = cached_chunks_.size() != cached_before;
		const bool was_dirty = chunk.is_dirty;

		// An incomplete chunk must not be animated-patched: the patch path assumes a complete
		// layout, so a missing sprite would fail it and force a full re-bake every frame.
		if (!chunk.is_dirty && chunk.is_complete && !chunk.animated_runs.empty() && ctx.options.show_preview && ctx.view.zoom < 10.0) {
			rme::prof::Scope patch_scope(rme::prof::Phase::AnimPatch);
			const auto patch_start = std::chrono::steady_clock::now();
			if (!refreshAnimatedFrames(chunk, ctx.elapsed_time)) {
				chunk.is_dirty = true;
			}
			frame_bake_ms_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - patch_start).count();
		}

		// On-screen test drives both the bake budget and the draw cull below.
		const int chunk_screen_x = cx * (CHUNK_SIZE * TILE_SIZE) + static_cast<int>(translation.x);
		const int chunk_screen_y = cy * (CHUNK_SIZE * TILE_SIZE) + static_cast<int>(translation.y);
		const bool chunk_visible = ctx.view.IsRectVisible(chunk_screen_x, chunk_screen_y, CHUNK_SIZE * TILE_SIZE, CHUNK_SIZE * TILE_SIZE);

		// Items are needed again once we are back inside the LOD range: re-bake chunks
		// that were baked without them.
		const bool want_items = !(ctx.view.zoom > 10.0f && ctx.options.hide_items_when_zoomed);
		if (want_items && !chunk.baked_with_items) {
			chunk.is_dirty = true;
		}

		// Re-bake an incomplete chunk so the sprites the last bake skipped appear as soon as
		// the preloader has loaded them. The retries are bounded; the last one may load inline
		// (force_load), which guarantees the chunk ends up complete.
		if (!chunk.is_complete && !chunk.is_dirty) {
			if (chunk.rebake_attempts >= MAX_INCOMPLETE_REBAKES) {
				chunk.force_load = true;
				chunk.is_dirty = true;
			} else if (steadyNowMs() >= chunk.rebake_at_ms) {
				chunk.is_dirty = true;
			}
		}

		if (chunk.is_dirty) {
			// Never defer an on-screen chunk: leaving it un-baked shows a black hole
			// while the user pans. Only the off-screen bake-ahead margin is budgeted.
			if (!chunk_visible && frame_bake_ms_ >= MAX_BAKE_MS_PER_FRAME) {
				++stats_.baked_deferred;
			} else {
				rme::prof::Scope bake_scope(rme::prof::Phase::ChunkBake);
				const auto bake_start = std::chrono::steady_clock::now();
				bakeChunk(chunk, map, ctx, chunk.force_load);
				frame_bake_ms_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - bake_start).count();
				if (is_new) {
					++stats_.baked_new;
				} else if (was_dirty) {
					++stats_.baked_edit;
				} else {
					++stats_.baked_anim;
				}
				++baked_count;
			}
		}
		chunk.last_accessed_frame = current_frame_;

		// Off-screen margin chunks are still kept warm for pan-ahead, but are not
		// drawn and do not join the active list, so their items and dynamic overlays
		// are skipped as well.
		if (!chunk_visible) {
			return;
		}

		if (!chunk.is_complete) {
			// The bake skipped sprites that were not uploaded yet, which leaves a hole - most
			// visible where water and its borders should be. Do not draw this chunk yet: the
			// pass below drains the preloader and bakes these chunks again, with the sprites in
			// place, so the hole is never shown.
			incomplete_visible_.push_back(&chunk);
		} else {
			drawChunk(chunk);
		}

		active_visible_chunks_.push_back(&chunk);
		if (!chunk.is_complete) {
			++incomplete_chunks_;
		}
		// Animations are only drawn inside the LOD range. When they are, the shortest visible
		// animation frame decides how often the view can actually change, which is what the
		// repaint loop should follow instead of repainting at 60 Hz. Both step values are
		// cached on the chunk at bake time, so this is O(1) per visible chunk.
		if (ctx.view.zoom < 10.0) {
			if (chunk.creature_animation_step_ms > 0) {
				const uint32_t step = chunk.creature_animation_step_ms;
				if (visible_animation_step_ms_ == 0 || step < visible_animation_step_ms_) {
					visible_animation_step_ms_ = step;
				}
			}
			if (chunk.animated_step_ms > 0) {
				const uint32_t step = chunk.animated_step_ms;
				if (visible_animation_step_ms_ == 0 || step < visible_animation_step_ms_) {
					visible_animation_step_ms_ = step;
				}
			}
		}
	});

	// Chunks whose bake had to skip not-yet-loaded sprites are finished here, before anything is
	// drawn: the bake already queued every sprite they need, so drain the preloader (its workers
	// decode sheets in parallel) and bake them again with the sprites in place. This is what keeps
	// water and its borders from flickering while the map loads or while panning into new area;
	// the wait ends as soon as the queue is empty and is capped so a single frame stays bounded.
	if (!incomplete_visible_.empty()) {
		auto& preloader = SpritePreloader::get();
		const auto drain_start = std::chrono::steady_clock::now();		// A trickle (panning reveals one or two chunks) must not cost a frame; a burst (a freshly
		// opened map, a relocation, which make a whole screen of chunks new at once) is worth
		// finishing before drawing, because that is exactly when the missing sprites would show as
		// water and borders flickering in. The loop stops as soon as the preloader has no work.
		const bool burst = cold_start_frame_ || incomplete_visible_.size() > INCOMPLETE_BURST_CHUNKS;
		const auto cap = burst ? COLD_START_DRAIN_CAP_MS : INCOMPLETE_DRAIN_CAP_MS;
		const auto budget = std::chrono::milliseconds(std::min<uint64_t>(
			cap,
			INCOMPLETE_DRAIN_BASE_MS + INCOMPLETE_DRAIN_PER_CHUNK_MS * incomplete_visible_.size()));
		while (preloader.hasPendingWork() && std::chrono::steady_clock::now() - drain_start < budget) {
			preloader.update();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		for (CachedChunk* chunk : incomplete_visible_) {
			bakeChunk(*chunk, map, ctx, false);
		}
		// Last resort: if a chunk is still incomplete after the drain, bake it once with loading
		// allowed. By now the preloader has most of what it needs, so this loads only the few
		// sprites it did not reach - and it guarantees the frame never shows a chunk with holes,
		// which is what made water and its borders flicker while panning.
		for (CachedChunk* chunk : incomplete_visible_) {
			if (!chunk->is_complete) {
				bakeChunk(*chunk, map, ctx, true);
			}
		}
		frame_bake_ms_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - drain_start).count();
		for (CachedChunk* chunk : incomplete_visible_) {
			drawChunk(*chunk);
		}
	}

	if (profile_enabled) {
		rme::prof::add(rme::prof::Phase::ChunkQuery, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - query_start).count());
	}

	stats_.draws += rendered_chunk_count;
	stats_.instances += rendered_instance_count;

	if (baked_count > 0) {
		spdlog::debug("[ChunkCache] Floor {}: Baked {} new/dirty chunk(s) | Visible: {} chunks ({} instances) | Total cached: {}/{}",
			map_z, baked_count, rendered_chunk_count, rendered_instance_count, cached_chunks_.size(), max_cached_chunks_);
	}

	glBindVertexArray(0);
	shader_.Unuse();
}

void ChunkCacheManager::renderFloorItems(
	int map_z,
	const RenderFrameContext& ctx,
	const glm::mat4& projection,
	AtlasManager& atlas
) {
	if (!isValid() || active_visible_chunks_.empty() || active_floor_ != map_z) {
		return;
	}

	const int offset = (map_z <= GROUND_LAYER)
		? (GROUND_LAYER - map_z) * TILE_SIZE
		: TILE_SIZE * (ctx.view.floor - map_z);
	const glm::vec3 translation(
		static_cast<float>(-ctx.view.view_scroll_x - offset),
		static_cast<float>(-ctx.view.view_scroll_y - offset),
		0.0f
	);
	const glm::mat4 floor_mvp = projection * glm::translate(glm::mat4(1.0f), translation);

	shader_.Use();
	shader_.SetMat4("uMVP", floor_mvp);
	shader_.SetInt("uAtlas", 0);
	shader_.SetInt("uAtlasLUT", SpriteAtlasLUT::TEXTURE_UNIT_INDEX);
	shader_.SetVec4("uGlobalTint", glm::vec4(1.0f));
	rme::rendering::shaders::SetChunkShaderUniforms(shader_, ctx.current_house_id, ctx.options);

	atlas.bind(0);
	atlas.bindLUT(SpriteAtlasLUT::TEXTURE_UNIT_INDEX);

	glBindVertexArray(vao_);

	{
		rme::prof::Scope items_scope(rme::prof::Phase::ItemDraw);
		// Far-zoom gating is per chunk. Chunks baked without items hold only the town/waypoint/
		// house-exit indicators and creature instances in their item range, which must stay
		// visible at every zoom; only a chunk still carrying baked items (its re-bake at this
		// zoom is pending) keeps the range suppressed so stale subpixel items never appear.
		const bool item_range_visible = !(ctx.view.zoom > 10.0f && ctx.options.hide_items_when_zoomed);
		for (const CachedChunk* chunk_ptr : active_visible_chunks_) {
			if (!chunk_ptr || chunk_ptr->is_empty || chunk_ptr->item_instance_count == 0 || chunk_ptr->vbo == 0
				|| (!item_range_visible && chunk_ptr->baked_with_items)) {
				continue;
			}

			const GLintptr byte_offset = static_cast<GLintptr>(chunk_ptr->terrain_instance_count * sizeof(TileInstance));
			glVertexArrayVertexBuffer(vao_, 1, chunk_ptr->vbo, byte_offset, sizeof(TileInstance));
			glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(chunk_ptr->item_instance_count));
			++stats_.draws;
			stats_.instances += chunk_ptr->item_instance_count;
		}
	}

	glBindVertexArray(0);
	shader_.Unuse();
}

void ChunkCacheManager::prune(
	int current_floor,
	int min_cx, int max_cx,
	int min_cy, int max_cy,
	bool has_bounds
) {
	size_t empty_evicted = 0;

	// Surface domain: floors 0 through GROUND_LAYER (7) are rendered together.
	// Never consider surface floors as "far" from each other!
	const bool is_surface_view = (current_floor <= GROUND_LAYER);

	for (auto it = cached_chunks_.begin(); it != cached_chunks_.end();) {
		const auto& [coord, chunk] = *it;
		const uint64_t age = current_frame_ - chunk.last_accessed_frame;

		bool is_far_floor = false;
		if (is_surface_view) {
			is_far_floor = (coord.z > GROUND_LAYER + 2);
		} else {
			is_far_floor = (coord.z <= GROUND_LAYER) || (std::abs(coord.z - current_floor) > 2);
		}

		// Tier 1: Empty chunks on far floors that are stale (negative cache cleanup)
		if (chunk.is_empty && is_far_floor && age > far_floor_frame_threshold_) {
			it = cached_chunks_.erase(it);
			++empty_evicted;
			continue;
		}

		++it;
	}

	size_t lru_evicted = 0;
	// Tier 3: Hard capacity ceiling (LRU eviction down to target_cached_chunks_)
	if (cached_chunks_.size() > max_cached_chunks_) {
		const size_t needed = cached_chunks_.size() - target_cached_chunks_;
		evictOldest(needed);
		lru_evicted = needed;
	}

	const size_t total_evicted = empty_evicted + lru_evicted;
	if (total_evicted > 0) {
		spdlog::info("[ChunkCache] Prune (frame {}): Evicted {} chunk(s) ({} empty, {} LRU) | Remaining: {}/{} (~{:.1f} MB VRAM)",
			current_frame_, total_evicted, empty_evicted, lru_evicted,
			cached_chunks_.size(), max_cached_chunks_, (cached_chunks_.size() * 3.5) / 1024.0);
	}
}

void ChunkCacheManager::evictOldest(size_t count_to_remove) {
	if (count_to_remove == 0 || cached_chunks_.empty()) {
		return;
	}

	std::vector<std::pair<uint64_t, ChunkCoord>> candidates;
	candidates.reserve(cached_chunks_.size());

	// Never evict chunks accessed in the current frame!
	for (const auto& [coord, chunk] : cached_chunks_) {
		if (chunk.last_accessed_frame < current_frame_) {
			candidates.emplace_back(chunk.last_accessed_frame, coord);
		}
	}

	if (candidates.empty()) {
		return;
	}

	const size_t num_evict = std::min(count_to_remove, candidates.size());
	std::partial_sort(
		candidates.begin(),
		candidates.begin() + num_evict,
		candidates.end(),
		[](const auto& a, const auto& b) { return a.first < b.first; }
	);

	for (size_t i = 0; i < num_evict; ++i) {
		cached_chunks_.erase(candidates[i].second);
	}

	spdlog::warn("[ChunkCache] High-water mark exceeded (>{} chunks)! LRU evicted {} oldest chunks | Remaining: {}",
		max_cached_chunks_, num_evict, cached_chunks_.size());
}

void ChunkCacheManager::renderDynamicOverlays(
	int map_z,
	const Map& map,
	const RenderFrameContext& ctx,
	SpriteBatch& sprite_batch,
	const TileRenderer& tile_renderer
) {
	if (!isValid()) {
		return;
	}


	// Dynamic overlay culling at extreme zoom:
	// When zoomed out beyond 10x (ctx.view.zoom >= 10.0, i.e. <= 10% zoom, tile size <= 3.2px),
	// dynamic tile elements (animated water ripples, torches, creatures) become subpixel.
	// Static terrain and items are already rendered by the chunk cache VBOs on the GPU.
	// Bypassing immediate CPU tile traversal here prevents pushing hundreds of thousands
	// of dynamic sprites into SpriteBatch, preserving high framerates at extreme zoom.
	if (ctx.view.zoom >= 10.0) {
		return;
	}

	if (active_floor_ != map_z) {
		return;
	}

	rme::prof::Scope dynamic_scope(rme::prof::Phase::DynamicOverlay);

	const int offset = (map_z <= GROUND_LAYER)
		? (GROUND_LAYER - map_z) * TILE_SIZE
		: TILE_SIZE * (ctx.view.floor - map_z);

	const int base_draw_x = -ctx.view.view_scroll_x - offset;
	const int base_draw_y = -ctx.view.view_scroll_y - offset;

	// Direct iteration of active visible chunks gathered in renderFloor!
	// Zero O(W x H) bounding box iteration, zero hash map lookups.
	for (const CachedChunk* chunk_ptr : active_visible_chunks_) {
		if (!chunk_ptr || !chunk_ptr->hasDynamicTiles()) {
			continue;
		}

		const auto& chunk = *chunk_ptr;
		const int chunk_base_x = chunk.coord.cx * CHUNK_SIZE;
		const int chunk_base_y = chunk.coord.cy * CHUNK_SIZE;

		ChunkFloors floors;
		if (!gatherChunkFloors(map, chunk.coord.cx, chunk.coord.cy, map_z, floors)) {
			continue;
		}
		ChunkFloors floors_above {};
		if (map_z == GROUND_LAYER + 1) {
			gatherChunkFloors(map, chunk.coord.cx, chunk.coord.cy, GROUND_LAYER, floors_above);
		}

		for (size_t w = 0; w < 4; ++w) {
			uint64_t word = chunk.dynamic_tile_mask[w];
			while (word != 0) {
				const int bit = std::countr_zero(word);
				const int tile_idx = static_cast<int>(w * 64 + bit);
				const int tx = tile_idx & 15;
				const int ty = tile_idx >> 4;

				const int x = chunk_base_x + tx;
				const int y = chunk_base_y + ty;

				const int draw_x = x * TILE_SIZE + base_draw_x;
				const int draw_y = y * TILE_SIZE + base_draw_y;

				if (ctx.view.IsPixelVisible(draw_x, draw_y)) {
					const TileLocation* loc = chunkTileLocation(floors, tx, ty);
					ASSERT(loc == map.getTileL(x, y, map_z));
					if (loc) {
						const TileLocation* loc_above = chunkTileLocation(floors_above, tx, ty);
						const Tile* tile_above = loc_above ? loc_above->get() : nullptr;
						ASSERT(tile_above == ((map_z == GROUND_LAYER + 1) ? map.getTile(x, y, GROUND_LAYER) : nullptr));
						tile_renderer.RenderDynamicPasses(sprite_batch, loc, ctx, draw_x, draw_y, tile_above);
					}
				}

				word &= word - 1; // Clear lowest set bit
			}
		}
	}
}
