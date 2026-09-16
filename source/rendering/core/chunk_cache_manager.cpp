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
#include "rendering/drawers/tiles/tile_renderer.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "rendering/utilities/pattern_calculator.h"
#include "rendering/drawers/tiles/tile_color_calculator.h"
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

	if (!shader_.Load(CHUNK_VERT_SHADER, CHUNK_FRAG_SHADER)) {
		spdlog::error("ChunkCacheManager: Failed to compile chunk shader");
		return false;
	}
	shader_initialized_ = true;

	if (!mega_buffer_.initialize(ChunkMegaBuffer::DEFAULT_CAPACITY)) {
		spdlog::error("ChunkCacheManager: Failed to initialize mega-buffer");
		return false;
	}

	if (!mdi_renderer_.initialize(16384)) {
		spdlog::warn("ChunkCacheManager: MDI unavailable, using fallback execution");
	}

	spdlog::info("ChunkCacheManager initialized successfully");
	return true;
}

void ChunkCacheManager::release() {
	for (auto& [coord, chunk] : cached_chunks_) {
		if (chunk.slice.isValid()) {
			mega_buffer_.free(chunk.slice);
		}
	}
	cached_chunks_.clear();
	mega_buffer_.release();
	mdi_renderer_.cleanup();
	shader_initialized_ = false;
	current_frame_ = 0;
}

void ChunkCacheManager::updateDirtyState(SpatialChangeTracker& change_tracker) {
	if (change_tracker.isAllDirty()) {
		invalidateAll();
		change_tracker.clearDirty();
		return;
	}

	auto dirty_chunks = change_tracker.takeDirtyChunks();
	for (const auto& coord : dirty_chunks) {
		auto it = cached_chunks_.find(coord);
		if (it != cached_chunks_.end()) {
			it->second.is_dirty = true;
		}
	}
}

void ChunkCacheManager::invalidateAll() {
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
		auto [new_it, _] = cached_chunks_.emplace(coord, chunk);
		return new_it->second;
	}
	return it->second;
}

