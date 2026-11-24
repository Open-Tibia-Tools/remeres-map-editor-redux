//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "rendering/ui/selection_controller.h"
#include "editor/selection_thread.h"
#include "rendering/ui/map_display.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "editor/operations/magic_wand.h"
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
#include "rendering/ui/selection_floor_scope.h"
#include "editor/lasso_geometry.h"

#include <optional>

namespace {

	constexpr int kWandViewportTolerance = 2;

	MagicWand::WandHit ToWandHit(const SelectionHit& hit) {
		MagicWand::WandHit wand;
		wand.tile = hit.tile;
		switch (hit.kind) {
			case SelectionHitKind::Ground:
				wand.kind = MagicWand::HitKind::Ground;
				wand.item = hit.item;
				break;
			case SelectionHitKind::Item:
				wand.kind = MagicWand::HitKind::Item;
				wand.item = hit.item;
				break;
			case SelectionHitKind::Creature:
				wand.kind = MagicWand::HitKind::Creature;
				wand.creature = hit.creature;
				break;
			case SelectionHitKind::Spawn:
			case SelectionHitKind::None:
				// Spawns are outside the wand's scope; the tile decides instead.
				break;
		}
		return wand;
	}

	// Bounding box of the visible part of `floor`, plus a small tolerance so a selection that
	// continues past the edge still has its border ring and edge tiles handled.
	PositionRect VisibleFloorRect(MapCanvas& canvas, int floor) {
		const wxSize size = canvas.GetClientSize();
		int left = 0;
		int top = 0;
		int right = 0;
		int bottom = 0;
		canvas.ScreenToMap(0, 0, floor, &left, &top);
		canvas.ScreenToMap(size.x, size.y, floor, &right, &bottom);
		const PositionRect rect = PositionRect::fromCorners(Position(left, top, floor), Position(right, bottom, floor));
		return rect.expanded(kWandViewportTolerance).clampedToMap();
	}

} // namespace

SelectionController::SelectionController(MapCanvas* canvas, Editor& editor) :
	canvas(canvas),
	editor(editor),
	dragging(false),
	boundbox_selection(false),
	drag_start_pos(Position()) {
}

SelectionController::~SelectionController() = default;

