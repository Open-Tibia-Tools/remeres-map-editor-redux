//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "rendering/ui/selection_controller.h"
#include "editor/selection_thread.h"
#include "rendering/ui/map_display.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/spawn.h"
#include "game/creature.h"
#include "app/settings.h"
#include "ui/gui.h"
#include "rendering/ui/brush_selector.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/raw/raw_brush.h"
#include "ui/dialog_helper.h"
#include "rendering/core/floor_visibility_mode.h"
#include "rendering/core/map_view_math.h"

SelectionController::SelectionController(MapCanvas* canvas, Editor& editor) :
	canvas(canvas),
	editor(editor),
	dragging(false),
	boundbox_selection(false),
	drag_start_pos(Position()) {
}

SelectionController::~SelectionController() {
}

void SelectionController::HandleClick(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, bool shift_down, bool ctrl_down, bool alt_down) {
	last_click_pixel = pixel_offset;
	const bool pixel_mode = SelectionPicker::ActiveMethod() == SelectionMethod::PixelPerfect;

	if (ctrl_down && alt_down) {
		Tile* tile = editor.map.getTile(mouse_map_pos);
		if (tile && tile->size() > 0) {
			// Select visible creature
			if (tile->creature && g_settings.getInteger(Config::SHOW_CREATURES)) {
				CreatureBrush* brush = tile->creature->getBrush();
				if (brush) {
					g_gui.SelectBrush(brush);
					return;
				}
			}
			// Fall back to item selection
			Item* item = tile->getTopItem();
			if (item && item->getRAWBrush()) {
				g_gui.SelectBrush(item->getRAWBrush());
			}
		}
	} else if (g_gui.IsSelectionMode()) {
		if (canvas->isPasting()) {
			// Set paste to false (no rendering etc.)
			canvas->EndPasting();

			// Paste to the map
			editor.copybuffer.paste(editor, mouse_map_pos);

			// Start dragging
			dragging = true;
			drag_start_pos = mouse_map_pos;
		} else {
			boundbox_selection = false;
			if (shift_down) {
				boundbox_selection = true;

				if (!ctrl_down) {
					editor.selection.start(); // Start selection session
					editor.selection.clear(); // Clear out selection
					editor.selection.finish(); // End selection session
					editor.selection.updateSelectionCount();
				}
			} else if (ctrl_down) {
				if (pixel_mode) {
					const SelectionHit hit = PickAt(mouse_map_pos, pixel_offset);
					if (hit.HasEntity()) {
						ApplyHit(mouse_map_pos, pixel_offset, hit, true);
					}
				} else {
					Tile* tile = FindSelectedTileUnderCursor(mouse_map_pos);
					if (tile) {
						if (tile->spawn && g_settings.getInteger(Config::SHOW_SPAWNS)) {
							editor.selection.start(); // Start selection session
							if (tile->spawn->isSelected()) {
								editor.selection.remove(tile, tile->spawn.get());
							} else {
								editor.selection.add(tile, tile->spawn.get());
							}
							editor.selection.finish(); // Finish selection session
							editor.selection.updateSelectionCount();
						} else if (tile->creature && g_settings.getInteger(Config::SHOW_CREATURES)) {
							editor.selection.start(); // Start selection session
							if (tile->creature->isSelected()) {
								editor.selection.remove(tile, tile->creature.get());
							} else {
								editor.selection.add(tile, tile->creature.get());
							}
							editor.selection.finish(); // Finish selection session
							editor.selection.updateSelectionCount();
						} else {
							Item* item = tile->getTopItem();
							if (item) {
								editor.selection.start(); // Start selection session
								if (item->isSelected()) {
									editor.selection.remove(tile, item);
								} else {
									editor.selection.add(tile, item);
								}
								editor.selection.finish(); // Finish selection session
								editor.selection.updateSelectionCount();
							}
						}
					}
				}
			} else {
				if (pixel_mode) {
					const SelectionHit hit = PickAt(mouse_map_pos, pixel_offset);
					if (!hit.HasEntity()) {
						if (Tile* lower_tile = FindSelectedTileUnderCursor(mouse_map_pos); lower_tile && lower_tile != editor.map.getTile(mouse_map_pos)) {
							dragging = true;
							drag_start_pos = Position(lower_tile->getX(), lower_tile->getY(), lower_tile->getZ());
							return;
						}
						ClearSelection();
					} else {
						if (!IsHitSelected(hit)) {
							ApplyHit(mouse_map_pos, pixel_offset, hit, false);
						}
						dragging = true;
						drag_start_pos = mouse_map_pos;
					}
				} else {
					Tile* tile = FindSelectedTileUnderCursor(mouse_map_pos);
					if (!tile) {
						editor.selection.start(); // Start selection session
						editor.selection.clear(); // Clear out selection
						editor.selection.finish(); // End selection session
						editor.selection.updateSelectionCount();
					} else if (tile->isSelected()) {
						dragging = true;
						drag_start_pos = Position(tile->getX(), tile->getY(), tile->getZ());
					} else {
						editor.selection.start(); // Start a selection session
						editor.selection.clear();
						editor.selection.commit();
						if (tile->spawn && g_settings.getInteger(Config::SHOW_SPAWNS)) {
							editor.selection.add(tile, tile->spawn.get());
							dragging = true;
							drag_start_pos = mouse_map_pos;
						} else if (tile->creature && g_settings.getInteger(Config::SHOW_CREATURES)) {
							editor.selection.add(tile, tile->creature.get());
							dragging = true;
							drag_start_pos = mouse_map_pos;
						} else {
							Item* item = tile->getTopItem();
							if (item) {
								editor.selection.add(tile, item);
								dragging = true;
								drag_start_pos = mouse_map_pos;
							}
						}
						editor.selection.finish(); // Finish the selection session
						editor.selection.updateSelectionCount();
					}
				}
			}
		}
	}
}

