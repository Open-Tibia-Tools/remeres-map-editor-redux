//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/drawers/cursors/drag_shadow_drawer.h"
#include "map/tile_operations.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/drawers/entities/item_drawer.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/render_frame_context.h"
#include "editor/editor.h"
#include "map/tile.h"
#include "game/sprites.h"

#include "game/item.h"
#include "game/creature.h"
#include "game/spawn.h"

DragShadowDrawer::DragShadowDrawer() {
}

DragShadowDrawer::~DragShadowDrawer() {
}

#include "rendering/core/primitive_renderer.h"

void DragShadowDrawer::draw(SpriteBatch& sprite_batch, Editor& editor, const std::optional<Position>& drag_start, ItemDrawer* item_drawer, SpriteDrawer* sprite_drawer, CreatureDrawer* creature_drawer, const RenderView& view, const DrawingOptions& options, const RenderFrameContext* ctx) {
	if (!drag_start.has_value()) {
		return;
	}

	// Draw dragging shadow
	if (!editor.selection.isBusy() && options.dragging && !options.ingame) {
		for (auto tit = editor.selection.begin(); tit != editor.selection.end(); tit++) {
			Tile* tile = *tit;
			Position pos = tile->getPosition();

			int move_x = drag_start->x - view.mouse_map_x;
			int move_y = drag_start->y - view.mouse_map_y;
			int move_z = drag_start->z - view.floor;

			pos.x -= move_x;
			pos.y -= move_y;
			pos.z -= move_z;

			if (pos.z < 0 || pos.z >= MAP_LAYERS) {
				continue;
			}

			// On screen and dragging?
			if (pos.x + 2 > view.start_x && pos.x < view.end_x && pos.y + 2 > view.start_y && pos.y < view.end_y && (move_x != 0 || move_y != 0 || move_z != 0)) {
				int offset;
				if (pos.z <= GROUND_LAYER) {
					offset = (GROUND_LAYER - pos.z) * TILE_SIZE;
				} else {
					offset = TILE_SIZE * (view.floor - pos.z);
				}

				int draw_x = ((pos.x * TILE_SIZE) - view.view_scroll_x) - offset;
				int draw_y = ((pos.y * TILE_SIZE) - view.view_scroll_y) - offset;

				// save performance when moving large chunks unzoomed
				ItemVector toRender = TileOperations::getSelectedItems(tile, view.zoom > 3.0);
				Tile* desttile = editor.map.getTile(pos);
				for (const auto& item : toRender) {
					if (desttile) {
						BlitItemParams params(desttile, item, options);
						params.ephemeral = true;
						params.red = 160;
						params.green = 160;
						params.blue = 160;
						params.alpha = 160;
						params.ctx = ctx;
						item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
					} else {
						BlitItemParams params(pos, item, options);
						params.ephemeral = true;
						params.red = 160;
						params.green = 160;
						params.blue = 160;
						params.alpha = 160;
						params.ctx = ctx;
						item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
					}
				}

				// save performance when moving large chunks unzoomed
				if (view.zoom <= 3.0) {
					if (tile->creature && tile->creature->isSelected() && options.show_creatures) {
						creature_drawer->BlitCreature(sprite_batch, sprite_drawer, draw_x, draw_y, tile->creature.get(), CreatureDrawOptions {
							.color = DrawColor(160, 160, 160, 160),
							.ctx = ctx
						});
					}
					if (tile->spawn && tile->spawn->isSelected()) {
						sprite_drawer->BlitSprite(sprite_batch, draw_x, draw_y, SPRITE_SPAWN, DrawColor(160, 160, 160, 160), ctx);
					}
				}
			}
		}
	}
}
