//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/drawers/overlays/brush_overlay_drawer.h"
#include "rendering/drawers/entities/item_drawer.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/drawers/cursors/brush_cursor_drawer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/primitive_renderer.h"
#include "rendering/core/graphics.h"
#include "rendering/core/render_frame_context.h"
#include "app/settings.h"
#include "editor/editor.h"
#include "brushes/managers/brush_manager.h"
#include "game/sprites.h"
#include "game/outfit.h"
#include "app/definitions.h"
#include "game/creatures.h"

#include "brushes/brush.h"

#include "brushes/doodad/doodad_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/flag/flag_brush.h"
#include "brushes/border/optional_border_brush.h"

// Helper to get color from config
glm::vec4 BrushOverlayDrawer::get_brush_color(BrushColor color, const Settings& settings) {
	glm::vec4 c(1.0f);
	switch (color) {
		case COLOR_BRUSH:
			c = glm::vec4(
				settings.getInteger(Config::CURSOR_RED) / 255.0f,
				settings.getInteger(Config::CURSOR_GREEN) / 255.0f,
				settings.getInteger(Config::CURSOR_BLUE) / 255.0f,
				settings.getInteger(Config::CURSOR_ALPHA) / 255.0f
			);
			break;

		case COLOR_FLAG_BRUSH:
		case COLOR_HOUSE_BRUSH:
			c = glm::vec4(
				settings.getInteger(Config::CURSOR_ALT_RED) / 255.0f,
				settings.getInteger(Config::CURSOR_ALT_GREEN) / 255.0f,
				settings.getInteger(Config::CURSOR_ALT_BLUE) / 255.0f,
				settings.getInteger(Config::CURSOR_ALT_ALPHA) / 255.0f
			);
			break;

		case COLOR_SPAWN_BRUSH:
		case COLOR_ERASER:
		case COLOR_INVALID:
			c = glm::vec4(166.0f / 255.0f, 0.0f, 0.0f, 128.0f / 255.0f);
			break;

		case COLOR_VALID:
			c = glm::vec4(0.0f, 166.0f / 255.0f, 0.0f, 128.0f / 255.0f);
			break;

		default:
			c = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f);
			break;
	}
	return c;
}

glm::vec4 BrushOverlayDrawer::get_check_color(Brush* brush, Editor& editor, const Position& pos, const Settings& settings) {
	if (brush->canDraw(&editor.map, pos)) {
		return get_brush_color(COLOR_VALID, settings);
	} else {
		return get_brush_color(COLOR_INVALID, settings);
	}
}

BrushOverlayDrawer::BrushOverlayDrawer() {
}

BrushOverlayDrawer::~BrushOverlayDrawer() {
}

