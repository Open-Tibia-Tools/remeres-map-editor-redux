//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "rendering/ui/drawing_controller.h"
#include "rendering/ui/map_display.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "map/map.h"
#include "map/tile.h"
#include "brushes/brush.h"
#include "ui/gui.h"
#include "brushes/brush_utility.h"
#include "app/settings.h"

// Brushes
#include "brushes/doodad/doodad_brush.h"

#include "brushes/carpet/carpet_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/zone/zone_brush.h"
#include "brushes/town/town_temple_brush.h"

DrawingController::DrawingController(MapCanvas* canvas, Editor& editor) :
	canvas(canvas),
	editor(editor),
	drawing(false),
	dragging_draw(false),
	replace_dragging(false),
	last_draw_pos(Position(-1, -1, -1)) {
}

DrawingController::~DrawingController() {
}

void DrawingController::HandleClick(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down) {
	Brush* brush = g_gui.GetCurrentBrush();
	if (brush) {
		const BrushFootprint footprint = g_gui.GetBrushFootprint();
		if (shift_down && brush->canDrag()) {
			dragging_draw = true;
		} else {
			if (footprint.isSingleTile() && !brush->oneSizeFitsAll()) {
				drawing = true;
			} else {
				drawing = g_gui.GetCurrentBrush()->canSmear();
			}
			if (brush->is<WallBrush>()) {
				if (alt_down && footprint.isSingleTile()) {
					// z0mg, just clicked a tile, shift variaton.
					if (ctrl_down) {
						editor.undraw(mouse_map_pos, alt_down);
					} else {
						editor.draw(mouse_map_pos, alt_down);
					}
					last_draw_pos = mouse_map_pos;
				} else {
					PositionVector tilestodraw;
					PositionVector tilestoborder;

					const int start_map_x = mouse_map_pos.x + footprint.min_offset_x;
					const int start_map_y = mouse_map_pos.y + footprint.min_offset_y;
					const int end_map_x = mouse_map_pos.x + footprint.max_offset_x;
					const int end_map_y = mouse_map_pos.y + footprint.max_offset_y;

					for (int y = start_map_y - 1; y <= end_map_y + 1; ++y) {
						for (int x = start_map_x - 1; x <= end_map_x + 1; ++x) {
							if ((x <= start_map_x + 1 || x >= end_map_x - 1) || (y <= start_map_y + 1 || y >= end_map_y - 1)) {
								tilestoborder.push_back(Position(x, y, mouse_map_pos.z));
							}
							if (((x == start_map_x || x == end_map_x) || (y == start_map_y || y == end_map_y)) && ((x >= start_map_x && x <= end_map_x) && (y >= start_map_y && y <= end_map_y))) {
								tilestodraw.push_back(Position(x, y, mouse_map_pos.z));
							}
						}
					}
					if (ctrl_down) {
						editor.undraw(tilestodraw, tilestoborder, alt_down);
					} else {
						editor.draw(tilestodraw, tilestoborder, alt_down);
					}
				}
			} else if (brush->is<DoorBrush>()) {
				PositionVector tilestodraw;
				PositionVector tilestoborder;

				tilestodraw.push_back(mouse_map_pos);

				tilestoborder.push_back(Position(mouse_map_pos.x, mouse_map_pos.y - 1, mouse_map_pos.z));
				tilestoborder.push_back(Position(mouse_map_pos.x - 1, mouse_map_pos.y, mouse_map_pos.z));
				tilestoborder.push_back(Position(mouse_map_pos.x, mouse_map_pos.y + 1, mouse_map_pos.z));
				tilestoborder.push_back(Position(mouse_map_pos.x + 1, mouse_map_pos.y, mouse_map_pos.z));

				if (ctrl_down) {
					editor.undraw(tilestodraw, tilestoborder, alt_down);
				} else {
					editor.draw(tilestodraw, tilestoborder, alt_down);
				}
			} else if (brush->is<DoodadBrush>() || brush->is<SpawnBrush>() || brush->is<CreatureBrush>()) {
				if (ctrl_down) {
					if (brush->is<DoodadBrush>()) {
						PositionVector tilestodraw;
						BrushUtility::GetTilesToDraw(mouse_map_pos.x, mouse_map_pos.y, mouse_map_pos.z, &tilestodraw, nullptr);
						editor.undraw(tilestodraw, alt_down);
					} else {
						editor.undraw(mouse_map_pos, shift_down || alt_down);
					}
				} else {
					bool will_show_spawn = false;
					if (brush->is<SpawnBrush>()) {
						if (!g_settings.getBoolean(Config::SHOW_SPAWNS)) {
							Tile* tile = editor.map.getTile(mouse_map_pos);
							if (!tile || !tile->spawn) {
								will_show_spawn = true;
							}
						}
					}

					editor.draw(mouse_map_pos, shift_down || alt_down);

					if (will_show_spawn) {
						Tile* tile = editor.map.getTile(mouse_map_pos);
						if (tile && tile->spawn) {
							g_settings.setInteger(Config::SHOW_SPAWNS, true);
							g_gui.UpdateMenubar();
						}
					}
				}
			} else {
				if (brush->is<GroundBrush>() && alt_down) {
					replace_dragging = true;
					Tile* draw_tile = editor.map.getTile(mouse_map_pos);
					if (draw_tile) {
						editor.replace_brush = draw_tile->getGroundBrush();
					} else {
						editor.replace_brush = nullptr;
					}
				}
				last_draw_pos = mouse_map_pos;

				if (brush->needBorders()) {
					PositionVector tilestodraw;
					PositionVector tilestoborder;

					bool fill = canvas->keyCode == WXK_CONTROL_D && ctrl_down && brush->is<GroundBrush>();
					BrushUtility::GetTilesToDraw(mouse_map_pos.x, mouse_map_pos.y, mouse_map_pos.z, &tilestodraw, &tilestoborder, fill);

					if (!fill && ctrl_down) {
						editor.undraw(tilestodraw, tilestoborder, alt_down);
					} else {
						editor.draw(tilestodraw, tilestoborder, alt_down);
					}
				} else if (brush->oneSizeFitsAll()) {
					if (brush->is<HouseExitBrush>() || brush->is<WaypointBrush>() || brush->is<TownTempleBrush>()) {
						editor.draw(mouse_map_pos, alt_down);
					} else {
						PositionVector tilestodraw;
						tilestodraw.push_back(mouse_map_pos);
						if (ctrl_down) {
							editor.undraw(tilestodraw, alt_down);
						} else {
							editor.draw(tilestodraw, alt_down);
						}
					}
				} else {
					PositionVector tilestodraw;

					BrushUtility::GetTilesToDraw(mouse_map_pos.x, mouse_map_pos.y, mouse_map_pos.z, &tilestodraw, nullptr);

					if (ctrl_down) {
						editor.undraw(tilestodraw, alt_down);
					} else {
						editor.draw(tilestodraw, alt_down);
					}
				}
			}
			// Change the doodad layout brush
			g_gui.FillDoodadPreviewBuffer();
		}
	}
}

