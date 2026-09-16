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

void FloorDrawer::draw(SpriteBatch& sprite_batch, ItemDrawer* item_drawer, SpriteDrawer* sprite_drawer, CreatureDrawer* creature_drawer, const RenderView& view, const DrawingOptions& options, BaseMap& map) {

	// Draw "transparent higher floor"
	if (view.floor != 8 && view.floor != 0 && options.transparent_floors) {
		int map_z = view.floor - 1;
		for (int map_x = view.start_x; map_x <= view.end_x; map_x++) {
			for (int map_y = view.start_y; map_y <= view.end_y; map_y++) {
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
						params.alpha = 96;
						if (tile->isPZ()) {
							params.red = 128;
							params.green = 255;
							params.blue = 128;
						} else {
							params.red = 255;
							params.green = 255;
							params.blue = 255;
						}
						item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
					}
					if (view.zoom <= 10.0 || !options.hide_items_when_zoomed) {
						for (const auto& item : tile->items) {
							BlitItemParams params(tile, item.get(), options);
							params.alpha = 96;
							item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
						}
					}
				}
			}
		}
	}
}
