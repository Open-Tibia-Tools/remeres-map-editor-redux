#include "rendering/drawers/tiles/floor_drawer.h"
#include "rendering/drawers/entities/item_drawer.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "map/basemap.h"
#include "map/tile.h"

FloorDrawer::FloorDrawer() {
}

FloorDrawer::~FloorDrawer() {
}

void FloorDrawer::draw(SpriteBatch& sprite_batch, ItemDrawer* item_drawer, SpriteDrawer* sprite_drawer, CreatureDrawer* creature_drawer, const RenderView& view, const DrawingOptions& options, BaseMap& map, const RenderFrameContext* ctx) {

	// Draw "transparent higher floor"
	if (view.floor != 8 && view.floor != 0 && options.transparent_floors) {
		const int map_z = view.floor - 1;
		// The floor above is drawn one tile up-left, so use its own bounds
		// rather than the current floor's.
		const ViewBounds bounds = view.getBoundsForFloor(map_z);
		for (int map_x = bounds.start_x; map_x <= bounds.end_x; map_x++) {
			for (int map_y = bounds.start_y; map_y <= bounds.end_y; map_y++) {
				Tile* tile = map.getTile(map_x, map_y, map_z);
				if (tile) {
					int offset;
					if (map_z <= GROUND_LAYER) {
						offset = (GROUND_LAYER - map_z) * TILE_SIZE;
					} else {
						offset = TILE_SIZE * (view.floor - map_z);
					}

					int draw_x = ((map_x * TILE_SIZE) - view.view_scroll_x) - offset;
					int draw_y = ((map_y * TILE_SIZE) - view.view_scroll_y) - offset;

					// Position pos = tile->getPosition();

					if (tile->ground) {
						BlitItemParams params(tile, tile->ground.get(), options);
						params.alpha = 255;
						params.ctx = ctx;
						if (tile->isPZ()) {
							params.red = options.transparent_floors_tint / 2;
							params.green = options.transparent_floors_tint;
							params.blue = options.transparent_floors_tint / 2;
						} else {
							params.red = options.transparent_floors_tint;
							params.green = options.transparent_floors_tint;
							params.blue = options.transparent_floors_tint;
						}
						item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
					}
					if (view.zoom <= 10.0 || !options.hide_items_when_zoomed) {
						for (const auto& item : tile->items) {
							BlitItemParams params(tile, item.get(), options);
							params.alpha = 255;
							params.ctx = ctx;
							params.red = options.transparent_floors_tint;
							params.green = options.transparent_floors_tint;
							params.blue = options.transparent_floors_tint;
							item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
						}
					}
				}
			}
		}
	}
}
