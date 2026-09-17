//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/chunk_cache_manager.h"
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
#include "rendering/utilities/pattern_calculator.h"
#include "rendering/core/sprite_preloader.h"
#include <spdlog/spdlog.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace {
	constexpr const char* CHUNK_VERT_SHADER = R"(#version 430 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aRect;
layout(location = 3) in uint aSpriteId;
layout(location = 4) in uint aFlags;
layout(location = 5) in vec4 aTint;

struct SpriteLUTEntry {
	vec4 uv_rect;
	float layer;
	float valid;
	vec2 _pad;
};

layout(std430, binding = 2) readonly buffer AtlasLUT {
	SpriteLUTEntry lutEntries[];
};

uniform mat4 uMVP;
uniform vec4 uGlobalTint;

out vec3 vTexCoord;
out vec4 vColor;

void main() {
	vec2 worldPos = aRect.xy + aPos * aRect.zw;
	gl_Position = uMVP * vec4(worldPos, 0.0, 1.0);

	SpriteLUTEntry entry = lutEntries[aSpriteId];
	vec2 uv = mix(entry.uv_rect.xy, entry.uv_rect.zw, aTexCoord);
	vTexCoord = vec3(uv, entry.layer);
	vColor = aTint * uGlobalTint;
}
)";

	constexpr const char* CHUNK_FRAG_SHADER = R"(#version 430 core
in vec3 vTexCoord;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2DArray uAtlas;