void SelectionController::HandleDrag(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down) {
	if (g_gui.IsSelectionMode()) {
		if (canvas->isPasting()) {
			canvas->RequestLocalRefresh();
		} else if (dragging) {
			wxString ss;

			int move_x = drag_start_pos.x - mouse_map_pos.x;
			int move_y = drag_start_pos.y - mouse_map_pos.y;
			int move_z = drag_start_pos.z - mouse_map_pos.z;
			ss << "Dragging " << -move_x << "," << -move_y << "," << -move_z;
			g_gui.SetStatusText(ss);

			canvas->RequestLocalRefresh();
		} else if (boundbox_selection) {
			// Calculate selection size
			int move_x = std::abs(canvas->last_click_map_x - mouse_map_pos.x);
			int move_y = std::abs(canvas->last_click_map_y - mouse_map_pos.y);
			wxString ss;
			ss << "Selection " << move_x + 1 << ":" << move_y + 1;
			g_gui.SetStatusText(ss);

			canvas->RequestLocalRefresh();
		}
	}
}

void SelectionController::HandleRelease(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down) {
	int move_x = canvas->last_click_map_x - mouse_map_pos.x;
	int move_y = canvas->last_click_map_y - mouse_map_pos.y;
	int move_z = canvas->last_click_map_z - mouse_map_pos.z;
	const bool pixel_mode = SelectionPicker::ActiveMethod() == SelectionMethod::PixelPerfect;

	if (g_gui.IsSelectionMode()) {
		if (dragging && (move_x != 0 || move_y != 0 || move_z != 0)) {
			editor.moveSelection(Position(move_x, move_y, move_z));
		} else {
			if (boundbox_selection) {
				if (mouse_map_pos.x == canvas->last_click_map_x && mouse_map_pos.y == canvas->last_click_map_y && ctrl_down) {
					// Mouse hasn't moved, do control+shift thingy!
					Tile* tile = editor.map.getTile(mouse_map_pos);
					if (tile) {
						editor.selection.start(); // Start a selection session
						if (tile->isSelected()) {
							editor.selection.remove(tile);
						} else {
							editor.selection.add(tile);
						}
						editor.selection.finish(); // Finish the selection session
						editor.selection.updateSelectionCount();
					}
				} else {
					ExecuteBoundboxSelection(Position(canvas->last_click_map_x, canvas->last_click_map_y, canvas->last_click_map_z), mouse_map_pos, mouse_map_pos.z);
				}
			} else if (ctrl_down) {
				////
			} else {
				if (pixel_mode) {
					const SelectionHit hit = PickAt(mouse_map_pos, last_click_pixel);
					if (hit.HasEntity() && !IsHitSelected(hit)) {
						ApplyHit(mouse_map_pos, last_click_pixel, hit, false);
					}
				} else {
					// User hasn't moved anything, meaning selection/deselection
					Tile* tile = FindSelectedTileUnderCursor(mouse_map_pos);
					if (tile) {
						if (tile->spawn && g_settings.getInteger(Config::SHOW_SPAWNS)) {
							if (!tile->spawn->isSelected()) {
								editor.selection.start(); // Start a selection session
								editor.selection.add(tile, tile->spawn.get());
								editor.selection.finish(); // Finish the selection session
								editor.selection.updateSelectionCount();
							}
						} else if (tile->creature && g_settings.getInteger(Config::SHOW_CREATURES)) {
							if (!tile->creature->isSelected()) {
								editor.selection.start(); // Start a selection session
								editor.selection.add(tile, tile->creature.get());
								editor.selection.finish(); // Finish the selection session
								editor.selection.updateSelectionCount();
							}
						} else {
							Item* item = tile->getTopItem();
							if (item && !item->isSelected()) {
								editor.selection.start(); // Start a selection session
								editor.selection.add(tile, item);
								editor.selection.finish(); // Finish the selection session
								editor.selection.updateSelectionCount();
							}
						}
					}
				}
			}
		}
		editor.actionQueue->resetTimer();
		dragging = false;
		boundbox_selection = false;
	}
}

