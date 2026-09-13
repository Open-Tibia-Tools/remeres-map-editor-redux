//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/gui.h"
#include "app/definitions.h"
#include "rendering/drawers/map_layer_drawer.h"
#include "rendering/drawers/tiles/tile_renderer.h"
#include "rendering/drawers/overlays/grid_drawer.h"
#include "editor/editor.h"
#include "live/live_client.h"
#include "map/map.h"
#include "map/map_region.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/light_buffer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/primitive_renderer.h"
#include "rendering/core/sprite_preloader.h"
#include "rendering/core/render_frame_context.h"
#include "rendering/core/render_chunk_cache.h"
#include "rendering/drawers/tiles/tile_extractor.h"
#include "item_definitions/core/item_definition_store.h"

#include <cmath>
#include <limits>

MapLayerDrawer::MapLayerDrawer(TileRenderer* tile_renderer, GridDrawer* grid_drawer, Editor* editor, rme::rendering::RenderChunkCache* chunk_cache) :
	tile_renderer(tile_renderer),
	grid_drawer(grid_drawer),
	editor(editor),
	chunk_cache_(chunk_cache) {
}

MapLayerDrawer::~MapLayerDrawer() {
}

void MapLayerDrawer::Draw(SpriteBatch& sprite_batch, int map_z, bool live_client, const RenderFrameContext& ctx, LightBuffer& light_buffer, bool light_collection_only) {
	const RenderView& view = ctx.view;
	const DrawingOptions& options = ctx.options;

	// Optimization: Pre-calculate offset and base coordinates
	// IsTileVisible does this for every tile, but it's constant per layer/frame.
	// We also skip IsTileVisible because visitLeaves already bounds us to the visible area (with 4-tile alignment),
	// which is well within IsTileVisible's 6-tile margin.
	const int offset = (map_z <= GROUND_LAYER)
		? (GROUND_LAYER - map_z) * TILE_SIZE
		: TILE_SIZE * (view.floor - map_z);

	int nd_start_x = 0;
	int nd_start_y = 0;
	int nd_end_x = 0;
	int nd_end_y = 0;
	int visibility_margin_pixels = PAINTERS_ALGORITHM_SAFETY_MARGIN_PIXELS;
	int visibility_margin_tiles = std::max(1, (visibility_margin_pixels + TILE_SIZE - 1) / TILE_SIZE);

	if (light_collection_only) {
		constexpr int light_collection_margin_pixels = TILE_SIZE * 16;
		const int camera_offset = (view.floor <= GROUND_LAYER)
			? (GROUND_LAYER - view.floor) * TILE_SIZE
			: 0;
		const int max_floor_offset = std::max(std::abs(offset - camera_offset), TILE_SIZE * MAP_MAX_LAYER);
		const int start_x = static_cast<int>(std::floor((view.view_scroll_x - light_collection_margin_pixels - max_floor_offset) / static_cast<float>(TILE_SIZE)));
		const int start_y = static_cast<int>(std::floor((view.view_scroll_y - light_collection_margin_pixels - max_floor_offset) / static_cast<float>(TILE_SIZE)));
		const int end_x = static_cast<int>(std::ceil((view.view_scroll_x + view.logical_width + light_collection_margin_pixels + max_floor_offset) / static_cast<float>(TILE_SIZE)));
		const int end_y = static_cast<int>(std::ceil((view.view_scroll_y + view.logical_height + light_collection_margin_pixels + max_floor_offset) / static_cast<float>(TILE_SIZE)));

		nd_start_x = start_x & ~3;
		nd_start_y = start_y & ~3;
		nd_end_x = (end_x & ~3) + 4;
		nd_end_y = (end_y & ~3) + 4;
	} else {
		nd_start_x = view.start_x & ~3;
		nd_start_y = view.start_y & ~3;
		nd_end_x = (view.end_x & ~3) + 4;
		nd_end_y = (view.end_y & ~3) + 4;
	}

	const int base_screen_x = -view.view_scroll_x - offset;
	const int base_screen_y = -view.view_scroll_y - offset;

	bool draw_lights = options.isDrawLight() && view.zoom <= 10.0;

	auto visitNodeTiles = [&](MapNode* nd, int nd_map_x, int nd_map_y, bool live, auto&& visitor) {
		int node_draw_x = nd_map_x * TILE_SIZE + base_screen_x;
		int node_draw_y = nd_map_y * TILE_SIZE + base_screen_y;

		// Node level culling
		if (!view.IsRectVisible(node_draw_x, node_draw_y, 4 * TILE_SIZE, 4 * TILE_SIZE, visibility_margin_pixels)) {
			return;
		}

		if (live && !nd->isVisible(map_z > GROUND_LAYER)) {
			if (!nd->isRequested(map_z > GROUND_LAYER)) {
				// Request the node
				if (editor->live_manager.GetClient()) {
					editor->live_manager.GetClient()->queryNode(nd_map_x, nd_map_y, map_z > GROUND_LAYER);
				}
				nd->setRequested(map_z > GROUND_LAYER, true);
			}
			grid_drawer->DrawNodeLoadingPlaceholder(sprite_batch, nd_map_x, nd_map_y, view);
			return;
		}

		bool fully_inside = view.IsRectFullyInside(node_draw_x, node_draw_y, 4 * TILE_SIZE, 4 * TILE_SIZE);

		Floor* floor = nd->getFloor(map_z);
		if (!floor) {
			return;
		}

		TileLocation* location = floor->locs.data();
		int draw_x_base = node_draw_x;
		for (int map_x = 0; map_x < 4; ++map_x, draw_x_base += TILE_SIZE) {
			int draw_y = node_draw_y;
			for (int map_y = 0; map_y < 4; ++map_y, ++location, draw_y += TILE_SIZE) {
				// Culling: Skip tiles that are far outside the viewport.
				if (!fully_inside && !view.IsPixelVisible(draw_x_base, draw_y, visibility_margin_pixels)) {
					continue;
				}

				visitor(location, draw_x_base, draw_y);
			}
		}
	};

	auto visitAllVisibleNodes = [&](auto&& visitor) {
		if (live_client) {
			for (int nd_map_x = nd_start_x; nd_map_x <= nd_end_x; nd_map_x += 4) {
				for (int nd_map_y = nd_start_y; nd_map_y <= nd_end_y; nd_map_y += 4) {
					MapNode* nd = editor->map.getLeaf(nd_map_x, nd_map_y);
					if (!nd) {
						nd = editor->map.createLeaf(nd_map_x, nd_map_y);
						nd->setVisible(false, false);
					}
					visitNodeTiles(nd, nd_map_x, nd_map_y, true, visitor);
				}
			}
			return;
		}

		int safe_start_x = nd_start_x - visibility_margin_tiles;
		int safe_start_y = nd_start_y - visibility_margin_tiles;
		int safe_end_x = nd_end_x + visibility_margin_tiles;
		int safe_end_y = nd_end_y + visibility_margin_tiles;

		editor->map.visitLeaves(safe_start_x, safe_start_y, safe_end_x, safe_end_y, [&](MapNode* nd, int nd_map_x, int nd_map_y) {
			visitNodeTiles(nd, nd_map_x, nd_map_y, false, visitor);
		});
	};

	// Chunk Cache & Sequential Batching (Data-Oriented Design)
	if (chunk_cache_ && !live_client && !light_collection_only) {
		const int start_cx = (nd_start_x - visibility_margin_tiles) >> 4;
		const int end_cx = (nd_end_x + visibility_margin_tiles) >> 4;
		const int start_cy = (nd_start_y - visibility_margin_tiles) >> 4;
		const int end_cy = (nd_end_y + visibility_margin_tiles) >> 4;

		std::vector<rme::rendering::RenderChunk*> visible_chunks;
		visible_chunks.reserve((end_cx - start_cx + 1) * (end_cy - start_cy + 1));

		for (int cy = start_cy; cy <= end_cy; ++cy) {
			for (int cx = start_cx; cx <= end_cx; ++cx) {
				rme::rendering::RenderChunk& chunk = chunk_cache_->getOrCreateChunk(cx, cy, map_z);
				if (chunk.dirty) {
					rme::rendering::TileExtractor::ExtractChunk(chunk, editor->map, ctx, *tile_renderer);
				}
				if (!chunk.empty()) {
					visible_chunks.push_back(&chunk);
				}
			}
		}

		const float f_screen_x = static_cast<float>(base_screen_x);
		const float f_screen_y = static_cast<float>(base_screen_y);

		// Pass 1: Sequential batch of all ground sprites across visible chunks
		for (const auto* chunk : visible_chunks) {
			if (!chunk->ground_sprites.empty()) {
				sprite_batch.appendTranslated(
					chunk->ground_sprites.data(),
					chunk->ground_sprites.size(),
					f_screen_x,
					f_screen_y
				);
			}
		}

		// Pass 2: Sequential batch of all item sprites across visible chunks
		for (const auto* chunk : visible_chunks) {
			if (!chunk->item_sprites.empty()) {
				sprite_batch.appendTranslated(
					chunk->item_sprites.data(),
					chunk->item_sprites.size(),
					f_screen_x,
					f_screen_y
				);
			}
		}

		// Dynamic Pass: Tooltips, Creatures, and Lights
		const bool collect_tooltips = options.show_tooltips && (map_z == view.floor);
		const bool draw_creatures = options.show_creatures;

		if (collect_tooltips || draw_creatures || draw_lights) {
			uint32_t floor_light_start = 0;
			if (draw_lights) {
				ASSERT(light_buffer.lights.size() <= std::numeric_limits<uint32_t>::max());
				floor_light_start = static_cast<uint32_t>(light_buffer.lights.size());
			}

			visitAllVisibleNodes([&](const TileLocation* location, int draw_x, int draw_y) {
				if (draw_lights) {
					tile_renderer->RegisterGroundLightOcclusion(location, view, light_buffer, floor_light_start);
					tile_renderer->DrawTile(sprite_batch, location, ctx, draw_x, draw_y, &light_buffer, true);
				}
				if (draw_creatures) {
					tile_renderer->DrawCreature(sprite_batch, location, ctx, draw_x, draw_y, draw_lights ? &light_buffer : nullptr);
				}
				if (collect_tooltips) {
					tile_renderer->CollectTooltips(location, ctx);
				}
			});
		}

		return;
	}

	// Fallback path: un-cached rendering (live client or light collection)
	if (draw_lights && !light_collection_only) {
		ASSERT(light_buffer.lights.size() <= std::numeric_limits<uint32_t>::max());
		const uint32_t floor_light_start = static_cast<uint32_t>(light_buffer.lights.size());
		visitAllVisibleNodes([&](const TileLocation* location, int, int) {
			tile_renderer->RegisterGroundLightOcclusion(location, view, light_buffer, floor_light_start);
		});
	}

	auto drawVisibleTiles = [&](const TileLocation* location, int draw_x, int draw_y) {
		tile_renderer->DrawTile(sprite_batch, location, ctx, draw_x, draw_y, draw_lights ? &light_buffer : nullptr, light_collection_only);
	};

	visitAllVisibleNodes(drawVisibleTiles);
}

void MapLayerDrawer::Draw(SpriteBatch& sprite_batch, int map_z, bool live_client, const RenderView& view, const DrawingOptions& options, LightBuffer& light_buffer, bool light_collection_only) {
	if (!g_gui.gfx.ensureAtlasManager()) {
		return;
	}
	RenderFrameContext ctx {
		.atlas = *g_gui.gfx.getAtlasManager(),
		.gfx = g_gui.gfx,
		.item_definitions = g_item_definitions,
		.options = options,
		.view = view,
		.elapsed_time = g_gui.gfx.getElapsedTime(),
		.current_house_id = static_cast<uint32_t>(options.current_house_id)
	};
	Draw(sprite_batch, map_z, live_client, ctx, light_buffer, light_collection_only);
}
