//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/drawers/overlays/brush_overlay_drawer.h"
#include "rendering/drawers/entities/item_drawer.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/graphics.h"
#include "rendering/core/render_frame_context.h"
#include "app/settings.h"
#include "editor/editor.h"
#include "brushes/managers/brush_manager.h"
#include "brushes/managers/doodad_preview_manager.h"
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
#include "brushes/ground/ground_brush.h"
#include "brushes/town/town_temple_brush.h"
#include "brushes/flag/flag_brush.h"
#include "brushes/border/optional_border_brush.h"
#include "rendering/indicators/technical_item_registry.h"
#include "rendering/indicators/zone_flags.h"
#include "item_definitions/core/item_definition_types.h"

namespace {

uint32_t getZoneFlagForBrush(const FlagBrush* flag_brush) noexcept {
	if (!flag_brush) {
		return 0u;
	}
	switch (flag_brush->getFlag()) {
		case TILESTATE_PROTECTIONZONE: return rme::rendering::ZONE_FLAG_PZ;
		case TILESTATE_NOPVP:          return rme::rendering::ZONE_FLAG_NOPVP;
		case TILESTATE_NOLOGOUT:       return rme::rendering::ZONE_FLAG_NOLOGOUT;
		case TILESTATE_PVPZONE:        return rme::rendering::ZONE_FLAG_PVPZONE;
		case TILESTATE_REFRESH:        return rme::rendering::ZONE_FLAG_REFRESH;
		default:                       return 0u;
	}
}

} // namespace

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

BrushOverlayDrawer::BrushOverlayDrawer() = default;

BrushOverlayDrawer::~BrushOverlayDrawer() = default;