Tile* SelectionController::FindSelectedTileUnderCursor(const Position& mouse_map_pos) const {
	Tile* tile = editor.map.getTile(mouse_map_pos);
	if (tile && tile->isSelected()) {
		return tile;
	}

	const bool show_all_floors = g_settings.getBoolean(Config::SHOW_ALL_FLOORS);
	if (!show_all_floors) {
		return tile;
	}

	if (!editor.selection.empty()) {
		const auto mode = SanitizeFloorVisibilityMode(g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE));
		const auto range = BuildFloorVisibilityRange(mouse_map_pos.z, show_all_floors, mode);
		const int max_z = std::max(range.start_floor, range.end_floor);

		for (int z = mouse_map_pos.z + 1; z <= max_z; ++z) {
			const int z_x = mouse_map_pos.x - MainMapViewMath::GetFloorTileOffset(mouse_map_pos.z) + MainMapViewMath::GetFloorTileOffset(z);
			const int z_y = mouse_map_pos.y - MainMapViewMath::GetFloorTileOffset(mouse_map_pos.z) + MainMapViewMath::GetFloorTileOffset(z);
			Tile* lower_tile = editor.map.getTile(z_x, z_y, z);
			if (lower_tile && lower_tile->isSelected()) {
				return lower_tile;
			}
		}
	}

	return tile;
}

SelectionHit SelectionController::PickAt(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset) const {
	return SelectionPicker::Pick(editor, mouse_map_pos, pixel_offset, SelectionPicker::ActiveMethod());
}

bool SelectionController::IsHitSelected(const SelectionHit& hit) const {
	switch (hit.kind) {
		case SelectionHitKind::Ground:
		case SelectionHitKind::Item:
			return hit.item && hit.item->isSelected();
		case SelectionHitKind::Creature:
			return hit.creature && hit.creature->isSelected();
		case SelectionHitKind::Spawn:
			return hit.spawn && hit.spawn->isSelected();
		case SelectionHitKind::None:
			return false;
	}
	return false;
}

void SelectionController::ApplyHit(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, const SelectionHit& hit, bool toggle) {
	if (!hit.tile) {
		return;
	}

	if (!toggle && IsHitSelected(hit) && editor.selection.size() == 1) {
		return;
	}

	editor.selection.start();
	SelectionHit effective_hit = hit;
	if (!toggle) {
		editor.selection.clear();
		editor.selection.commit();
		// Clearing swaps the tile at this position with a deep copy. The hit returned by the
		// picker points into the old tile and is dangling after the commit, so pick again from
		// the live map before selecting.
		effective_hit = PickAt(mouse_map_pos, pixel_offset);
	}

	const bool selected = IsHitSelected(effective_hit);
	switch (effective_hit.kind) {
		case SelectionHitKind::Ground:
		case SelectionHitKind::Item:
			if (effective_hit.item) {
				if (toggle && selected) {
					editor.selection.remove(effective_hit.tile, effective_hit.item);
				} else {
					editor.selection.add(effective_hit.tile, effective_hit.item);
				}
			}
			break;
		case SelectionHitKind::Creature:
			if (effective_hit.creature) {
				if (toggle && selected) {
					editor.selection.remove(effective_hit.tile, effective_hit.creature);
				} else {
					editor.selection.add(effective_hit.tile, effective_hit.creature);
				}
			}
			break;
		case SelectionHitKind::Spawn:
			if (effective_hit.spawn) {
				if (toggle && selected) {
					editor.selection.remove(effective_hit.tile, effective_hit.spawn);
				} else {
					editor.selection.add(effective_hit.tile, effective_hit.spawn);
				}
			}
			break;
		case SelectionHitKind::None:
			break;
	}

	editor.selection.finish();
	editor.selection.updateSelectionCount();
}