void main() {
	vec4 texColor = texture(uAtlas, vTexCoord);
	FragColor = texColor * vColor;
	if (FragColor.a < 0.01) {
		discard;
	}
}
)";
}

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

	if (!shader_.Load(CHUNK_VERT_SHADER, CHUNK_FRAG_SHADER)) {
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

	// Loc 3: aSpriteId (uint)
	glEnableVertexArrayAttrib(vao_, 3);
	glVertexArrayAttribIFormat(vao_, 3, 1, GL_UNSIGNED_INT, offsetof(TileInstance, sprite_id));
	glVertexArrayAttribBinding(vao_, 3, 1);

	// Loc 4: aFlags (uint)
	glEnableVertexArrayAttrib(vao_, 4);
	glVertexArrayAttribIFormat(vao_, 4, 1, GL_UNSIGNED_INT, offsetof(TileInstance, flags));
	glVertexArrayAttribBinding(vao_, 4, 1);

	// Loc 5: aTint (vec4: r, g, b, a)
	glEnableVertexArrayAttrib(vao_, 5);
	glVertexArrayAttribFormat(vao_, 5, 4, GL_FLOAT, GL_FALSE, offsetof(TileInstance, r));
	glVertexArrayAttribBinding(vao_, 5, 1);

	spdlog::info("[ChunkCache] Initialized successfully (VAO: {}, Max capacity: {} chunks, Target: {} chunks)",
		vao_, MAX_CACHED_CHUNKS, TARGET_CACHED_CHUNKS);
	return true;
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

void ChunkCacheManager::bakeChunk(CachedChunk& chunk, const Map& map, const RenderFrameContext& ctx) {
	bake_buffer_.clear();
	chunk.dynamic_tiles.clear();

	const int32_t base_x = chunk.coord.cx * CHUNK_SIZE;
	const int32_t base_y = chunk.coord.cy * CHUNK_SIZE;
	const int32_t z = chunk.coord.z;

	const int32_t cell_x = chunk.coord.cx >> 2;
	const int32_t cell_y = chunk.coord.cy >> 2;
	const int32_t chunk_ix = chunk.coord.cx & 3;
	const int32_t chunk_iy = chunk.coord.cy & 3;

	const Floor* floors[4][4] = {};
	bool any_floor = false;

	// Single lookup for the containing 64x64 cell
	const auto& grid = map.getGrid();
	const uint64_t cell_key = SpatialHashGrid::makeKeyFromCell(cell_x, cell_y);
	const size_t cell_idx = grid.findCellIndex(cell_key);

	if (cell_idx < grid.cellCount()) {
		const auto* cell_ptr = grid.getCell(cell_idx);
		if (cell_ptr) {
			const auto& cell = *cell_ptr;
			for (int ny = 0; ny < 4; ++ny) {
				const int node_y = (chunk_iy << 2) + ny;
				const int row_base = node_y << 4; // * 16
				for (int nx = 0; nx < 4; ++nx) {
					const int node_x = (chunk_ix << 2) + nx;
					const MapNode* nd = cell.nodes[row_base + node_x].get();
					if (nd) {
						floors[nx][ny] = nd->getFloor(z);
						if (floors[nx][ny]) {
							any_floor = true;
						}
					}
				}
			}
		}
	}

	if (!any_floor) {
		chunk.is_empty = true;
		chunk.is_dirty = false;
		chunk.instance_count = 0;
		return;
	}

	auto pushSpriteInstances = [&](GameSprite* spr, const SpritePatterns& pat, int draw_base_x, int draw_base_y, float rf, float gf, float bf, float af) {
		const bool is_simple = (spr->width == 1 && spr->height == 1 && spr->layers == 1);
		if (is_simple) {
			const AtlasRegion* reg = nullptr;
			if (spr->is_simple && pat.subtype == -1 && pat.x == 0 && pat.y == 0 && pat.z == 0 && pat.frame == 0) {
				reg = spr->getCachedDefaultRegion();
			}
			if (!reg) {
				reg = spr->getAtlasRegion(0, 0, 0, pat.subtype, pat.x, pat.y, pat.z, pat.frame);
			}
			if (reg && reg->debug_sprite_id != AtlasRegion::INVALID_SENTINEL) {
				TileInstance inst;
				inst.x = static_cast<float>(draw_base_x);
				inst.y = static_cast<float>(draw_base_y);
				inst.w = static_cast<float>(reg->pixel_width);
				inst.h = static_cast<float>(reg->pixel_height);
				inst.sprite_id = reg->debug_sprite_id;
				inst.flags = 0;
				inst.r = rf;
				inst.g = gf;
				inst.b = bf;
				inst.a = af;
				bake_buffer_.push_back(inst);
			}
		} else {
			const auto composite_metrics = spr->getPlainLayoutMetrics(pat.subtype, pat.x, pat.y, pat.z, pat.frame);
			int x_offset = 0;
			for (int cx = 0; cx < composite_metrics.num_columns; ++cx) {
				int y_offset = 0;
				for (int cy = 0; cy < composite_metrics.num_rows; ++cy) {
					for (int cf = 0; cf < spr->layers; ++cf) {
						const AtlasRegion* reg = spr->getAtlasRegion(cx, cy, cf, pat.subtype, pat.x, pat.y, pat.z, pat.frame);
						if (reg && reg->debug_sprite_id != AtlasRegion::INVALID_SENTINEL) {
							TileInstance inst;
							inst.x = static_cast<float>(draw_base_x - x_offset);
							inst.y = static_cast<float>(draw_base_y - y_offset);
							inst.w = static_cast<float>(reg->pixel_width);
							inst.h = static_cast<float>(reg->pixel_height);
							inst.sprite_id = reg->debug_sprite_id;
							inst.flags = 0;
							inst.r = rf;
							inst.g = gf;
							inst.b = bf;
							inst.a = af;
							bake_buffer_.push_back(inst);
						}
					}
					y_offset += composite_metrics.row_heights[cy];
				}
				x_offset += composite_metrics.column_widths[cx];
			}
		}
	};

	// =========================================================================
	// Pass 1: Static Terrain Ground & Ground Borders (Floor base layer)
	// Render all base ground and ground borders across the chunk FIRST,
	// so that objects, structures, and multi-tile sprites (2x1, 1x2, 2x2, etc.)
	// always render cleanly on top of terrain transitions without border clipping.
	// =========================================================================
	for (int d = 0; d < 2 * CHUNK_SIZE - 1; ++d) {
		for (int tx = 0; tx <= d && tx < CHUNK_SIZE; ++tx) {
			const int ty = d - tx;
			if (ty >= CHUNK_SIZE) {
				continue;
			}

			const Floor* fl = floors[tx >> 2][ty >> 2];
			if (!fl) {
				continue;
			}

			const int loc_idx = (tx & 3) * 4 + (ty & 3);
			const TileLocation* loc = &fl->locs[loc_idx];
			const Tile* tile = loc->get();
			if (!tile) {
				continue;
			}
			if (ctx.options.show_only_modified && !tile->isModified()) {
				continue;
			}

			const int x = base_x + tx;
			const int y = base_y + ty;

			uint8_t gr = 255, gg = 255, gb = 255;
			if (!ctx.options.show_as_minimap && (ctx.options.hasTileColorModifiers() || loc->getSpawnCount() > 0)) {
				TileColorCalculator::Calculate(tile, ctx.options, ctx.current_house_id, loc->getSpawnCount(), gr, gg, gb);
			}

			// 1. Static terrain ground (water, grass, dirt, lava, etc.)
			if (tile->ground) {
				const ItemDefinitionView git = tile->ground->getDefinition();
				if (git) {
					GameSprite* gspr = ctx.gfx.getGameSprite(git.clientId());
					if (gspr) {
						const SpritePatterns g_pat = PatternCalculator::Calculate(gspr, git, tile->ground.get(), tile, Position(x, y, z), 0);
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

						pushSpriteInstances(gspr, g_pat, ground_x, ground_y,
							static_cast<float>(r) * (1.0f / 255.0f),
							static_cast<float>(g) * (1.0f / 255.0f),
							static_cast<float>(b) * (1.0f / 255.0f),
							1.0f);
					}
				}
			}

			// 2. Static ground borders (coastlines, grass edges, sand borders, etc.)
			for (const auto& item : tile->items) {
				if (!item || !item->isBorder() || item->isInvalidOTBMItem()) {
					continue;
				}
				const ItemDefinitionView it = item->getDefinition();
				if (!it) {
					continue;
				}
				GameSprite* ispr = ctx.gfx.getGameSprite(it.clientId());
				if (!ispr || ispr->isAnimated()) {
					continue;
				}

				const SpritePatterns i_pat = PatternCalculator::Calculate(ispr, it, item.get(), tile, Position(x, y, z), 0);
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
					1.0f);
			}
		}
	}

	// =========================================================================
	// Pass 2: Static Objects, Structures & Elevated Items (Non-border items)
	// Rendered in strict diagonal Painter's Algorithm order (North-West to South-East)
	// with elevation stacking. Multi-tile sprites (extending North/West) correctly
	// overlay terrain borders baked in Pass 1.
	// =========================================================================
	for (int d = 0; d < 2 * CHUNK_SIZE - 1; ++d) {
		for (int tx = 0; tx <= d && tx < CHUNK_SIZE; ++tx) {
			const int ty = d - tx;
			if (ty >= CHUNK_SIZE) {
				continue;
			}

			const Floor* fl = floors[tx >> 2][ty >> 2];
			if (!fl) {
				continue;
			}

			const int loc_idx = (tx & 3) * 4 + (ty & 3);
			const TileLocation* loc = &fl->locs[loc_idx];
			const Tile* tile = loc->get();
			if (!tile) {
				continue;
			}
			if (ctx.options.show_only_modified && !tile->isModified()) {
				continue;
			}

			const int x = base_x + tx;
			const int y = base_y + ty;

			bool is_dynamic = false;
			if (tile->creature || tile->spawn || loc->getSpawnCount() > 0 || loc->getWaypointCount() > 0 ||
				loc->getTownCount() > 0 || loc->getHouseExits() != nullptr || tile->invalidZones ||
				(tile->ground && tile->ground->isInvalidOTBMItem())) {
				is_dynamic = true;
			}

			int elev = 0;
			for (const auto& item : tile->items) {
				if (!item) {
					continue;
				}
				if (item->isBorder()) {
					if (const ItemDefinitionView it = item->getDefinition()) {
						if (GameSprite* ispr = ctx.gfx.getGameSprite(it.clientId())) {
							if (ispr->isAnimated()) {
								is_dynamic = true;
							}
						}
					}
					continue;
				}
				if (item->isInvalidOTBMItem()) {
					is_dynamic = true;
					continue;
				}
				const ItemDefinitionView it = item->getDefinition();
				if (!it) {
					continue;
				}
				GameSprite* ispr = ctx.gfx.getGameSprite(it.clientId());
				if (!ispr) {
					continue;
				}
				if (ispr->isAnimated()) {
					is_dynamic = true;
					if (ispr->hasElevation()) {
						elev += ispr->draw_height;
					}
					continue;
				}

				const auto [draw_offset_x, draw_offset_y] = ispr->getDrawOffset();
				const int item_x = x * 32 - elev - draw_offset_x;
				const int item_y = y * 32 - elev - draw_offset_y;

				const SpritePatterns i_pat = PatternCalculator::Calculate(ispr, it, item.get(), tile, Position(x, y, z), 0);
				if (!ispr->isSimpleAndLoaded()) {
					rme::collectTileSprites(ispr, i_pat.x, i_pat.y, i_pat.z, i_pat.frame);
				}

				uint8_t r = 255, g = 255, b = 255, a = 255;
				if (item->isSelected()) {
					r >>= 1;
					g >>= 1;
					b >>= 1;
				} else if (ctx.options.extended_house_shader && ctx.options.show_houses && tile->isHouseTile()) {
					TileColorCalculator::GetHouseColor(tile->getHouseID(), r, g, b);
				}

				pushSpriteInstances(ispr, i_pat, item_x, item_y,
					static_cast<float>(r) * (1.0f / 255.0f),
					static_cast<float>(g) * (1.0f / 255.0f),
					static_cast<float>(b) * (1.0f / 255.0f),
					static_cast<float>(a) * (1.0f / 255.0f));

				if (ispr->hasElevation()) {
					elev += ispr->draw_height;
				}
			}

			if (is_dynamic) {
				chunk.dynamic_tiles.push_back(DynamicTileInfo{ static_cast<uint8_t>(tx), static_cast<uint8_t>(ty) });
			}
		}
	}

	uploadChunk(chunk, bake_buffer_);
	chunk.is_dirty = false;
}