void DrawingController::HandleDrag(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down) {
	if (mouse_map_pos == last_draw_pos) {
		return;
	}
	last_draw_pos = mouse_map_pos;

	Brush* brush = g_gui.GetCurrentBrush();
	if (drawing && brush) {
		if (brush->is<DoodadBrush>()) {
			if (ctrl_down) {
				PositionVector tilestodraw;
				BrushUtility::GetTilesToDraw(mouse_map_pos.x, mouse_map_pos.y, mouse_map_pos.z, &tilestodraw, nullptr);
				editor.undraw(tilestodraw, shift_down || alt_down);
			} else {
				editor.draw(mouse_map_pos, shift_down || alt_down);
			}
		} else if (brush->is<DoorBrush>()) {
			if (!brush->canDraw(&editor.map, mouse_map_pos)) {
				// We don't have to waste an action in this case...
			} else {
				PositionVector tilestodraw;
				PositionVector tilestoborder;

				tilestodraw.push_back(mouse_map_pos);

				tilestoborder.push_back(Position(mouse_map_pos.x, mouse_map_pos.y - 1, mouse_map_pos.z));
				tilestoborder.push_back(Position(mouse_map_pos.x - 1, mouse_map_pos.y, mouse_map_pos.z));
				tilestoborder.push_back(Position(mouse_map_pos.x, mouse_map_pos.y + 1, mouse_map_pos.z));
				tilestoborder.push_back(Position(mouse_map_pos.x + 1, mouse_map_pos.y, mouse_map_pos.z));

				if (ctrl_down) {
					editor.undraw(tilestodraw, tilestoborder, alt_down);
				} else {
					editor.draw(tilestodraw, tilestoborder, alt_down);
				}
			}
		} else if (brush->needBorders()) {
			PositionVector tilestodraw, tilestoborder;

			BrushUtility::GetTilesToDraw(mouse_map_pos.x, mouse_map_pos.y, mouse_map_pos.z, &tilestodraw, &tilestoborder);

			if (ctrl_down) {
				editor.undraw(tilestodraw, tilestoborder, alt_down);
			} else {
				editor.draw(tilestodraw, tilestoborder, alt_down);
			}
		} else if (brush->oneSizeFitsAll()) {
			drawing = true;
			PositionVector tilestodraw;
			tilestodraw.push_back(mouse_map_pos);

			if (ctrl_down) {
				editor.undraw(tilestodraw, alt_down);
			} else {
				editor.draw(tilestodraw, alt_down);
			}
		} else { // No borders
			PositionVector tilestodraw;
			BrushUtility::GetTilesToDraw(mouse_map_pos.x, mouse_map_pos.y, mouse_map_pos.z, &tilestodraw, nullptr);
			if (ctrl_down) {
				editor.undraw(tilestodraw, alt_down);
			} else {
				editor.draw(tilestodraw, alt_down);
			}
		}

		// Create newd doodad layout (does nothing if a non-doodad brush is selected)
		g_gui.FillDoodadPreviewBuffer();

		canvas->RequestLocalRefresh();
	} else if (dragging_draw) {
		canvas->RequestLocalRefresh();
	}
	// Removed map_update check as this function is usually called when coords changed
	if (!drawing && !dragging_draw && brush) {
		// Just refresh the view basically, handled by MapCanvas usually, but consistent to return status
	}
}