void SelectionController::ClearSelection() {
	editor.selection.start();
	editor.selection.clear();
	editor.selection.finish();
	editor.selection.updateSelectionCount();
}

void SelectionController::HandlePropertiesClick(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, bool shift_down, bool ctrl_down, bool alt_down) {
	last_click_pixel = pixel_offset;
	const bool pixel_mode = SelectionPicker::ActiveMethod() == SelectionMethod::PixelPerfect;
	const SelectionHit hit = pixel_mode ? PickAt(mouse_map_pos, pixel_offset) : SelectionHit {};
	Tile* tile = pixel_mode ? hit.tile : FindSelectedTileUnderCursor(mouse_map_pos);

	if (g_gui.IsDrawingMode()) {
		g_gui.SetSelectionMode();
	}

	canvas->EndPasting();

	boundbox_selection = false;
	if (shift_down) {
		boundbox_selection = true;

		if (!ctrl_down) {
			editor.selection.start(); // Start selection session
			editor.selection.clear(); // Clear out selection
			editor.selection.finish(); // End selection session
			editor.selection.updateSelectionCount();
		}
	} else if (!tile) {
		if (pixel_mode) {
			if (Tile* lower_tile = FindSelectedTileUnderCursor(mouse_map_pos); lower_tile && lower_tile != editor.map.getTile(mouse_map_pos)) {
				canvas->last_click_map_x = lower_tile->getX();
				canvas->last_click_map_y = lower_tile->getY();
				canvas->last_click_map_z = lower_tile->getZ();
				return;
			}
		}
		editor.selection.start(); // Start selection session
		editor.selection.clear(); // Clear out selection
		editor.selection.finish(); // End selection session
		editor.selection.updateSelectionCount();
	} else if (pixel_mode ? IsHitSelected(hit) : tile->isSelected()) {
		// Clicked on an already-selected entity (on active floor or visible floor below)
		canvas->last_click_map_x = tile->getX();
		canvas->last_click_map_y = tile->getY();
		canvas->last_click_map_z = tile->getZ();
	} else if (pixel_mode) {
		const Position storage_position = tile->getPosition();
		ApplyHit(mouse_map_pos, pixel_offset, hit, false);
		canvas->last_click_map_x = storage_position.x;
		canvas->last_click_map_y = storage_position.y;
		canvas->last_click_map_z = storage_position.z;
	} else {
		editor.selection.start(); // Start a selection session
		editor.selection.clear();
		editor.selection.commit();
		if (tile->spawn && g_settings.getInteger(Config::SHOW_SPAWNS)) {
			editor.selection.add(tile, tile->spawn.get());
		} else if (tile->creature && g_settings.getInteger(Config::SHOW_CREATURES)) {
			editor.selection.add(tile, tile->creature.get());
		} else {
			Item* item = tile->getTopItem();
			if (item) {
				editor.selection.add(tile, item);
			}
		}
		editor.selection.finish(); // Finish the selection session
		editor.selection.updateSelectionCount();
		canvas->last_click_map_x = tile->getX();
		canvas->last_click_map_y = tile->getY();
		canvas->last_click_map_z = tile->getZ();
	}
}

void SelectionController::HandlePropertiesRelease(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down) {
	if (g_gui.IsDrawingMode()) {
		g_gui.SetSelectionMode();
	}

	if (boundbox_selection) {
		if (mouse_map_pos.x == canvas->last_click_map_x && mouse_map_pos.y == canvas->last_click_map_y && ctrl_down) {
			// Mouse hasn't move, do control+shift thingy!
			Tile* tile = editor.map.getTile(mouse_map_pos);
			if (tile) {
				editor.selection.start(); // Start a selection session
				if (tile->isSelected()) {
					editor.selection.remove(tile);
				} else {
					editor.selection.add(tile);
				}
				editor.selection.finish(); // Finish the selection session
				editor.selection.updateSelectionCount();
			}
		} else {
			ExecuteBoundboxSelection(Position(canvas->last_click_map_x, canvas->last_click_map_y, canvas->last_click_map_z), mouse_map_pos, mouse_map_pos.z);
		}
	} else if (ctrl_down) {
		// Nothing
	}

	editor.actionQueue->resetTimer();
	dragging = false;
	boundbox_selection = false;
}