void ChunkCacheManager::bakeChunk(CachedChunk& chunk, const Map& map, const RenderFrameContext& ctx) {
	bake_buffer_.clear();
	chunk.dynamic_tiles.clear();
	bool has_missing_sprites = false;

	const int32_t base_x = chunk.coord.cx * CHUNK_SIZE;
	const int32_t base_y = chunk.coord.cy * CHUNK_SIZE;
	const int32_t z = chunk.coord.z;

	// OTClient-parity diagonal iteration (dx + dy) for strict Painter's Algorithm depth order
	for (int d = 0; d < 2 * CHUNK_SIZE - 1; ++d) {
		for (int tx = 0; tx <= d && tx < CHUNK_SIZE; ++tx) {
			const int ty = d - tx;
			if (ty >= CHUNK_SIZE) {
				continue;
			}

			const int x = base_x + tx;
			const int y = base_y + ty;

			const TileLocation* loc = map.getTileL(x, y, z);
			if (!loc) {
				continue;
			}
			const Tile* tile = loc->get();
			if (!tile) {
				continue;
			}
			if (ctx.options.show_only_modified && !tile->isModified()) {
				continue;
			}

			bool is_dynamic = false;
			if (tile->creature || tile->spawn || loc->getSpawnCount() > 0 || loc->getWaypointCount() > 0 ||
				loc->getTownCount() > 0 || loc->getHouseExits() != nullptr || tile->invalidZones ||
				(tile->ground && tile->ground->isInvalidOTBMItem())) {
				is_dynamic = true;
			}

			// Static terrain ground (water, grass, dirt, lava, etc. - ALWAYS baked into chunk cache!)
			if (tile->ground) {
				const ItemDefinitionView git = tile->ground->getDefinition();
				if (git) {
					GameSprite* gspr = ctx.gfx.getGameSprite(git.clientId());
					if (gspr) {
						const SpritePatterns g_pat = PatternCalculator::Calculate(gspr, git, tile->ground.get(), tile, Position(x, y, z), 0);
						const AtlasRegion* reg = nullptr;
						if (gspr->is_simple && g_pat.subtype == -1 && g_pat.x == 0 && g_pat.y == 0 && g_pat.z == 0 && g_pat.frame == 0) {
							reg = gspr->getCachedDefaultRegion();
						}
						if (!reg) {
							reg = gspr->getAtlasRegion(0, 0, 0, g_pat.subtype, g_pat.x, g_pat.y, g_pat.z, g_pat.frame);
						}
						if (reg && reg->debug_sprite_id != AtlasRegion::INVALID_SENTINEL) {
							uint8_t gr = 255, gg = 255, gb = 255;
							if (tile->ground->isSelected()) {
								gr >>= 1;
								gg >>= 1;
								gb >>= 1;
							} else if (!ctx.options.show_as_minimap && (ctx.options.hasTileColorModifiers() || loc->getSpawnCount() > 0)) {
								TileColorCalculator::Calculate(tile, ctx.options, ctx.current_house_id, loc->getSpawnCount(), gr, gg, gb);
							}

							TileInstance inst;
							inst.x = static_cast<float>(x * 32);
							inst.y = static_cast<float>(y * 32);
							inst.w = static_cast<float>(reg->pixel_width);
							inst.h = static_cast<float>(reg->pixel_height);
							inst.sprite_id = reg->debug_sprite_id;
							inst.flags = 0;
							inst.r = static_cast<float>(gr) * (1.0f / 255.0f);
							inst.g = static_cast<float>(gg) * (1.0f / 255.0f);
							inst.b = static_cast<float>(gb) * (1.0f / 255.0f);
							inst.a = 1.0f;
							bake_buffer_.push_back(inst);
						} else {
							if (!gspr->isSimpleAndLoaded()) {
								rme::collectTileSprites(gspr, g_pat.x, g_pat.y, g_pat.z, g_pat.frame);
							}
							has_missing_sprites = true;
						}
					}
				}
			}

			// Static items on tile with elevation stacking
			int elev = 0;
			for (const auto& item : tile->items) {
				if (!item) {
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

				const SpritePatterns i_pat = PatternCalculator::Calculate(ispr, it, item.get(), tile, Position(x, y, z));

				uint8_t r = 255, g = 255, b = 255, a = 255;
				if (item->isSelected()) {
					r >>= 1;
					g >>= 1;
					b >>= 1;
				} else if (ctx.options.extended_house_shader && ctx.options.show_houses && tile->isHouseTile()) {
					TileColorCalculator::GetHouseColor(tile->getHouseID(), r, g, b);
				}

				const float rf = static_cast<float>(r) * (1.0f / 255.0f);
				const float gf = static_cast<float>(g) * (1.0f / 255.0f);
				const float bf = static_cast<float>(b) * (1.0f / 255.0f);
				const float af = static_cast<float>(a) * (1.0f / 255.0f);

				const bool is_simple_sprite = (ispr->width == 1 && ispr->height == 1 && ispr->layers == 1);
				if (is_simple_sprite) {
					const AtlasRegion* reg = nullptr;
					if (ispr->is_simple && i_pat.subtype == -1 && i_pat.x == 0 && i_pat.y == 0 && i_pat.z == 0) {
						reg = ispr->getCachedDefaultRegion();
					}
					if (!reg) {
						reg = ispr->getAtlasRegion(0, 0, 0, i_pat.subtype, i_pat.x, i_pat.y, i_pat.z, 0);
					}
					if (reg && reg->debug_sprite_id != AtlasRegion::INVALID_SENTINEL) {
						TileInstance inst;
						inst.x = static_cast<float>(item_x);
						inst.y = static_cast<float>(item_y);
						inst.w = static_cast<float>(reg->pixel_width);
						inst.h = static_cast<float>(reg->pixel_height);
						inst.sprite_id = reg->debug_sprite_id;
						inst.flags = 0;
						inst.r = rf;
						inst.g = gf;
						inst.b = bf;
						inst.a = af;
						bake_buffer_.push_back(inst);
					} else {
						if (!ispr->isSimpleAndLoaded()) {
							rme::collectTileSprites(ispr, i_pat.x, i_pat.y, i_pat.z, i_pat.frame);
						}
						has_missing_sprites = true;
					}
				} else {
					const auto composite_metrics = ispr->getPlainLayoutMetrics(i_pat.subtype, i_pat.x, i_pat.y, i_pat.z, 0);
					int x_offset = 0;
					for (int cx = 0; cx < composite_metrics.num_columns; ++cx) {
						int y_offset = 0;
						for (int cy = 0; cy < composite_metrics.num_rows; ++cy) {
							for (int cf = 0; cf < ispr->layers; ++cf) {
								const AtlasRegion* reg = ispr->getAtlasRegion(cx, cy, cf, i_pat.subtype, i_pat.x, i_pat.y, i_pat.z, 0);
								if (reg && reg->debug_sprite_id != AtlasRegion::INVALID_SENTINEL) {
									TileInstance inst;
									inst.x = static_cast<float>(item_x - x_offset);
									inst.y = static_cast<float>(item_y - y_offset);
									inst.w = static_cast<float>(reg->pixel_width);
									inst.h = static_cast<float>(reg->pixel_height);
									inst.sprite_id = reg->debug_sprite_id;
									inst.flags = 0;
									inst.r = rf;
									inst.g = gf;
									inst.b = bf;
									inst.a = af;
									bake_buffer_.push_back(inst);
								} else {
									if (!ispr->isSimpleAndLoaded()) {
										rme::collectTileSprites(ispr, i_pat.x, i_pat.y, i_pat.z, i_pat.frame);
									}
									has_missing_sprites = true;
								}
							}
							y_offset += composite_metrics.row_heights[cy];
						}
						x_offset += composite_metrics.column_widths[cx];
					}
				}

				if (ispr->hasElevation()) {
					elev += ispr->draw_height;
				}
			}

			if (is_dynamic) {
				chunk.dynamic_tiles.push_back(DynamicTileInfo{ static_cast<uint8_t>(tx), static_cast<uint8_t>(ty) });
			}
		}
	}

	if (bake_buffer_.empty()) {
		chunk.is_empty = true;
		if (chunk.slice.isValid()) {
			mega_buffer_.free(chunk.slice);
		}
	} else {
		chunk.is_empty = false;
		const uint32_t needed = static_cast<uint32_t>(bake_buffer_.size());
		if (!chunk.slice.isValid() || chunk.slice.capacity < needed) {
			if (chunk.slice.isValid()) {
				mega_buffer_.free(chunk.slice);
			}
			chunk.slice = mega_buffer_.allocate(needed);
		}
		if (chunk.slice.isValid()) {
			mega_buffer_.upload(chunk.slice, bake_buffer_.data(), needed);
		}
	}

	chunk.is_dirty = has_missing_sprites;
}

void ChunkCacheManager::renderFloor(
	int map_z,
	const Map& map,
	const RenderFrameContext& ctx,
	const glm::mat4& projection,
	const AtlasManager& atlas
) {
	if (!isValid()) {
		return;
	}

	++current_frame_;
	if (current_frame_ % PRUNE_INTERVAL_FRAMES == 0) {
		prune(ctx.view.floor);
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

	mdi_renderer_.clear();

	shader_.Use();
	shader_.SetMat4("uMVP", floor_mvp);
	shader_.SetInt("uAtlas", 0);
	shader_.SetVec4("uGlobalTint", glm::vec4(1.0f));

	atlas.bind(0);
	atlas.bindLUT(SpriteAtlasLUT::SSBO_BINDING_INDEX);

	mega_buffer_.bindVAO();

	auto flushMdiBatch = [&]() {
		if (mdi_renderer_.getCommandCount() == 0) {
			return;
		}
		if (mdi_renderer_.isAvailable()) {
			mdi_renderer_.upload();
			mdi_renderer_.execute();
		} else {
			// Fallback for systems without MDI: glDrawElementsInstancedBaseInstance
			for (const auto& cmd : mdi_renderer_.getCommands()) {
				glDrawElementsInstancedBaseInstance(
					GL_TRIANGLES, cmd.count, GL_UNSIGNED_INT, nullptr,
					cmd.instanceCount, cmd.baseInstance
				);
			}
		}
		mdi_renderer_.clear();
	};

	for (int cy = min_cy; cy <= max_cy; ++cy) {
		for (int cx = min_cx; cx <= max_cx; ++cx) {
			const ChunkCoord coord{ cx, cy, map_z };
			CachedChunk& chunk = getOrCreateChunk(coord);
			if (chunk.is_dirty) {
				bakeChunk(chunk, map, ctx);
			}
			chunk.last_accessed_frame = current_frame_;

			if (!chunk.is_empty && chunk.slice.count > 0) {
				if (static_cast<int>(mdi_renderer_.getCommandCount()) >= mdi_renderer_.getMaxCommands()) {
					flushMdiBatch();
				}
				mdi_renderer_.addDrawCommand(6, chunk.slice.count, 0, 0, chunk.slice.base_instance);
			}
		}
	}

	flushMdiBatch();

	mega_buffer_.unbindVAO();
	shader_.Unuse();
}

void ChunkCacheManager::prune(int current_floor) {
	for (auto it = cached_chunks_.begin(); it != cached_chunks_.end();) {
		const auto& [coord, chunk] = *it;
		if (std::abs(coord.z - current_floor) > 2 && (current_frame_ - chunk.last_accessed_frame) > EVICTION_FRAME_THRESHOLD) {
			if (chunk.slice.isValid()) {
				mega_buffer_.free(const_cast<SlabSlice&>(chunk.slice));
			}
			it = cached_chunks_.erase(it);
		} else {
			++it;
		}
	}
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

	// LOD Policy: When zoomed out beyond threshold, dynamic entities and overlays are culled
	if (ctx.view.zoom >= 10.0 && ctx.options.hide_items_when_zoomed) {
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

	const int base_draw_x = -ctx.view.view_scroll_x - offset;
	const int base_draw_y = -ctx.view.view_scroll_y - offset;

	for (int cy = min_cy; cy <= max_cy; ++cy) {
		for (int cx = min_cx; cx <= max_cx; ++cx) {
			const ChunkCoord coord{ cx, cy, map_z };
			auto it = cached_chunks_.find(coord);
			if (it == cached_chunks_.end() || it->second.dynamic_tiles.empty()) {
				continue;
			}

			const auto& chunk = it->second;
			const int chunk_base_x = cx * CHUNK_SIZE;
			const int chunk_base_y = cy * CHUNK_SIZE;

			for (const auto& dt : chunk.dynamic_tiles) {
				const int x = chunk_base_x + dt.rel_x;
				const int y = chunk_base_y + dt.rel_y;
				const TileLocation* loc = map.getTileL(x, y, map_z);
				if (!loc) {
					continue;
				}

				const int draw_x = x * TILE_SIZE + base_draw_x;
				const int draw_y = y * TILE_SIZE + base_draw_y;

				const Tile* tile_above = (map_z == GROUND_LAYER + 1) ? map.getTile(x, y, GROUND_LAYER) : nullptr;
				tile_renderer.RenderDynamicPasses(sprite_batch, loc, ctx, draw_x, draw_y, tile_above);
			}
		}
	}
}