void ChunkCacheManager::advanceFrame(int current_floor) {
	++current_frame_;
	if (current_frame_ % PRUNE_INTERVAL_FRAMES == 0) {
		prune(current_floor);
	}
}

void ChunkCacheManager::renderFloor(
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
	const int min_cx = bounds.start_x >> 4;
	const int max_cx = (bounds.end_x + 15) >> 4;
	const int min_cy = bounds.start_y >> 4;
	const int max_cy = (bounds.end_y + 15) >> 4;

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
	shader_.SetVec4("uGlobalTint", glm::vec4(1.0f));

	atlas.bind(0);
	atlas.bindLUT(SpriteAtlasLUT::SSBO_BINDING_INDEX);

	glBindVertexArray(vao_);

	// Reset active visible chunk list for this floor
	active_visible_chunks_.clear();
	active_floor_ = map_z;

	uint32_t baked_count = 0;
	uint32_t rendered_chunk_count = 0;
	uint32_t rendered_instance_count = 0;

	// Sparse Query: Touches ONLY populated chunks on map_z!
	map.visitPopulatedChunks(min_cx, min_cy, max_cx, max_cy, map_z, [&](int cx, int cy) {
		const ChunkCoord coord{ cx, cy, map_z };
		CachedChunk& chunk = getOrCreateChunk(coord);
		if (chunk.is_dirty) {
			bakeChunk(chunk, map, ctx);
			++baked_count;
		}
		chunk.last_accessed_frame = current_frame_;

		if (!chunk.is_empty && chunk.instance_count > 0 && chunk.vbo != 0) {
			glVertexArrayVertexBuffer(vao_, 1, chunk.vbo, 0, sizeof(TileInstance));
			glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(chunk.instance_count));
			++rendered_chunk_count;
			rendered_instance_count += chunk.instance_count;
		}

		active_visible_chunks_.push_back(&chunk);
	});

	if (baked_count > 0) {
		spdlog::info("[ChunkCache] Floor {}: Baked {} new/dirty chunk(s) | Visible: {} chunks ({} instances) | Total cached: {}/{}",
			map_z, baked_count, rendered_chunk_count, rendered_instance_count, cached_chunks_.size(), MAX_CACHED_CHUNKS);
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
		if (chunk.is_empty && is_far_floor && age > FAR_FLOOR_FRAME_THRESHOLD) {
			it = cached_chunks_.erase(it);
			++empty_evicted;
			continue;
		}

		++it;
	}

	size_t lru_evicted = 0;
	// Tier 3: Hard capacity ceiling (LRU eviction down to TARGET_CACHED_CHUNKS)
	if (cached_chunks_.size() > MAX_CACHED_CHUNKS) {
		const size_t needed = cached_chunks_.size() - TARGET_CACHED_CHUNKS;
		evictOldest(needed);
		lru_evicted = needed;
	}

	const size_t total_evicted = empty_evicted + lru_evicted;
	if (total_evicted > 0) {
		spdlog::info("[ChunkCache] Prune (frame {}): Evicted {} chunk(s) ({} empty, {} LRU) | Remaining: {}/{} (~{:.1f} MB VRAM)",
			current_frame_, total_evicted, empty_evicted, lru_evicted,
			cached_chunks_.size(), MAX_CACHED_CHUNKS, (cached_chunks_.size() * 3.5) / 1024.0);
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
		MAX_CACHED_CHUNKS, num_evict, cached_chunks_.size());
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

	const int offset = (map_z <= GROUND_LAYER)
		? (GROUND_LAYER - map_z) * TILE_SIZE
		: TILE_SIZE * (ctx.view.floor - map_z);

	const int base_draw_x = -ctx.view.view_scroll_x - offset;
	const int base_draw_y = -ctx.view.view_scroll_y - offset;

	// Direct iteration of active visible chunks gathered in renderFloor!
	// Zero O(W x H) bounding box iteration, zero hash map lookups.
	for (const CachedChunk* chunk_ptr : active_visible_chunks_) {
		if (!chunk_ptr || chunk_ptr->dynamic_tiles.empty()) {
			continue;
		}

		const auto& chunk = *chunk_ptr;
		const int chunk_base_x = chunk.coord.cx * CHUNK_SIZE;
		const int chunk_base_y = chunk.coord.cy * CHUNK_SIZE;

		for (const auto& dt : chunk.dynamic_tiles) {
			const int x = chunk_base_x + dt.rel_x;
			const int y = chunk_base_y + dt.rel_y;

			const int draw_x = x * TILE_SIZE + base_draw_x;
			const int draw_y = y * TILE_SIZE + base_draw_y;

			if (!ctx.view.IsPixelVisible(draw_x, draw_y)) {
				continue;
			}

			const TileLocation* loc = map.getTileL(x, y, map_z);
			if (!loc) {
				continue;
			}

			const Tile* tile_above = (map_z == GROUND_LAYER + 1) ? map.getTile(x, y, GROUND_LAYER) : nullptr;
			tile_renderer.RenderDynamicPasses(sprite_batch, loc, ctx, draw_x, draw_y, tile_above);
		}
	}
}