void DrawingController::HandleRelease(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down) {
	if (dragging_draw) {
		Brush* brush = g_gui.GetCurrentBrush();
		if (brush) {
			if (brush->is<SpawnBrush>()) {
				int start_map_x = std::min(canvas->last_click_map_x, mouse_map_pos.x);
				int start_map_y = std::min(canvas->last_click_map_y, mouse_map_pos.y);
				int end_map_x = std::max(canvas->last_click_map_x, mouse_map_pos.x);
				int end_map_y = std::max(canvas->last_click_map_y, mouse_map_pos.y);

				int map_x = start_map_x + (end_map_x - start_map_x) / 2;
				int map_y = start_map_y + (end_map_y - start_map_y) / 2;

				int width = std::min(g_settings.getInteger(Config::MAX_SPAWN_RADIUS), ((end_map_x - start_map_x) / 2 + (end_map_y - start_map_y) / 2) / 2);
				const BrushSizeState previous_size_state = g_gui.GetBrushSizeState();
				g_gui.SetBrushSize(width);
				editor.draw(Position(map_x, map_y, mouse_map_pos.z), alt_down);
				g_gui.RestoreBrushSizeState(previous_size_state);
			} else {
				PositionVector tilestodraw;
				PositionVector tilestoborder;
				if (brush->is<WallBrush>()) {
					int start_map_x = std::min(canvas->last_click_map_x, mouse_map_pos.x);
					int start_map_y = std::min(canvas->last_click_map_y, mouse_map_pos.y);
					int end_map_x = std::max(canvas->last_click_map_x, mouse_map_pos.x);
					int end_map_y = std::max(canvas->last_click_map_y, mouse_map_pos.y);

					for (int y = start_map_y - 1; y <= end_map_y + 1; y++) {
						for (int x = start_map_x - 1; x <= end_map_x + 1; x++) {
							if ((x <= start_map_x + 1 || x >= end_map_x - 1) || (y <= start_map_y + 1 || y >= end_map_y - 1)) {
								tilestoborder.push_back(Position(x, y, mouse_map_pos.z));
							}
							if (((x == start_map_x || x == end_map_x) || (y == start_map_y || y == end_map_y)) && ((x >= start_map_x && x <= end_map_x) && (y >= start_map_y && y <= end_map_y))) {
								tilestodraw.push_back(Position(x, y, mouse_map_pos.z));
							}
						}
					}
				} else {
					if (g_gui.GetBrushShape() == BRUSHSHAPE_SQUARE) {
						int last_x = canvas->last_click_map_x;
						int last_y = canvas->last_click_map_y;
						int curr_x = mouse_map_pos.x;
						int curr_y = mouse_map_pos.y;

						if (last_x > curr_x) {
							std::swap(curr_x, last_x);
						}
						if (last_y > curr_y) {
							std::swap(curr_y, last_y);
						}

						for (int x = last_x - 1; x <= curr_x + 1; x++) {
							for (int y = last_y - 1; y <= curr_y + 1; y++) {
								if ((x <= last_x || x >= curr_x) || (y <= last_y || y >= curr_y)) {
									tilestoborder.push_back(Position(x, y, mouse_map_pos.z));
								}
								if ((x >= last_x && x <= curr_x) && (y >= last_y && y <= curr_y)) {
									tilestodraw.push_back(Position(x, y, mouse_map_pos.z));
								}
							}
						}
					} else {
						int start_x, end_x;
						int start_y, end_y;
						int width = std::max(
							std::abs(
								std::max(mouse_map_pos.y, canvas->last_click_map_y) - std::min(mouse_map_pos.y, canvas->last_click_map_y)
							),
							std::abs(
								std::max(mouse_map_pos.x, canvas->last_click_map_x) - std::min(mouse_map_pos.x, canvas->last_click_map_x)
							)
						);
						if (mouse_map_pos.x < canvas->last_click_map_x) {
							start_x = canvas->last_click_map_x - width;
							end_x = canvas->last_click_map_x;
						} else {
							start_x = canvas->last_click_map_x;
							end_x = canvas->last_click_map_x + width;
						}
						if (mouse_map_pos.y < canvas->last_click_map_y) {
							start_y = canvas->last_click_map_y - width;
							end_y = canvas->last_click_map_y;
						} else {
							start_y = canvas->last_click_map_y;
							end_y = canvas->last_click_map_y + width;
						}

						int center_x = start_x + (end_x - start_x) / 2;
						int center_y = start_y + (end_y - start_y) / 2;
						float radii = width / 2.0f + 0.005f;

						for (int y = start_y - 1; y <= end_y + 1; y++) {
							float dy = center_y - y;
							for (int x = start_x - 1; x <= end_x + 1; x++) {
								float dx = center_x - x;
								float distance = sqrt(dx * dx + dy * dy);
								if (distance < radii) {
									tilestodraw.push_back(Position(x, y, mouse_map_pos.z));
								}
								if (std::abs(distance - radii) < 1.5) {
									tilestoborder.push_back(Position(x, y, mouse_map_pos.z));
								}
							}
						}
					}
				}
				if (ctrl_down) {
					editor.undraw(tilestodraw, tilestoborder, alt_down);
				} else {
					editor.draw(tilestodraw, tilestoborder, alt_down);
				}
			}
		}
	}

	Brush* brush = g_gui.GetCurrentBrush();
	if (brush && brush->is<ZoneBrush>()) {
		g_gui.UpdateZonePalette(static_cast<ZoneBrush*>(brush)->getZone());
	} else if (brush && (brush->is<HouseBrush>() || brush->is<HouseExitBrush>())) {
		uint32_t house_id = 0;
		if (auto* hb = dynamic_cast<HouseBrush*>(brush)) {
			house_id = hb->getHouseID();
		} else if (auto* heb = dynamic_cast<HouseExitBrush*>(brush)) {
			house_id = heb->getHouseID();
		}
		g_gui.UpdateHousePalette(house_id);
	} else if (brush && brush->is<TownTempleBrush>()) {
		g_gui.RefreshPalettes();
	}

	editor.actionQueue->resetTimer();
	drawing = false;
	dragging_draw = false;
	replace_dragging = false;
	editor.replace_brush = nullptr;
	last_draw_pos = Position(-1, -1, -1); // Reset last_draw_pos here as well, or call Reset()
}

void DrawingController::HandleWheel(int rotation, bool alt_down, bool ctrl_down) {
	if (alt_down) {
		static double diff = 0.0;
		diff += rotation;
		if (diff <= 1.0 || diff >= 1.0) {
			if (diff < 0.0) {
				g_gui.IncreaseBrushSize();
			} else {
				g_gui.DecreaseBrushSize();
			}
			diff = 0.0;
		}
	}
}
