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

#include "rendering/drawers/map_layer_drawer.h"
#include "app/definitions.h"
#include "rendering/drawers/tiles/tile_renderer.h"
#include "rendering/drawers/overlays/grid_drawer.h"
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
#include "item_definitions/core/item_definition_store.h"

#include <cmath>
#include <limits>

MapLayerDrawer::MapLayerDrawer(TileRenderer* tile_renderer, GridDrawer* grid_drawer, Map& map) :
	tile_renderer(tile_renderer),
	grid_drawer(grid_drawer),
	map(map) {
}

MapLayerDrawer::~MapLayerDrawer() {
}

void MapLayerDrawer::Draw(SpriteBatch& sprite_batch, int map_z, LiveClient* live_client, const RenderFrameContext& ctx, LightBuffer& light_buffer, bool light_collection_only) {
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

	if (light_collection_only) {
		constexpr int light_collection_margin_pixels = TILE_SIZE * 16;
		visibility_margin_pixels = light_collection_margin_pixels;
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

	const int visibility_margin_tiles = std::max(1, (visibility_margin_pixels + TILE_SIZE - 1) / TILE_SIZE);

	const int base_screen_x = -view.view_scroll_x - offset;
	const int base_screen_y = -view.view_scroll_y - offset;

	bool draw_lights = options.isDrawLight() && view.zoom <= 10.0;

	const int max_logical_w = static_cast<int>(view.logical_width);
	const int max_logical_h = static_cast<int>(view.logical_height);
	const int min_visible_draw_x = -TILE_SIZE - visibility_margin_pixels;
	const int max_visible_draw_x = max_logical_w + visibility_margin_pixels;
	const int min_visible_draw_y = -TILE_SIZE - visibility_margin_pixels;
	const int max_visible_draw_y = max_logical_h + visibility_margin_pixels;

	auto visitNodeTiles = [&](MapNode* nd, int nd_map_x, int nd_map_y, bool live, auto&& visitor) {
		int node_draw_x = nd_map_x * TILE_SIZE + base_screen_x;
		int node_draw_y = nd_map_y * TILE_SIZE + base_screen_y;

		// Node level culling (integer AABB)
		if (node_draw_x + 4 * TILE_SIZE + visibility_margin_pixels < 0 || node_draw_x - visibility_margin_pixels > max_logical_w ||
			node_draw_y + 4 * TILE_SIZE + visibility_margin_pixels < 0 || node_draw_y - visibility_margin_pixels > max_logical_h) {
			return;
		}

		if (live && !nd->isVisible(map_z > GROUND_LAYER)) {
			if (!nd->isRequested(map_z > GROUND_LAYER)) {
				// Request the node
				if (live_client) {
					live_client->queryNode(nd_map_x, nd_map_y, map_z > GROUND_LAYER);
				}
				nd->setRequested(map_z > GROUND_LAYER, true);
			}
			grid_drawer->DrawNodeLoadingPlaceholder(sprite_batch, nd_map_x, nd_map_y, view, ctx.atlas);
			return;
		}

		const bool fully_inside = (node_draw_x >= 0 && node_draw_x + 4 * TILE_SIZE <= max_logical_w &&
			node_draw_y >= 0 && node_draw_y + 4 * TILE_SIZE <= max_logical_h);

		Floor* floor = nd->getFloor(map_z);
		if (!floor) {
			return;
		}

		Floor* floor_above = (map_z == GROUND_LAYER + 1) ? nd->getFloor(GROUND_LAYER) : nullptr;
		TileLocation* location = floor->locs.data();
		TileLocation* loc_above = floor_above ? floor_above->locs.data() : nullptr;
		int draw_x_base = node_draw_x;
		for (int map_x = 0; map_x < 4; ++map_x, draw_x_base += TILE_SIZE) {
			int draw_y = node_draw_y;
			for (int map_y = 0; map_y < 4; ++map_y, ++location, draw_y += TILE_SIZE) {
				const Tile* tile_above = loc_above ? (loc_above++)->get() : nullptr;

				if (!location->get()) {
					continue;
				}

				// Culling: Skip tiles that are far outside the viewport (fast integer AABB).
				if (!fully_inside && (draw_x_base < min_visible_draw_x || draw_x_base > max_visible_draw_x ||
					draw_y < min_visible_draw_y || draw_y > max_visible_draw_y)) {
					continue;
				}

				visitor(location, draw_x_base, draw_y, tile_above);
			}
		}
	};

	auto visitAllVisibleNodes = [&](auto&& visitor) {
		if (live_client) {
			for (int nd_map_x = nd_start_x; nd_map_x <= nd_end_x; nd_map_x += 4) {
				for (int nd_map_y = nd_start_y; nd_map_y <= nd_end_y; nd_map_y += 4) {
					MapNode* nd = map.getLeaf(nd_map_x, nd_map_y);
					if (!nd) {
						nd = map.createLeaf(nd_map_x, nd_map_y);
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

		map.visitLeaves(safe_start_x, safe_start_y, safe_end_x, safe_end_y, [&](MapNode* nd, int nd_map_x, int nd_map_y) {
			visitNodeTiles(nd, nd_map_x, nd_map_y, false, visitor);
		});
	};

	// OTClient floor-aware light occlusion: capture light count at START of each floor,
	// so opaque ground tiles can record it during DrawTile to block light from floors below
	if (draw_lights && !light_collection_only) {
		light_buffer.SetFloorLightStart();
	}

	LightBuffer* active_light_buffer = draw_lights ? &light_buffer : nullptr;
	auto drawVisibleTiles = [&](const TileLocation* location, int draw_x, int draw_y, const Tile* tile_above) {
		tile_renderer->DrawTile(sprite_batch, location, ctx, draw_x, draw_y, active_light_buffer, light_collection_only, tile_above);
	};

	visitAllVisibleNodes(drawVisibleTiles);
}
