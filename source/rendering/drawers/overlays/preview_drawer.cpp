#include "rendering/drawers/overlays/preview_drawer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/primitive_renderer.h"
#include "rendering/core/render_frame_context.h"
#include "rendering/drawers/entities/item_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "brushes/brush.h"
#include "editor/copybuffer.h"
#include "editor/editor.h"
#include "map/map_region.h"

PreviewDrawer::PreviewDrawer() {
}

PreviewDrawer::~PreviewDrawer() {
}

void PreviewDrawer::draw(SpriteBatch& sprite_batch, bool is_pasting, BaseMap* secondary_map, const RenderView& view, int map_z, const DrawingOptions& options, Editor& editor, ItemDrawer* item_drawer, SpriteDrawer* sprite_drawer, CreatureDrawer* creature_drawer, uint32_t current_house_id, Brush* current_brush, const RenderFrameContext* ctx) {
	if (secondary_map != nullptr && !options.ingame) {
		Brush* brush = current_brush;

		Position normalPos;
		Position to(view.mouse_map_x, view.mouse_map_y, view.floor);

		if (is_pasting) {
			normalPos = editor.copybuffer.getPosition();
		} else if (brush && brush->is<DoodadBrush>()) {
			normalPos = Position(0x8000, 0x8000, 0x8);
		} else {
			normalPos = to;
		}

		const int source_z = normalPos.z + map_z - to.z;
		if (source_z >= 0 && source_z < MAP_LAYERS) {
			const int source_start_x = normalPos.x + view.start_x - to.x;
			const int source_start_y = normalPos.y + view.start_y - to.y;
			const int source_end_x = normalPos.x + view.end_x - to.x;
			const int source_end_y = normalPos.y + view.end_y - to.y;
			const int offset = map_z <= GROUND_LAYER ? (GROUND_LAYER - map_z) * TILE_SIZE : TILE_SIZE * (view.floor - map_z);
			const uint8_t base_alpha = is_pasting ? 128 : 255;

			auto drawPreviewTile = [&](Tile* tile, int map_x, int map_y) {
				int draw_x = ((map_x * TILE_SIZE) - view.view_scroll_x) - offset;
				int draw_y = ((map_y * TILE_SIZE) - view.view_scroll_y) - offset;

				uint8_t r = 255;
				uint8_t g = 255;
				uint8_t b = 255;

				if (tile->ground) {
					if (tile->isBlocking() && options.show_blocking) {
						g = g / 3 * 2;
						b = b / 3 * 2;
					}

					if (tile->isHouseTile() && options.show_houses) {
						if (static_cast<int>(tile->getHouseID()) == current_house_id) {
							r /= 2;
						} else {
							r /= 2;
							g /= 2;
						}
					} else if (options.show_special_tiles && tile->isPZ()) {
						r /= 2;
						b /= 2;
					}
					if (options.show_special_tiles && tile->getMapFlags() & TILESTATE_PVPZONE) {
						r = r / 3 * 2;
						b = b / 3 * 2;
					}
					if (options.show_special_tiles && tile->getMapFlags() & TILESTATE_NOLOGOUT) {
						b /= 2;
					}
					if (options.show_special_tiles && tile->getMapFlags() & TILESTATE_NOPVP) {
						g /= 2;
					}

					BlitItemParams params(tile, tile->ground.get(), options);
					params.ephemeral = true;
					params.red = r;
					params.green = g;
					params.blue = b;
					params.alpha = base_alpha;
					params.ctx = ctx;
					item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
				}

				if (view.zoom <= 10.0 || !options.hide_items_when_zoomed) {
					for (const auto& item : tile->items) {
						BlitItemParams params(tile, item.get(), options);
						params.ephemeral = true;
						params.alpha = base_alpha;
						params.ctx = ctx;
						if (item->isBorder()) {
							params.red = 255;
							params.green = r;
							params.blue = g;
							params.alpha = base_alpha == 255 ? b : base_alpha;
						}
						item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
					}
					if (tile->creature && options.show_creatures) {
						creature_drawer->BlitCreature(sprite_batch, sprite_drawer, draw_x, draw_y, tile->creature.get(), CreatureDrawOptions { .ctx = ctx });
					}
				}
			};

			secondary_map->visitLeaves(source_start_x, source_start_y, source_end_x + 1, source_end_y + 1, [&](MapNode* node, int, int) {
				Floor* floor = node->getFloor(source_z);
				if (!floor) {
					return;
				}

				TileLocation* location = floor->locs.data();
				for (int local_x = 0; local_x < 4; ++local_x) {
					for (int local_y = 0; local_y < 4; ++local_y, ++location) {
						Tile* tile = location->get();
						if (!tile) {
							continue;
						}

						const int map_x = to.x + tile->getX() - normalPos.x;
						const int map_y = to.y + tile->getY() - normalPos.y;
						if (map_x < view.start_x || map_x > view.end_x || map_y < view.start_y || map_y > view.end_y) {
							continue;
						}

						drawPreviewTile(tile, map_x, map_y);
					}
				}
			});
		}

		// Draw highlight on the specific tile under mouse
		// This helps user see where they are pointing in the "chaos"
		Position mousePos(view.mouse_map_x, view.mouse_map_y, view.floor);
		if (mousePos.z == map_z) {
			// Use correct offset logic
			int offset;
			if (map_z <= GROUND_LAYER) {
				offset = (GROUND_LAYER - map_z) * TILE_SIZE;
			} else {
				offset = TILE_SIZE * (view.floor - map_z);
			}
			int draw_x = ((mousePos.x * TILE_SIZE) - view.view_scroll_x) - offset;
			int draw_y = ((mousePos.y * TILE_SIZE) - view.view_scroll_y) - offset;

			const AtlasManager* atlas = ctx ? &ctx->atlas : nullptr;
			if (atlas) {
				// Draw a semi-transparent white box over the tile
				const glm::vec4 highlightColor(1.0f, 1.0f, 1.0f, 0.25f); // 25% white
				sprite_batch.drawRect(static_cast<float>(draw_x), static_cast<float>(draw_y), static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE), highlightColor, *atlas);
			}
		}
	}
}