void BrushOverlayDrawer::draw(
	SpriteBatch& sprite_batch,
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

	int cursor_blend_mode = 0;
	if (brushColorType == COLOR_BRUSH) {
		cursor_blend_mode = cfg.getInteger(Config::CURSOR_BLEND_MODE);
	} else if (brushColorType == COLOR_HOUSE_BRUSH || brushColorType == COLOR_FLAG_BRUSH) {
		cursor_blend_mode = cfg.getInteger(Config::CURSOR_ALT_BLEND_MODE);
	}

	auto drawBrushRect = [&](float rx, float ry, float rw, float rh, const glm::vec4& col) {
		if (cursor_blend_mode == 1) {
			glm::vec4 mult_col(glm::mix(glm::vec3(1.0f), glm::vec3(col.r, col.g, col.b), col.a), 1.0f);
			sprite_batch.drawRect(rx, ry, rw, rh, mult_col, atlas);
		} else {
			sprite_batch.drawRect(rx, ry, rw, rh, col, atlas);
		}
	};

	const bool show_outline = cfg.getBoolean(Config::CURSOR_SHOW_OUTLINE);

	auto drawTileEdges = [&](float rx, float ry, bool north, bool south, bool west, bool east) {
		if (!show_outline) {
			return;
		}
		const AtlasRegion* white_pixel = atlas.getWhitePixel();
		if (!white_pixel) {
			return;
		}

		constexpr float S = static_cast<float>(TILE_SIZE);
		const glm::vec4 col(1.0f, 1.0f, 1.0f, 0.7f);

		if (north) {
			sprite_batch.draw(rx, ry, S, 1.0f, *white_pixel, col.r, col.g, col.b, col.a);
		}
		if (south) {
			sprite_batch.draw(rx, ry + S - 1.0f, S, 1.0f, *white_pixel, col.r, col.g, col.b, col.a);
		}
		if (west) {
			sprite_batch.draw(rx, ry, 1.0f, S, *white_pixel, col.r, col.g, col.b, col.a);
		}
		if (east) {
			sprite_batch.draw(rx + S - 1.0f, ry, 1.0f, S, *white_pixel, col.r, col.g, col.b, col.a);
		}
	};

	auto drawTileOutline = [&](float rx, float ry) {
		drawTileEdges(rx, ry, true, true, true, true);
	};

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

			if (cursor_blend_mode == 1) {
				sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
			}

			// Top
			drawBrushRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy), static_cast<float>(last_click_end_sx - last_click_start_sx), static_cast<float>(TILE_SIZE), brushColor);

			// Bottom
			if (delta_y > TILE_SIZE) {
				drawBrushRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_end_sy - TILE_SIZE), static_cast<float>(last_click_end_sx - last_click_start_sx), static_cast<float>(TILE_SIZE), brushColor);
			}

			// Right
			if (delta_x > TILE_SIZE && delta_y > TILE_SIZE) {
				float h = (last_click_end_sy - TILE_SIZE) - (last_click_start_sy + TILE_SIZE);
				drawBrushRect(static_cast<float>(last_click_end_sx - TILE_SIZE), static_cast<float>(last_click_start_sy + TILE_SIZE), static_cast<float>(TILE_SIZE), h, brushColor);
			}

			// Left
			if (delta_y > TILE_SIZE) {
				float h = (last_click_end_sy - TILE_SIZE) - (last_click_start_sy + TILE_SIZE);
				drawBrushRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy + TILE_SIZE), static_cast<float>(TILE_SIZE), h, brushColor);
			}

			if (cursor_blend_mode == 1) {
				sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			}
		} else {
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
								const Position target_pos(x, y, view.floor);
								const Tile* target_tile = editor.map.getTile(target_pos);
								item_drawer->DrawRawBrush(sprite_batch, sprite_drawer, cx, cy, raw_brush->getItemID(), 240, 245, 255, 180, &ctx, target_tile, target_pos);
							}
						}
					}
				} else if (brush->is<FlagBrush>()) {
					const uint32_t zf = getZoneFlagForBrush(brush->as<FlagBrush>());
					const AtlasRegion* white_pixel = atlas.getWhitePixel();
					if (white_pixel && zf != 0u) {
						int start_x = std::min(drag_state.last_click_map_x, view.mouse_map_x);
						int end_x = std::max(drag_state.last_click_map_x, view.mouse_map_x);
						int start_y = std::min(drag_state.last_click_map_y, view.mouse_map_y);
						int end_y = std::max(drag_state.last_click_map_y, view.mouse_map_y);

						if (cursor_blend_mode == 1) {
							sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
						}
						const uint32_t zf_mult = (cursor_blend_mode == 1) ? static_cast<uint32_t>(rme::rendering::ZONE_FLAG_MULTIPLICATIVE) : 0u;

						for (int y = start_y; y <= end_y; ++y) {
							int cy = y * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
							for (int x = start_x; x <= end_x; ++x) {
								int cx = x * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
								uint32_t border_flags = 0u;
								if (y == start_y) border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_N;
								if (y == end_y)   border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_S;
								if (x == start_x) border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_W;
								if (x == end_x)   border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_E;

								sprite_batch.draw(static_cast<float>(cx), static_cast<float>(cy), 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 0.85f, 0.0f, zf | border_flags | zf_mult);
							}
						}

						if (cursor_blend_mode == 1) {
							sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
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

					if (cursor_blend_mode == 1) {
						sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
					}

					if (autoborder_active) {
						// Draw outline only
						float thickness = 1.0f; // Thin border

						// Top
						drawBrushRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy), w, thickness, brushColor);
						// Bottom
						drawBrushRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy + h - thickness), w, thickness, brushColor);
						// Left
						drawBrushRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy + thickness), thickness, h - 2 * thickness, brushColor);
						// Right
						drawBrushRect(static_cast<float>(last_click_start_sx + w - thickness), static_cast<float>(last_click_start_sy + thickness), thickness, h - 2 * thickness, brushColor);
					} else {
						drawBrushRect(static_cast<float>(last_click_start_sx), static_cast<float>(last_click_start_sy), w, h, brushColor);
					}

					if (cursor_blend_mode == 1) {
						sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
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

				if (cursor_blend_mode == 1) {
					sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
				}
				const uint32_t zf_mult = (cursor_blend_mode == 1) ? static_cast<uint32_t>(rme::rendering::ZONE_FLAG_MULTIPLICATIVE) : 0u;

				for (int y = start_y - 1; y <= end_y + 1; y++) {
					int cy = y * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
					float dy = center_y - y;
					for (int x = start_x - 1; x <= end_x + 1; x++) {
						int cx = x * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();

						float dx = center_x - x;
						float distance = sqrt(dx * dx + dy * dy);
						if (distance < radii) {
							if (brush->is<RAWBrush>()) {
								const Position target_pos(x, y, view.floor);
								const Tile* target_tile = editor.map.getTile(target_pos);
								item_drawer->DrawRawBrush(sprite_batch, sprite_drawer, cx, cy, raw_brush->getItemID(), 160, 160, 160, 160, &ctx, target_tile, target_pos);
							} else if (brush->is<FlagBrush>()) {
								const uint32_t zf = getZoneFlagForBrush(brush->as<FlagBrush>());
								const AtlasRegion* white_pixel = atlas.getWhitePixel();
								if (white_pixel && zf != 0u) {
									auto inCircle = [&](int nx, int ny) {
										float cdx = static_cast<float>(center_x - nx);
										float cdy = static_cast<float>(center_y - ny);
										return sqrt(cdx * cdx + cdy * cdy) < radii;
									};
									uint32_t border_flags = 0u;
									if (!inCircle(x, y - 1)) border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_N;
									if (!inCircle(x, y + 1)) border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_S;
									if (!inCircle(x - 1, y)) border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_W;
									if (!inCircle(x + 1, y)) border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_E;

									sprite_batch.draw(static_cast<float>(cx), static_cast<float>(cy), 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 0.85f, 0.0f, zf | border_flags | zf_mult);
								}
							} else {
								drawBrushRect(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE), brushColor);
							}
						}
					}
				}

				if (cursor_blend_mode == 1) {
					sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
				}
			}
		}
	} else {
		const BrushFootprint footprint = bm.GetBrushFootprint();
		if (brush->is<DoorBrush>()) {
			int cx = (view.mouse_map_x) * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
			int cy = (view.mouse_map_y) * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
			DoorBrush* door_brush = brush->as<DoorBrush>();
			Position pos(view.mouse_map_x, view.mouse_map_y, view.floor);
			bool valid = door_brush->canDraw(&editor.map, pos);
			uint16_t door_id = valid ? door_brush->getDoorItemId(&editor.map, pos) : door_brush->getFallbackDoorItemId();
			if (door_id != 0) {
				const Tile* target_tile = editor.map.getTile(pos);
				if (valid) {
					item_drawer->DrawRawBrush(sprite_batch, sprite_drawer, cx, cy, door_id, 255, 255, 255, 160, &ctx, target_tile, pos);
				} else {
					item_drawer->DrawRawBrush(sprite_batch, sprite_drawer, cx, cy, door_id, 255, 64, 64, 160, &ctx, target_tile, pos);
				}
			}
			drawTileOutline(static_cast<float>(cx), static_cast<float>(cy));
		} else if (brush->is<CreatureBrush>()) {
			int cy = (view.mouse_map_y) * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
			int cx = (view.mouse_map_x) * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();
			CreatureBrush* creature_brush = brush->as<CreatureBrush>();
			const Direction creature_direction = creature_brush->getDirection();
			if (creature_brush->canDraw(&editor.map, Position(view.mouse_map_x, view.mouse_map_y, view.floor))) {
				creature_drawer->BlitCreature(sprite_batch, sprite_drawer, cx, cy, creature_brush->getType()->outfit, creature_direction, CreatureDrawOptions { .color = DrawColor(255, 255, 255, 160), .ctx = &ctx });
			} else {
				creature_drawer->BlitCreature(sprite_batch, sprite_drawer, cx, cy, creature_brush->getType()->outfit, creature_direction, CreatureDrawOptions { .color = DrawColor(255, 64, 64, 160), .ctx = &ctx });
			}
			drawTileOutline(static_cast<float>(cx), static_cast<float>(cy));
		} else {
			RAWBrush* raw_brush = brush->is<RAWBrush>() ? brush->as<RAWBrush>() : nullptr;
			WallBrush* wall_brush = brush->is<WallBrush>() ? brush->as<WallBrush>() : nullptr;
			GroundBrush* ground_brush = brush->is<GroundBrush>() ? brush->as<GroundBrush>() : nullptr;
			CarpetBrush* carpet_brush = brush->is<CarpetBrush>() ? brush->as<CarpetBrush>() : nullptr;
			TableBrush* table_brush = brush->is<TableBrush>() ? brush->as<TableBrush>() : nullptr;
			DoodadBrush* doodad_brush = brush->is<DoodadBrush>() ? brush->as<DoodadBrush>() : nullptr;

			uint16_t placement_item_id = 0;
			if (raw_brush) {
				placement_item_id = raw_brush->getItemID();
			} else if (wall_brush) {
				placement_item_id = wall_brush->getPlacementItemId();
			} else if (ground_brush) {
				placement_item_id = ground_brush->getPlacementItemId();
			} else if (carpet_brush) {
				placement_item_id = carpet_brush->getPlacementItemId();
			} else if (table_brush) {
				placement_item_id = table_brush->getPlacementItemId();
			} else if (doodad_brush && g_doodad_preview.GetBufferMap()->size() == 0) {
				placement_item_id = doodad_brush->getPlacementItemId(g_brush_manager.GetBrushVariation());
				if (placement_item_id == 0 && doodad_brush->getLookID() > 0) {
					if (auto sid = ctx.item_definitions.findByClientId(static_cast<ClientItemId>(doodad_brush->getLookID()))) {
						placement_item_id = *sid;
					}
				}
			}

			BaseMap* buffer_map = (doodad_brush && placement_item_id == 0) ? g_doodad_preview.GetBufferMap() : nullptr;
			const bool is_item_preview = (buffer_map != nullptr || placement_item_id != 0);

			if (cursor_blend_mode == 1 && !is_item_preview) {
				sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
			}
			const uint32_t zf_mult = (cursor_blend_mode == 1) ? static_cast<uint32_t>(rme::rendering::ZONE_FLAG_MULTIPLICATIVE) : 0u;
			auto hasItemAt = [&](int ox, int oy) -> bool {
				if (buffer_map) {
					Tile* t = buffer_map->getTile(0x8000 + ox, 0x8000 + oy, 0x8);
					return t != nullptr && !t->empty();
				}
				if (placement_item_id != 0) {
					return footprint.containsOffset(ox, oy);
				}
				return false;
			};

			const int min_y = buffer_map ? (footprint.min_offset_y - 2) : (footprint.min_offset_y - 1);
			const int max_y = buffer_map ? (footprint.max_offset_y + 2) : (footprint.max_offset_y + 1);
			const int min_x = buffer_map ? (footprint.min_offset_x - 2) : (footprint.min_offset_x - 1);
			const int max_x = buffer_map ? (footprint.max_offset_x + 2) : (footprint.max_offset_x + 1);

			for (int y = min_y; y <= max_y; y++) {
				int cy = (view.mouse_map_y + y) * TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
				for (int x = min_x; x <= max_x; x++) {
					int cx = (view.mouse_map_x + x) * TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();

					if (buffer_map || placement_item_id != 0) {
						if (hasItemAt(x, y)) {
							if (placement_item_id != 0) {
								const Position target_pos(view.mouse_map_x + x, view.mouse_map_y + y, view.floor);
								const Tile* target_tile = editor.map.getTile(target_pos);
								uint16_t item_to_draw = placement_item_id;
								if (wall_brush && wall_brush->is<WallDecorationBrush>()) {
									if (uint16_t dynamic_id = wall_brush->getPlacementItemId(target_tile)) {
										item_to_draw = dynamic_id;
									}
								}
								item_drawer->DrawRawBrush(sprite_batch, sprite_drawer, cx, cy, item_to_draw, 255, 255, 255, 160, &ctx, target_tile, target_pos);
							}
							bool north = !hasItemAt(x, y - 1);
							bool south = !hasItemAt(x, y + 1);
							bool west  = !hasItemAt(x - 1, y);
							bool east  = !hasItemAt(x + 1, y);
							drawTileEdges(static_cast<float>(cx), static_cast<float>(cy), north, south, west, east);
						}
						continue;
					}

					// The loop scans one extra ring around the footprint so ground
					// brushes can preview their borders; other previews must stay
					// inside the tiles the brush actually paints. The spawn preview
					// is left as is: it shows the spawn radius, max(1, size), which
					// is not the brush footprint.
					if (!brush->is<SpawnBrush>()) {
						const bool is_single_tile_brush = brush->is<WaypointBrush>() || brush->is<HouseExitBrush>() || brush->is<TownTempleBrush>();
						if (is_single_tile_brush ? (x != 0 || y != 0) : !footprint.containsOffset(x, y)) {
							continue;
						}
					}

					if (brush->is<SpawnBrush>()) {
						const AtlasRegion* white_pixel = atlas.getWhitePixel();
						if (white_pixel) {
							if (x == 0 && y == 0) {
								sprite_batch.draw(static_cast<float>(cx), static_cast<float>(cy), 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 0.9f, 0.0f, rme::rendering::MakeIndicatorZoneFlag(static_cast<uint8_t>(rme::rendering::TileIndicatorType::Spawn)));
							} else {
								uint32_t border_flags = 0u;
								if (y == footprint.min_offset_y) border_flags |= rme::rendering::ZONE_FLAG_SPAWN_BORDER_N;
								if (y == footprint.max_offset_y) border_flags |= rme::rendering::ZONE_FLAG_SPAWN_BORDER_S;
								if (x == footprint.min_offset_x) border_flags |= rme::rendering::ZONE_FLAG_SPAWN_BORDER_W;
								if (x == footprint.max_offset_x) border_flags |= rme::rendering::ZONE_FLAG_SPAWN_BORDER_E;
								sprite_batch.draw(static_cast<float>(cx), static_cast<float>(cy), 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 0.85f, 0.0f, rme::rendering::ZONE_FLAG_SPAWN | border_flags);
							}
						}
					} else if (brush->is<WaypointBrush>()) {
						const AtlasRegion* white_pixel = atlas.getWhitePixel();
						if (white_pixel) {
							sprite_batch.draw(static_cast<float>(cx), static_cast<float>(cy), 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 0.9f, 0.0f, rme::rendering::MakeIndicatorZoneFlag(static_cast<uint8_t>(rme::rendering::TileIndicatorType::Waypoint)));
						}
					} else if (brush->is<HouseExitBrush>()) {
						const AtlasRegion* white_pixel = atlas.getWhitePixel();
						if (white_pixel) {
							sprite_batch.draw(static_cast<float>(cx), static_cast<float>(cy), 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 0.9f, 0.0f, rme::rendering::MakeIndicatorZoneFlag(static_cast<uint8_t>(rme::rendering::TileIndicatorType::HouseEntry)));
						}
					} else if (brush->is<TownTempleBrush>()) {
						const AtlasRegion* white_pixel = atlas.getWhitePixel();
						if (white_pixel) {
							sprite_batch.draw(static_cast<float>(cx), static_cast<float>(cy), 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 0.9f, 0.0f, rme::rendering::MakeIndicatorZoneFlag(static_cast<uint8_t>(rme::rendering::TileIndicatorType::TownTemple)));
						}
					} else if (brush->is<FlagBrush>()) {
						const AtlasRegion* white_pixel = atlas.getWhitePixel();
						if (white_pixel) {
							const uint32_t zf = getZoneFlagForBrush(brush->as<FlagBrush>());
							if (zf != 0u) {
								uint32_t border_flags = 0u;
								if (!footprint.containsOffset(x, y - 1)) border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_N;
								if (!footprint.containsOffset(x, y + 1)) border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_S;
								if (!footprint.containsOffset(x - 1, y)) border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_W;
								if (!footprint.containsOffset(x + 1, y)) border_flags |= rme::rendering::ZONE_FLAG_ZONE_BORDER_E;

								sprite_batch.draw(static_cast<float>(cx), static_cast<float>(cy), 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 0.85f, 0.0f, zf | border_flags | zf_mult);
							}
						}
					} else {
						glm::vec4 c = brushColor;
						if (brush->is<OptionalBorderBrush>()) {
							c = get_check_color(brush, editor, Position(view.mouse_map_x + x, view.mouse_map_y + y, view.floor), cfg);
						}
						drawBrushRect(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE), c);
					}
				}
			}

			if (cursor_blend_mode == 1 && !is_item_preview) {
				sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			}
		}
	}
}