void SelectionController::HandleClick(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, bool shift_down, bool ctrl_down, bool alt_down) {
	last_click_pixel = pixel_offset;
	// A wand press whose release went elsewhere (outside the canvas, over the progress bar) must not swallow this click's release.
	magic_wand_click = false;
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

void SelectionController::HandleWandClick(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, bool ctrl_down) {
	// Held Tab owns the left press in every mode, so a drawing-mode click can never paint.
	Reset();
	magic_wand_click = true;
	MagicWand::WandHit wand = ToWandHit(PickAt(mouse_map_pos, pixel_offset));
	if (!wand.tile) {
		wand.tile = editor.map.getTile(mouse_map_pos);
	}
	const MagicWand::SearchMode mode = MagicWand::SanitizeSearchMode(g_settings.getInteger(Config::MAGIC_WAND_MODE));
	MagicWand::Select(editor, wand, VisibleFloorRect(*canvas, mouse_map_pos.z), ctrl_down, mode);
}

bool SelectionController::ConsumeWandRelease() {
	if (!magic_wand_click) {
		return false;
	}
	Reset();
	editor.actionQueue->resetTimer();
	return true;
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
	if (magic_wand_click) {
		Reset();
		editor.actionQueue->resetTimer();
		return;
	}
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

void SelectionController::HandleDoubleClick(const Position& mouse_map_pos, bool shift_down, bool alt_down) {
	if (canvas->IsWandHeld() && !shift_down && !alt_down) {
		// The first press of the double click already ran the wand; only its release is consumed.
		Reset();
		magic_wand_click = true;
		return;
	}

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
	const SelectionTileRect rect {
		.min_x = std::min(start_pos.x, end_pos.x),
		.min_y = std::min(start_pos.y, end_pos.y),
		.max_x = std::max(start_pos.x, end_pos.x),
		.max_y = std::max(start_pos.y, end_pos.y),
	};
	std::vector<std::unique_ptr<SelectionThread>> threads = CreateSelectionThreads(ComputeFloorScope(rect, floor), nullptr, 0, false);

	editor.selection.start(); // Start a selection session
	RunSelectionThreads(std::move(threads));
	editor.selection.finish(); // Finish the selection session
	editor.selection.updateSelectionCount();
}

static_assert(static_cast<int>(SelectionFloorScopeType::CurrentFloor) == SELECT_CURRENT_FLOOR);
static_assert(static_cast<int>(SelectionFloorScopeType::AllFloors) == SELECT_ALL_FLOORS);
static_assert(static_cast<int>(SelectionFloorScopeType::VisibleFloors) == SELECT_VISIBLE_FLOORS);

SelectionFloorScope SelectionController::ComputeFloorScope(const SelectionTileRect& rect, int floor) const {
	const bool show_all_floors = g_settings.getBoolean(Config::SHOW_ALL_FLOORS);
	const auto mode = SanitizeFloorVisibilityMode(g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE));
	return ComputeSelectionFloorScope(
		rect,
		floor,
		static_cast<SelectionFloorScopeType>(g_settings.getInteger(Config::SELECTION_TYPE)),
		g_settings.getInteger(Config::COMPENSATED_SELECT) != 0,
		BuildFloorVisibilityRange(floor, show_all_floors, mode)
	);
}

std::vector<std::unique_ptr<SelectionThread>> SelectionController::CreateSelectionThreads(const SelectionFloorScope& scope, const LassoPolygon* polygon, int polygon_offset, bool deselect) const {
	std::vector<std::unique_ptr<SelectionThread>> threads;
	for (const SelectionBand& band : ComputeSelectionBands(scope, g_settings.getInteger(Config::WORKER_THREADS))) {
		const Position band_start(band.start_x, scope.start_y, scope.start_z);
		const Position band_end(band.end_x, scope.end_y, scope.end_z);
		if (polygon) {
			threads.push_back(std::make_unique<SelectionThread>(editor, band_start, band_end, *polygon, polygon_offset, deselect));
		} else {
			threads.push_back(std::make_unique<SelectionThread>(editor, band_start, band_end));
		}
	}
	return threads;
}

void SelectionController::RunSelectionThreads(std::vector<std::unique_ptr<SelectionThread>> threads) {
	for (auto& thread : threads) {
		thread->Start();
	}
	for (auto& thread : threads) {
		editor.selection.join(std::move(thread));
	}
}

bool SelectionController::CanStartLasso() const {
	return g_gui.IsSelectionMode() && !canvas->isPasting() && !dragging && !boundbox_selection && !canvas->screendragging;
}

SelectionController::LassoViewport SelectionController::CaptureLassoViewport() const {
	LassoViewport viewport;
	canvas->GetViewBox(&viewport.view_x, &viewport.view_y, &viewport.screen_width, &viewport.screen_height);
	viewport.content_scale = canvas->GetContentScaleFactor();
	viewport.zoom = canvas->GetZoom();
	viewport.floor = canvas->GetFloor();
	return viewport;
}

bool SelectionController::HandleLassoButtonDown(LassoButton button, LassoScreenPoint point, LassoModifiers mods) {
	const bool can_start = button == LassoButton::Left && !lasso.IsActive() && CanStartLasso();
	const LassoEvent event = lasso.OnButtonDown(button, point, mods, can_start, canvas->FromDIP(LassoGesture::kSampleSpacingDip));
	ApplyLassoEvent(event);
	return IsLassoEventConsumed(event);
}

bool SelectionController::HandleLassoButtonUp(LassoButton button, LassoScreenPoint point, LassoModifiers mods) {
	const LassoEvent event = lasso.OnButtonUp(button, point, mods);
	ApplyLassoEvent(event);
	return IsLassoEventConsumed(event);
}

bool SelectionController::HandleLassoMotion(LassoScreenPoint point, LassoModifiers mods, bool left_down) {
	const LassoEvent event = lasso.OnMotion(point, mods, left_down);
	ApplyLassoEvent(event);
	return IsLassoEventConsumed(event);
}

bool SelectionController::HandleLassoKeyDown(int key_code, LassoModifiers mods) {
	if (!lasso.IsActive()) {
		return false;
	}

	// Every key is consumed while the lasso is active: navigation is frozen, and not skipping the
	// Alt auto-repeat keeps WM_SYSKEYDOWN away from DefWindowProc (no menu bar activation).
	switch (key_code) {
		case WXK_ESCAPE:
			ApplyLassoEvent(lasso.Cancel());
			return true;
		case WXK_ALT:
			mods.alt = true;
			break;
		case WXK_SHIFT:
			mods.shift = true;
			break;
		case WXK_CONTROL:
			mods.ctrl = true;
			break;
		default:
			break;
	}
	ApplyLassoEvent(lasso.OnModifierKey(mods));
	return true;
}

bool SelectionController::HandleLassoKeyUp(int key_code, LassoModifiers mods) {
	if (!lasso.IsActive()) {
		return false;
	}

	switch (key_code) {
		case WXK_ALT:
			ApplyLassoEvent(lasso.OnAltReleased(mods));
			return true;
		case WXK_SHIFT:
			mods.shift = false;
			break;
		case WXK_CONTROL:
			mods.ctrl = false;
			break;
		default:
			break;
	}
	ApplyLassoEvent(lasso.OnModifierKey(mods));
	return true;
}

void SelectionController::CancelLasso() {
	ApplyLassoEvent(lasso.Cancel());
}

void SelectionController::ApplyLassoEvent(LassoEvent event) {
	switch (event) {
		case LassoEvent::Ignored:
			return;
		case LassoEvent::Started:
			lasso_viewport = CaptureLassoViewport();
			ClearLassoPreview();
			break;
		case LassoEvent::Committed:
			ClearLassoPreview();
			// The points are screen coordinates; applying them after the view moved would select
			// a different area than the one drawn.
			if (CaptureLassoViewport() != lasso_viewport) {
				g_gui.SetStatusText("Lasso cancelled: the view changed while drawing.");
			} else {
				ExecuteLassoSelection(lasso.Points(), lasso.Mode());
				g_gui.SetStatusText("Lasso selection applied.");
			}
			break;
		case LassoEvent::Discarded:
			ClearLassoPreview();
			g_gui.SetStatusText("Lasso discarded: it needs at least 3 points enclosing an area.");
			break;
		case LassoEvent::Cancelled:
			ClearLassoPreview();
			g_gui.SetStatusText("Lasso cancelled.");
			break;
		case LassoEvent::Consumed:
			break;
	}

	if (lasso.IsActive()) {
		const LassoMode mode = lasso.Mode();
		const std::size_t point_count = lasso.Points().size();
		const bool at_point_limit = lasso.IsAtPointLimit();
		if (!lasso_status_valid || mode != lasso_status_mode || point_count != lasso_status_points || at_point_limit != lasso_status_at_limit) {
			lasso_status_mode = mode;
			lasso_status_points = point_count;
			lasso_status_at_limit = at_point_limit;
			lasso_status_valid = true;
			g_gui.SetStatusText(wxstr(FormatLassoStatus(mode, point_count, at_point_limit)));
		}
	} else {
		lasso_status_valid = false;
	}
	SyncLassoMouseCapture();

	// Build here (input event), not in the render loop; the preview computes NanoVG tile-outline
	// segments only, it never touches the chunk cache or any VBO.
	if (lasso.IsActive()) {
		UpdateLassoPreview();
	}

	if (event == LassoEvent::Committed) {
		canvas->RequestSharedMapRefresh();
	} else {
		canvas->RequestLocalRefresh();
	}
}

void SelectionController::SyncLassoMouseCapture() {
	const bool wants_capture = lasso.IsLeftDown();
	if (wants_capture && !canvas->HasCapture()) {
		canvas->CaptureMouse();
	} else if (!wants_capture && canvas->HasCapture()) {
		canvas->ReleaseMouse();
	}
}

LassoPoint SelectionController::ConvertLassoPoint(const LassoScreenPoint& point) const {
	const MainMapPixelPosition position = canvas->ScreenToMapPixel(point.x, point.y);
	return LassoPoint {
		.x = position.map_x + static_cast<double>(position.pixel_x) / TILE_SIZE,
		.y = position.map_y + static_cast<double>(position.pixel_y) / TILE_SIZE,
	};
}

void SelectionController::FillLassoPolygonPoints(std::span<const LassoScreenPoint> points) {
	lasso_tile_points.clear();
	lasso_tile_points.reserve(points.size());
	for (const LassoScreenPoint& point : points) {
		lasso_tile_points.push_back(ConvertLassoPoint(point));
	}
	lasso_tile_points_converted = points.size();
}

void SelectionController::UpdateLassoTilePoints(std::span<const LassoScreenPoint> points, bool append_cursor, const LassoScreenPoint& cursor) {
	if (points.size() < lasso_tile_points_converted) {
		// The gesture restarted; the cached conversions belong to another polygon.
		lasso_tile_points.clear();
		lasso_tile_points_converted = 0;
	}
	lasso_tile_points.resize(points.size());
	for (std::size_t i = lasso_tile_points_converted; i < points.size(); ++i) {
		lasso_tile_points[i] = ConvertLassoPoint(points[i]);
	}
	lasso_tile_points_converted = points.size();
	lasso_tile_points.resize(points.size() + (append_cursor ? 1 : 0));
	if (append_cursor) {
		lasso_tile_points[points.size()] = ConvertLassoPoint(cursor);
	}
}

void SelectionController::ClearLassoPreview() {
	lasso_preview_polygon.reset();
	lasso_preview_points = 0;
	lasso_preview_cursor = {};
	lasso_tile_points_converted = 0;
	outline_scratch.clear();
}

void SelectionController::ValidateLassoPreviewView() {
	if (!lasso.IsActive() || !lasso_preview_polygon.has_value()) {
		return;
	}
	if (CaptureLassoViewport() == lasso_viewport) {
		return;
	}

	// External navigation (menu zoom/floor/scroll) invalidates the preview. Do not rebuild here:
	// this runs in the render loop and must not allocate. Reset the key so the next input can
	// rebuild the preview if the view returns.
	ClearLassoPreview();
}

void SelectionController::UpdateLassoPreview() {
	if (!lasso.IsActive()) {
		if (lasso_preview_polygon.has_value()) {
			ClearLassoPreview();
		}
		return;
	}

	const std::span<const LassoScreenPoint> points = lasso.Points();
	// OnAltReleased appends this cursor to the applied polygon; include it in the preview too so
	// click-vertex lassos show exactly the tiles that will be selected.
	const LassoScreenPoint cursor = lasso.Cursor();
	const bool append_cursor = points.size() < LassoGesture::kMaxPoints && (points.empty() || points.back() != cursor);
	const std::size_t preview_point_count = points.size() + (append_cursor ? 1 : 0);
	const LassoScreenPoint preview_cursor = append_cursor ? cursor : points.back();

	if (preview_point_count == lasso_preview_points && preview_cursor == lasso_preview_cursor) {
		return;
	}

	// The gesture is screen-space; a resize, DPI change or external navigation invalidates it.
	if (CaptureLassoViewport() != lasso_viewport) {
		// Reset the rebuild key so a later input can recover if the view returns.
		ClearLassoPreview();
		return;
	}

	lasso_preview_points = preview_point_count;
	lasso_preview_cursor = preview_cursor;

	if (preview_point_count < 3 || !HasLassoAreaWithCursor(points, cursor, append_cursor)) {
		// The gesture is discarded on release, so the real selection will not change.
		ClearLassoPreview();
		return;
	}

	UpdateLassoTilePoints(points, append_cursor, cursor);
	LassoPolygon polygon(lasso_tile_points);
	lasso_preview_polygon = std::move(polygon);

	// Compute the tile-set boundary: O(perimeter), no chunk-cache or VBO interaction.
	LassoOutline::Compute(*lasso_preview_polygon, editor.map.getWidth(), editor.map.getHeight(), outline_scratch);
}

void SelectionController::ExecuteLassoSelection(std::span<const LassoScreenPoint> points, LassoMode mode) {
	FillLassoPolygonPoints(points);
	const LassoPolygon polygon(lasso_tile_points);
	// Compensated selection shifts lower floors by up to this many tiles, so the polygon may reach
	// past the map edge in current-floor space and still cover tiles on those floors (box selection
	// does not clamp either).
	const int floor = canvas->GetFloor();
	const int floor_shift = ComputeSelectionFloorShift(
		static_cast<SelectionFloorScopeType>(g_settings.getInteger(Config::SELECTION_TYPE)),
		g_settings.getInteger(Config::COMPENSATED_SELECT) != 0,
		floor
	);
	const std::optional<LassoTileBounds> bounds = ComputeLassoTileBounds(polygon, editor.map.getWidth() + floor_shift, editor.map.getHeight() + floor_shift);
	const bool replace = mode == LassoMode::Replace;
	if (!bounds && !replace) {
		return;
	}

	editor.selection.start(); // One session, so Replace's clear and the new tiles undo together
	if (replace) {
		editor.selection.clear();
		editor.selection.commit();
	}
	if (bounds) {
		const SelectionTileRect rect {
			.min_x = bounds->min_x,
			.min_y = bounds->min_y,
			.max_x = bounds->max_x,
			.max_y = bounds->max_y,
		};
		const SelectionFloorScope scope = ComputeFloorScope(rect, floor);
		RunSelectionThreads(CreateSelectionThreads(scope, &polygon, scope.StartOffset(rect), mode == LassoMode::Subtract));
	}
	editor.selection.finish(); // Finish the selection session
	editor.selection.updateSelectionCount();
	editor.actionQueue->resetTimer();
}