void BrushOverlayDrawer::draw(
	SpriteBatch& sprite_batch,
	PrimitiveRenderer& primitive_renderer,
	BrushCursorDrawer* brush_cursor_drawer,
	const BrushOverlayDragState& drag_state,
	ItemDrawer* item_drawer,
	SpriteDrawer* sprite_drawer,
	CreatureDrawer* creature_drawer,
	const RenderView& view,
	const DrawingOptions& options,
	Editor& editor,
	const AtlasManager& atlas,
	const RenderFrameContext& ctx,
	const Settings* settings,
	const BrushManager* brush_manager
) {
	if (!options.is_drawing_mode) {
		return;
	}
	const BrushManager& bm = brush_manager ? *brush_manager : g_brush_manager;
	const Settings& cfg = settings ? *settings : g_settings;

	Brush* brush = bm.GetCurrentBrush();
	if (!brush) {
		return;
	}
	if (options.ingame) {
		return;
	}

	BrushColor brushColorType = COLOR_BLANK;
	if (brush->is<TerrainBrush>() || brush->is<TableBrush>() || brush->is<CarpetBrush>()) {
		brushColorType = COLOR_BRUSH;
	} else if (brush->is<HouseBrush>()) {
		brushColorType = COLOR_HOUSE_BRUSH;
	} else if (brush->is<FlagBrush>()) {
		brushColorType = COLOR_FLAG_BRUSH;
	} else if (brush->is<SpawnBrush>()) {
		brushColorType = COLOR_SPAWN_BRUSH;
	} else if (brush->is<EraserBrush>()) {
		brushColorType = COLOR_ERASER;
	}

	glm::vec4 brushColor = get_brush_color(brushColorType, cfg);

	if (drag_state.is_dragging_draw) {
		ASSERT(brush->canDrag());

		if (brush->is<WallBrush>()) {
			int last_click_start_map_x = std::min(drag_state.last_click_map_x, view.mouse_map_x);
			int last_click_start_map_y = std::min(drag_state.last_click_map_y, view.mouse_map_y);
			int last_click_end_map_x = std::max(drag_state.last_click_map_x, view.mouse_map_x) + 1;
			int last_click_end_map_y = std::max(drag_state.last_click_map_y, view.mouse_map_y) + 1;

			int last_click_start_sx = last_click_start_map_x * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
			int last_click_start_sy = last_click_start_map_y * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
			int last_click_end_sx = last_click_end_map_x * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
			int last_click_end_sy = last_click_end_map_y * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();

			int delta_x = last_click_end_sx - last_click_start_sx;
			int delta_y = last_click_end_sy - last_click_start_sy;

			// Top
			sprite_batch.drawRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy), static_cast<float>(last_click_end_sx - last_click_start_sx), static_cast<float>(TILE_SIZE), brushColor, atlas);

			// Bottom
			if (delta_y > TILE_SIZE) {
				sprite_batch.drawRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_end_sy - TILE_SIZE), static_cast<float>(last_click_end_sx - last_click_start_sx), static_cast<float>(TILE_SIZE), brushColor, atlas);
			}

			// Right
			if (delta_x > TILE_SIZE && delta_y > TILE_SIZE) {
				float h = (last_click_end_sy - TILE_SIZE) - (last_click_start_sy + TILE_SIZE);
				sprite_batch.drawRect(static_cast<float>(last_click_end_sx - TILE_SIZE), static_cast<float>(last_click_start_sy + TILE_SIZE), static_cast<float>(TILE_SIZE), h, brushColor, atlas);
			}

			// Left
			if (delta_y > TILE_SIZE) {
				float h = (last_click_end_sy - TILE_SIZE) - (last_click_start_sy + TILE_SIZE);
				sprite_batch.drawRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy + TILE_SIZE), static_cast<float>(TILE_SIZE), h, brushColor, atlas);
			}
		} else {
			// if (brush->is<RAWBrush>()) { glEnable(GL_TEXTURE_2D); } -> handled by DrawRawBrush or BatchRenderer

			if (bm.GetBrushShape() == BRUSHSHAPE_SQUARE || brush->is<SpawnBrush>()) {
				if (brush->is<RAWBrush>() || brush->is<OptionalBorderBrush>()) {
					int start_x, end_x;
					int start_y, end_y;

					if (view.mouse_map_x < drag_state.last_click_map_x) {
						start_x = view.mouse_map_x;
						end_x = drag_state.last_click_map_x;
					} else {
						start_x = drag_state.last_click_map_x;
						end_x = view.mouse_map_x;
					}
					if (view.mouse_map_y < drag_state.last_click_map_y) {
						start_y = view.mouse_map_y;
						end_y = drag_state.last_click_map_y;
					} else {
						start_y = drag_state.last_click_map_y;
						end_y = view.mouse_map_y;
					}

					RAWBrush* raw_brush = nullptr;
					if (brush->is<RAWBrush>()) {
						raw_brush = brush->as<RAWBrush>();
					}

					for (int y = start_y; y <= end_y; y++) {
						int cy = y * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
						for (int x = start_x; x <= end_x; x++) {
							int cx = x * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
							if (brush->is<OptionalBorderBrush>()) {
								sprite_batch.drawRect(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE), get_check_color(brush, editor, Position(x, y, view.floor), cfg), atlas);
							} else {
								item_drawer->DrawRawBrush(sprite_batch, sprite_drawer, cx, cy, raw_brush->getItemID(), 160, 160, 160, 160, &ctx);
							}
						}
					}
				} else {
					int last_click_start_map_x = std::min(drag_state.last_click_map_x, view.mouse_map_x);
					int last_click_start_map_y = std::min(drag_state.last_click_map_y, view.mouse_map_y);
					int last_click_end_map_x = std::max(drag_state.last_click_map_x, view.mouse_map_x) + 1;
					int last_click_end_map_y = std::max(drag_state.last_click_map_y, view.mouse_map_y) + 1;

					int last_click_start_sx = last_click_start_map_x * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
					int last_click_start_sy = last_click_start_map_y * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
					int last_click_end_sx = last_click_end_map_x * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
					int last_click_end_sy = last_click_end_map_y * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();

					float w = last_click_end_sx - last_click_start_sx;
					float h = last_click_end_sy - last_click_start_sy;
					bool autoborder_active = cfg.getInteger(Config::USE_AUTOMAGIC) && brush->needBorders();
					if (autoborder_active) {
						// Draw outline only
						float thickness = 1.0f; // Thin border

						// Top
						sprite_batch.drawRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy), w, thickness, brushColor, atlas);
						// Bottom
						sprite_batch.drawRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy + h - thickness), w, thickness, brushColor, atlas);
						// Left
						sprite_batch.drawRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy + thickness), thickness, h - 2 * thickness, brushColor, atlas);
						// Right
						sprite_batch.drawRect(static_cast<float>(last_click_start_sx + w - thickness), static_cast<float>(last_click_start_sy + thickness), thickness, h - 2 * thickness, brushColor, atlas);
					} else {
						sprite_batch.drawRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy), w, h, brushColor, atlas);
					}
				}
			} else if (bm.GetBrushShape() == BRUSHSHAPE_CIRCLE) {
				// Calculate drawing offsets
				int start_x, end_x;
				int start_y, end_y;
				int width = std::max(
					std::abs(std::max(view.mouse_map_y, drag_state.last_click_map_y) - std::min(view.mouse_map_y, drag_state.last_click_map_y)),
					std::abs(std::max(view.mouse_map_x, drag_state.last_click_map_x) - std::min(view.mouse_map_x, drag_state.last_click_map_x))
				);

				if (view.mouse_map_x < drag_state.last_click_map_x) {
					start_x = drag_state.last_click_map_x - width;
					end_x = drag_state.last_click_map_x;
				} else {
					start_x = drag_state.last_click_map_x;
					end_x = drag_state.last_click_map_x + width;
				}

				if (view.mouse_map_y < drag_state.last_click_map_y) {
					start_y = drag_state.last_click_map_y - width;
					end_y = drag_state.last_click_map_y;
				} else {
					start_y = drag_state.last_click_map_y;
					end_y = drag_state.last_click_map_y + width;
				}

				int center_x = start_x + (end_x - start_x) / 2;
				int center_y = start_y + (end_y - start_y) / 2;
				float radii = width / 2.0f + 0.005f;

				RAWBrush* raw_brush = nullptr;
				if (brush->is<RAWBrush>()) {
					raw_brush = brush->as<RAWBrush>();
				}

				for (int y = start_y - 1; y <= end_y + 1; y++) {
					int cy = y * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
					float dy = center_y - y;
					for (int x = start_x - 1; x <= end_x + 1; x++) {
						int cx = x * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();

						float dx = center_x - x;
						float distance = sqrt(dx * dx + dy * dy);
						if (distance < radii) {
							if (brush->is<RAWBrush>()) {
								item_drawer->DrawRawBrush(sprite_batch, sprite_drawer, cx, cy, raw_brush->getItemID(), 160, 160, 160, 160, &ctx);
							} else {
								sprite_batch.drawRect(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE), brushColor, atlas);
							}
						}
					}
				}
			}

			// if (brush->is<RAWBrush>()) { glDisable(GL_TEXTURE_2D); }
		}
	} else {
		const BrushFootprint footprint = bm.GetBrushFootprint();
		if (brush->is<WallBrush>()) {
			int start_map_x = view.mouse_map_x + footprint.min_offset_x;
			int start_map_y = view.mouse_map_y + footprint.min_offset_y;
			int end_map_x = view.mouse_map_x + footprint.max_offset_x + 1;
			int end_map_y = view.mouse_map_y + footprint.max_offset_y + 1;

			int start_sx = start_map_x * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
			int start_sy = start_map_y * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
			int end_sx = end_map_x * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
			int end_sy = end_map_y * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();

			int delta_x = end_sx - start_sx;
			int delta_y = end_sy - start_sy;

			// Top
			sprite_batch.drawRect(static_cast<float>(start_sx), static_cast<float>(start_sy), static_cast<float>(end_sx - start_sx), static_cast<float>(TILE_SIZE), brushColor, atlas);

			// Bottom
			if (delta_y > TILE_SIZE) {
				sprite_batch.drawRect(static_cast<float>(start_sx), static_cast<float>(end_sy - TILE_SIZE), static_cast<float>(end_sx - start_sx), static_cast<float>(TILE_SIZE), brushColor, atlas);
			}

			// Right
			if (delta_x > TILE_SIZE && delta_y > TILE_SIZE) {
				float h = static_cast<float>(end_sy - start_sy - 2 * TILE_SIZE);
				sprite_batch.drawRect(static_cast<float>(end_sx - TILE_SIZE), static_cast<float>(start_sy + TILE_SIZE), static_cast<float>(TILE_SIZE), h, brushColor, atlas);
			}

			// Left
			if (delta_y > TILE_SIZE) {
				float h = static_cast<float>(end_sy - start_sy - 2 * TILE_SIZE);
				sprite_batch.drawRect(static_cast<float>(start_sx), static_cast<float>(start_sy + TILE_SIZE), static_cast<float>(TILE_SIZE), h, brushColor, atlas);
			}
		} else if (brush->is<DoorBrush>()) {
			int cx = (view.mouse_map_x) * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
			int cy = (view.mouse_map_y) * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();

			sprite_batch.drawRect(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE), get_check_color(brush, editor, Position(view.mouse_map_x, view.mouse_map_y, view.floor), cfg), atlas);
		} else if (brush->is<CreatureBrush>()) {
			// glEnable(GL_TEXTURE_2D);
			int cy = (view.mouse_map_y) * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
			int cx = (view.mouse_map_x) * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
			CreatureBrush* creature_brush = brush->as<CreatureBrush>();
			if (creature_brush->canDraw(&editor.map, Position(view.mouse_map_x, view.mouse_map_y, view.floor))) {
				creature_drawer->BlitCreature(sprite_batch, sprite_drawer, cx, cy, creature_brush->getType()->outfit, SOUTH, CreatureDrawOptions { .color = DrawColor(255, 255, 255, 160), .ctx = &ctx });
			} else {
				creature_drawer->BlitCreature(sprite_batch, sprite_drawer, cx, cy, creature_brush->getType()->outfit, SOUTH, CreatureDrawOptions { .color = DrawColor(255, 64, 64, 160), .ctx = &ctx });
			}
			// glDisable(GL_TEXTURE_2D);
		} else if (!brush->is<DoodadBrush>()) {
			RAWBrush* raw_brush = nullptr;
			if (brush->is<RAWBrush>()) { // Textured brush
				// glEnable(GL_TEXTURE_2D);
				raw_brush = brush->as<RAWBrush>();
			}

			for (int y = footprint.min_offset_y - 1; y <= footprint.max_offset_y + 1; y++) {
				int cy = (view.mouse_map_y + y) * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
				for (int x = footprint.min_offset_x - 1; x <= footprint.max_offset_x + 1; x++) {
					int cx = (view.mouse_map_x + x) * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
					if (!footprint.containsOffset(x, y)) {
						continue;
					}

					if (brush->is<RAWBrush>()) {
						item_drawer->DrawRawBrush(sprite_batch, sprite_drawer, cx, cy, raw_brush->getItemID(), 160, 160, 160, 160, &ctx);
					} else {
						if (brush->is<WaypointBrush>()) {
							if (brush_cursor_drawer) {
								uint8_t r, g, b;
								get_color(brush, editor, Position(view.mouse_map_x + x, view.mouse_map_y + y, view.floor), r, g, b);
								brush_cursor_drawer->draw(sprite_batch, primitive_renderer, atlas, cx, cy, brush, r, g, b);
							}
						} else {
							glm::vec4 c = brushColor;
							if (brush->is<HouseExitBrush>() || brush->is<OptionalBorderBrush>()) {
								c = get_check_color(brush, editor, Position(view.mouse_map_x + x, view.mouse_map_y + y, view.floor), cfg);
							}
							sprite_batch.drawRect(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE), c, atlas);
						}
					}
				}
			}

			// if (brush->is<RAWBrush>()) { // Textured brush
			// 	glDisable(GL_TEXTURE_2D);
			// }
		}
	}
}

void BrushOverlayDrawer::get_color(Brush* brush, Editor& editor, const Position& position, uint8_t& r, uint8_t& g, uint8_t& b) {
	if (brush->canDraw(&editor.map, position)) {
		if (brush->is<WaypointBrush>()) {
			r = 0x00;
			g = 0xff, b = 0x00;
		} else {
			r = 0x00;
			g = 0x00, b = 0xff;
		}
	} else {
		r = 0xff;
		g = 0x00, b = 0x00;
	}
}