void SelectionController::HandleDoubleClick(const Position& mouse_map_pos) {
	dragging = false;
	boundbox_selection = false;
	if (g_settings.getInteger(Config::DOUBLECLICK_PROPERTIES)) {
		Tile* tile = nullptr;
		if (editor.selection.size() == 1) {
			tile = editor.selection.getSelectedTile();
		}
		if (!tile) {
			tile = FindSelectedTileUnderCursor(mouse_map_pos);
		}
		if (tile) {
			DialogHelper::OpenProperties(editor, tile);
		}
	}
}

void SelectionController::ExecuteBoundboxSelection(const Position& start_pos, const Position& end_pos, int floor) {
	int start_x = start_pos.x;
	int start_y = start_pos.y;
	int end_x = end_pos.x;
	int end_y = end_pos.y;

	if (start_x > end_x) {
		std::swap(start_x, end_x);
	}
	if (start_y > end_y) {
		std::swap(start_y, end_y);
	}

	int numtiles = 0;
	int threadcount = std::max(g_settings.getInteger(Config::WORKER_THREADS), 1);

	int s_x = 0, s_y = 0, s_z = 0;
	int e_x = 0, e_y = 0, e_z = 0;

	switch (g_settings.getInteger(Config::SELECTION_TYPE)) {
		case SELECT_CURRENT_FLOOR: {
			s_z = e_z = floor;
			s_x = start_x;
			s_y = start_y;
			e_x = end_x;
			e_y = end_y;
			numtiles = (e_x - s_x + 1) * (e_y - s_y + 1);
			break;
		}
		case SELECT_ALL_FLOORS: {
			s_x = start_x;
			s_y = start_y;
			s_z = MAP_MAX_LAYER;
			e_x = end_x;
			e_y = end_y;
			e_z = floor;

			if (g_settings.getInteger(Config::COMPENSATED_SELECT)) {
				s_x -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
				s_y -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);

				e_x -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
				e_y -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
			}

			numtiles = (s_z - e_z + 1) * (e_x - s_x + 1) * (e_y - s_y + 1);
			break;
		}
		case SELECT_VISIBLE_FLOORS: {
			s_x = start_x;
			s_y = start_y;
			const bool show_all_floors = g_settings.getBoolean(Config::SHOW_ALL_FLOORS);
			const auto mode = SanitizeFloorVisibilityMode(g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE));
			const auto range = BuildFloorVisibilityRange(floor, show_all_floors, mode);
			s_z = std::max({range.start_floor, range.end_floor, floor});
			e_z = std::max(std::min(range.start_floor, range.end_floor), floor);
			e_x = end_x;
			e_y = end_y;

			if (g_settings.getInteger(Config::COMPENSATED_SELECT)) {
				s_x -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
				s_y -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);

				e_x -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
				e_y -= (floor < GROUND_LAYER ? GROUND_LAYER - floor : 0);
			}

			numtiles = (s_z - e_z + 1) * (e_x - s_x + 1) * (e_y - s_y + 1);
			break;
		}
	}

	if (numtiles < 500) {
		// No point in threading for such a small set.
		threadcount = 1;
	}
	// Subdivide the selection area
	// We know it's a square, just split it into several areas
	int width = e_x - s_x;
	if (width < threadcount) {
		threadcount = std::min(1, width);
	}
	// Let's divide!
	int remainder = width;
	int cleared = 0;
	std::vector<std::unique_ptr<SelectionThread>> threads;
	if (width == 0) {
		threads.push_back(std::make_unique<SelectionThread>(editor, Position(s_x, s_y, s_z), Position(s_x, e_y, e_z)));
	} else {
		for (int i = 0; i < threadcount; ++i) {
			int chunksize = width / threadcount;
			// The last threads takes all the remainder
			if (i == threadcount - 1) {
				chunksize = remainder;
			}
			threads.push_back(std::make_unique<SelectionThread>(editor, Position(s_x + cleared, s_y, s_z), Position(s_x + cleared + chunksize, e_y, e_z)));
			cleared += chunksize;
			remainder -= chunksize;
		}
	}
	ASSERT(cleared == width);
	ASSERT(remainder == 0);

	editor.selection.start(); // Start a selection session
	for (auto& thread : threads) {
		thread->Start();
	}
	for (auto& thread : threads) {
		editor.selection.join(std::move(thread));
	}
	editor.selection.finish(); // Finish the selection session
	editor.selection.updateSelectionCount();
}
