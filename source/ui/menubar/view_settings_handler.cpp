#include "ui/menubar/view_settings_handler.h"
#include "rendering/core/floor_visibility_mode.h"
#include "ui/main_menubar.h"
#include "ui/gui.h"
#include "app/preferences.h"
#include "map/map.h"
#include <format>

ViewSettingsHandler::ViewSettingsHandler(MainMenuBar* menuBar) :
	menuBar(menuBar) {
}

void ViewSettingsHandler::LoadValues() {
	using namespace MenuBar;

	g_settings.setInteger(Config::SHOW_AS_MINIMAP, 0);

	menuBar->CheckItem(VIEW_TOOLBARS_BRUSHES, g_settings.getBoolean(Config::SHOW_TOOLBAR_BRUSHES));
	menuBar->CheckItem(VIEW_TOOLBARS_POSITION, g_settings.getBoolean(Config::SHOW_TOOLBAR_POSITION));
	menuBar->CheckItem(VIEW_TOOLBARS_SIZES, g_settings.getBoolean(Config::SHOW_TOOLBAR_SIZES));
	menuBar->CheckItem(VIEW_TOOLBARS_STANDARD, g_settings.getBoolean(Config::SHOW_TOOLBAR_STANDARD));
	menuBar->CheckItem(VIEW_TOOLBARS_LIGHT, g_settings.getBoolean(Config::SHOW_TOOLBAR_LIGHT));

	menuBar->CheckItem(SELECT_MODE_COMPENSATE, g_settings.getBoolean(Config::COMPENSATED_SELECT));

	if (menuBar->IsItemChecked(MenuBar::SELECT_MODE_CURRENT)) {
		g_settings.setInteger(Config::SELECTION_TYPE, SELECT_CURRENT_FLOOR);
	} else if (menuBar->IsItemChecked(MenuBar::SELECT_MODE_LOWER)) {
		g_settings.setInteger(Config::SELECTION_TYPE, SELECT_ALL_FLOORS);
	} else if (menuBar->IsItemChecked(MenuBar::SELECT_MODE_VISIBLE)) {
		g_settings.setInteger(Config::SELECTION_TYPE, SELECT_VISIBLE_FLOORS);
	}

	switch (g_settings.getInteger(Config::SELECTION_TYPE)) {
		case SELECT_CURRENT_FLOOR:
			menuBar->CheckItem(SELECT_MODE_CURRENT, true);
			break;
		case SELECT_ALL_FLOORS:
			menuBar->CheckItem(SELECT_MODE_LOWER, true);
			break;
		default:
		case SELECT_VISIBLE_FLOORS:
			menuBar->CheckItem(SELECT_MODE_VISIBLE, true);
			break;
	}

	const bool pixel_perfect = g_settings.getBoolean(Config::PIXEL_PERFECT_SELECTION);
	menuBar->CheckItem(SELECT_METHOD_STANDARD, !pixel_perfect);
	menuBar->CheckItem(SELECT_METHOD_PIXEL_PERFECT, pixel_perfect);

	menuBar->CheckItem(AUTOMAGIC, g_settings.getBoolean(Config::USE_AUTOMAGIC));

	const bool supports_zones = g_gui.IsEditorOpen() && g_gui.GetCurrentMap().supportsZones();

	menuBar->CheckItem(SHOW_SHADE, g_settings.getBoolean(Config::SHOW_SHADE));
	menuBar->CheckItem(SHOW_INGAME_BOX, g_settings.getBoolean(Config::SHOW_INGAME_BOX));
	menuBar->CheckItem(SHOW_LIGHTS, g_settings.getBoolean(Config::SHOW_LIGHTS));
	menuBar->CheckItem(SHOW_LIGHT_STR, g_settings.getBoolean(Config::SHOW_LIGHT_STR));
	menuBar->CheckItem(SHOW_TECHNICAL_ITEMS, g_settings.getBoolean(Config::SHOW_TECHNICAL_ITEMS));
	menuBar->CheckItem(SHOW_INVALID_TILES, g_settings.getBoolean(Config::SHOW_INVALID_TILES));
	menuBar->CheckItem(SHOW_INVALID_ZONES, g_settings.getBoolean(Config::SHOW_INVALID_ZONES));
	menuBar->CheckItem(SHOW_WAYPOINTS, g_settings.getBoolean(Config::SHOW_WAYPOINTS));
	menuBar->CheckItem(SHOW_SCRIPT_MARKERS, g_settings.getBoolean(Config::SHOW_SCRIPT_MARKERS));
	menuBar->CheckItem(SHOW_DEPOT_MARKERS, g_settings.getBoolean(Config::SHOW_DEPOT_MARKERS));
	menuBar->CheckItem(SHOW_RAID_MARKERS, g_settings.getBoolean(Config::SHOW_RAID_MARKERS));
	menuBar->CheckItem(SHOW_START_MARKERS, g_settings.getBoolean(Config::SHOW_START_MARKERS));
	const bool show_all_floors = g_settings.getBoolean(Config::SHOW_ALL_FLOORS);
	const auto floor_visibility_mode = SanitizeFloorVisibilityMode(g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE));
	menuBar->CheckItem(SHOW_ALL_FLOORS, show_all_floors);
	menuBar->CheckItem(SHOW_ALL_FLOORS_CLIENT, show_all_floors && floor_visibility_mode == FloorVisibilityMode::ClientVisible);
	menuBar->CheckItem(SHOW_ALL_FLOORS_ALL, show_all_floors && floor_visibility_mode == FloorVisibilityMode::AllVisible);
	menuBar->CheckItem(GHOST_ITEMS, g_settings.getBoolean(Config::TRANSPARENT_ITEMS));
	menuBar->CheckItem(GHOST_HIGHER_FLOORS, g_settings.getBoolean(Config::TRANSPARENT_FLOORS));
	menuBar->CheckItem(SHOW_EXTRA, !g_settings.getBoolean(Config::SHOW_EXTRA));
	menuBar->CheckItem(SHOW_GRID, g_settings.getBoolean(Config::SHOW_GRID));
	menuBar->CheckItem(HIGHLIGHT_ITEMS, g_settings.getBoolean(Config::HIGHLIGHT_ITEMS));
	menuBar->CheckItem(HIGHLIGHT_LOCKED_DOORS, g_settings.getBoolean(Config::HIGHLIGHT_LOCKED_DOORS));
	menuBar->CheckItem(SHOW_CREATURES, g_settings.getBoolean(Config::SHOW_CREATURES));
	menuBar->CheckItem(SHOW_SPAWNS, g_settings.getBoolean(Config::SHOW_SPAWNS));
	menuBar->CheckItem(SHOW_SPECIAL, g_settings.getBoolean(Config::SHOW_SPECIAL_TILES));
	menuBar->CheckItem(SHOW_AS_MINIMAP, g_settings.getBoolean(Config::SHOW_AS_MINIMAP));
	menuBar->CheckItem(SHOW_ONLY_COLORS, g_settings.getBoolean(Config::SHOW_ONLY_TILEFLAGS));
	menuBar->CheckItem(SHOW_ONLY_MODIFIED, g_settings.getBoolean(Config::SHOW_ONLY_MODIFIED_TILES));
	menuBar->CheckItem(SHOW_HOUSES, g_settings.getBoolean(Config::SHOW_HOUSES));
	menuBar->CheckItem(SHOW_ZONES, supports_zones && g_settings.getBoolean(Config::SHOW_ZONES));
	menuBar->CheckItem(SHOW_PATHING, g_settings.getBoolean(Config::SHOW_BLOCKING));
	menuBar->CheckItem(SHOW_TOOLTIPS, g_settings.getBoolean(Config::SHOW_TOOLTIPS));
	menuBar->CheckItem(SHOW_PREVIEW, g_settings.getBoolean(Config::SHOW_PREVIEW));
	menuBar->CheckItem(SHOW_WALL_HOOKS, g_settings.getBoolean(Config::SHOW_WALL_HOOKS));
	menuBar->CheckItem(SHOW_TOWNS, g_settings.getBoolean(Config::SHOW_TOWNS));
	menuBar->CheckItem(ALWAYS_SHOW_ZONES, g_settings.getBoolean(Config::ALWAYS_SHOW_ZONES));
	menuBar->CheckItem(EXT_HOUSE_SHADER, g_settings.getBoolean(Config::EXT_HOUSE_SHADER));
	menuBar->CheckItem(EXT_PATHING_SHADER, g_settings.getBoolean(Config::EXT_PATHING_SHADER));

}

void ViewSettingsHandler::OnChangeViewSettings(wxCommandEvent& event) {
	using namespace MenuBar;

	bool old_grid = g_settings.getBoolean(Config::SHOW_GRID);
	bool old_ghost = g_settings.getBoolean(Config::TRANSPARENT_ITEMS);
	bool old_ghost_floors = g_settings.getBoolean(Config::TRANSPARENT_FLOORS);

	const bool supports_zones = g_gui.IsEditorOpen() && g_gui.GetCurrentMap().supportsZones();

	const ActionID action_id = static_cast<ActionID>(event.GetId() - (wxID_HIGHEST + 1));
	if (action_id == SHOW_ALL_FLOORS) {
		const bool current = g_settings.getBoolean(Config::SHOW_ALL_FLOORS);
		g_settings.setInteger(Config::SHOW_ALL_FLOORS, !current);
	} else if (action_id == SHOW_ALL_FLOORS_CLIENT) {
		const bool current_show = g_settings.getBoolean(Config::SHOW_ALL_FLOORS);
		const auto current_mode = SanitizeFloorVisibilityMode(g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE));
		if (current_show && current_mode == FloorVisibilityMode::ClientVisible) {
			g_settings.setInteger(Config::SHOW_ALL_FLOORS, false);
		} else {
			g_settings.setInteger(Config::SHOW_ALL_FLOORS, true);
			g_settings.setInteger(Config::FLOOR_VISIBILITY_MODE, static_cast<int>(FloorVisibilityMode::ClientVisible));
		}
	} else if (action_id == SHOW_ALL_FLOORS_ALL) {
		const bool current_show = g_settings.getBoolean(Config::SHOW_ALL_FLOORS);
		const auto current_mode = SanitizeFloorVisibilityMode(g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE));
		if (current_show && current_mode == FloorVisibilityMode::AllVisible) {
			g_settings.setInteger(Config::SHOW_ALL_FLOORS, false);
		} else {
			g_settings.setInteger(Config::SHOW_ALL_FLOORS, true);
			g_settings.setInteger(Config::FLOOR_VISIBILITY_MODE, static_cast<int>(FloorVisibilityMode::AllVisible));
		}
	}

	const bool show_all_floors = g_settings.getBoolean(Config::SHOW_ALL_FLOORS);
	if (show_all_floors) {
		menuBar->EnableItem(SELECT_MODE_VISIBLE, true);
		menuBar->EnableItem(SELECT_MODE_LOWER, true);
	} else {
		menuBar->EnableItem(SELECT_MODE_VISIBLE, false);
		menuBar->EnableItem(SELECT_MODE_LOWER, false);
		menuBar->CheckItem(SELECT_MODE_CURRENT, true);
		g_settings.setInteger(Config::SELECTION_TYPE, SELECT_CURRENT_FLOOR);
	}
	g_settings.setInteger(Config::TRANSPARENT_FLOORS, menuBar->IsItemChecked(GHOST_HIGHER_FLOORS));
	g_settings.setInteger(Config::TRANSPARENT_ITEMS, menuBar->IsItemChecked(GHOST_ITEMS));
	g_settings.setInteger(Config::SHOW_INGAME_BOX, menuBar->IsItemChecked(SHOW_INGAME_BOX));
	g_settings.setInteger(Config::SHOW_LIGHTS, menuBar->IsItemChecked(SHOW_LIGHTS));
	g_settings.setInteger(Config::SHOW_LIGHT_STR, menuBar->IsItemChecked(SHOW_LIGHT_STR));
	g_settings.setInteger(Config::SHOW_TECHNICAL_ITEMS, menuBar->IsItemChecked(SHOW_TECHNICAL_ITEMS));
	g_settings.setInteger(Config::SHOW_INVALID_TILES, menuBar->IsItemChecked(SHOW_INVALID_TILES));
	g_settings.setInteger(Config::SHOW_WAYPOINTS, menuBar->IsItemChecked(SHOW_WAYPOINTS));
	g_settings.setInteger(Config::SHOW_SCRIPT_MARKERS, menuBar->IsItemChecked(SHOW_SCRIPT_MARKERS));
	g_settings.setInteger(Config::SHOW_DEPOT_MARKERS, menuBar->IsItemChecked(SHOW_DEPOT_MARKERS));
	g_settings.setInteger(Config::SHOW_RAID_MARKERS, menuBar->IsItemChecked(SHOW_RAID_MARKERS));
	g_settings.setInteger(Config::SHOW_START_MARKERS, menuBar->IsItemChecked(SHOW_START_MARKERS));
	g_settings.setInteger(Config::SHOW_GRID, menuBar->IsItemChecked(SHOW_GRID));
	g_settings.setInteger(Config::SHOW_EXTRA, !menuBar->IsItemChecked(SHOW_EXTRA));

	g_settings.setInteger(Config::SHOW_SHADE, menuBar->IsItemChecked(SHOW_SHADE));
	g_settings.setInteger(Config::SHOW_SPECIAL_TILES, menuBar->IsItemChecked(SHOW_SPECIAL));
	g_settings.setInteger(Config::SHOW_AS_MINIMAP, menuBar->IsItemChecked(SHOW_AS_MINIMAP));
	g_settings.setInteger(Config::SHOW_ONLY_TILEFLAGS, menuBar->IsItemChecked(SHOW_ONLY_COLORS));
	g_settings.setInteger(Config::SHOW_ONLY_MODIFIED_TILES, menuBar->IsItemChecked(SHOW_ONLY_MODIFIED));
	g_settings.setInteger(Config::SHOW_CREATURES, menuBar->IsItemChecked(SHOW_CREATURES));
	g_settings.setInteger(Config::SHOW_SPAWNS, menuBar->IsItemChecked(SHOW_SPAWNS));
	g_settings.setInteger(Config::SHOW_HOUSES, menuBar->IsItemChecked(SHOW_HOUSES));
	g_settings.setInteger(Config::HIGHLIGHT_ITEMS, menuBar->IsItemChecked(HIGHLIGHT_ITEMS));
	g_settings.setInteger(Config::HIGHLIGHT_LOCKED_DOORS, menuBar->IsItemChecked(HIGHLIGHT_LOCKED_DOORS));
	g_settings.setInteger(Config::SHOW_BLOCKING, menuBar->IsItemChecked(SHOW_PATHING));
	g_settings.setInteger(Config::SHOW_TOOLTIPS, menuBar->IsItemChecked(SHOW_TOOLTIPS));
	g_settings.setInteger(Config::SHOW_PREVIEW, menuBar->IsItemChecked(SHOW_PREVIEW));
	g_settings.setInteger(Config::SHOW_WALL_HOOKS, menuBar->IsItemChecked(SHOW_WALL_HOOKS));
	g_settings.setInteger(Config::SHOW_TOWNS, menuBar->IsItemChecked(SHOW_TOWNS));

	g_settings.setInteger(Config::SHOW_INVALID_ZONES, menuBar->IsItemChecked(SHOW_INVALID_ZONES));
	g_settings.setInteger(Config::ALWAYS_SHOW_ZONES, menuBar->IsItemChecked(ALWAYS_SHOW_ZONES));

	if (supports_zones) {
		g_settings.setInteger(Config::SHOW_ZONES, menuBar->IsItemChecked(SHOW_ZONES));
	}
	g_settings.setInteger(Config::EXT_HOUSE_SHADER, menuBar->IsItemChecked(EXT_HOUSE_SHADER));
	g_settings.setInteger(Config::EXT_PATHING_SHADER, menuBar->IsItemChecked(EXT_PATHING_SHADER));

	bool new_grid = g_settings.getBoolean(Config::SHOW_GRID);
	if (old_grid != new_grid) {
		g_gui.SetStatusText(std::format("Grid: {}", new_grid ? "On" : "Off"));
	}

	bool new_ghost = g_settings.getBoolean(Config::TRANSPARENT_ITEMS);
	if (old_ghost != new_ghost) {
		g_gui.SetStatusText(std::format("Ghost Mode: {}", new_ghost ? "On" : "Off"));
	}

	bool new_ghost_floors = g_settings.getBoolean(Config::TRANSPARENT_FLOORS);
	if (old_ghost_floors != new_ghost_floors) {
		g_gui.SetStatusText(std::format("Higher Floors: {}", new_ghost_floors ? "On" : "Off"));
	}

	menuBar->UpdateFloorMenu();
	g_gui.RefreshView();
}

void ViewSettingsHandler::OnToolbars(wxCommandEvent& event) {
	using namespace MenuBar;

	ActionID id = static_cast<ActionID>(event.GetId() - (wxID_HIGHEST + 1));
	switch (id) {
		case VIEW_TOOLBARS_BRUSHES:
			g_gui.ShowToolbar(TOOLBAR_BRUSHES, event.IsChecked());
			g_settings.setInteger(Config::SHOW_TOOLBAR_BRUSHES, event.IsChecked());
			break;
		case VIEW_TOOLBARS_POSITION:
			g_gui.ShowToolbar(TOOLBAR_POSITION, event.IsChecked());
			g_settings.setInteger(Config::SHOW_TOOLBAR_POSITION, event.IsChecked());
			break;
		case VIEW_TOOLBARS_SIZES:
			g_gui.ShowToolbar(TOOLBAR_SIZES, event.IsChecked());
			g_settings.setInteger(Config::SHOW_TOOLBAR_SIZES, event.IsChecked());
			break;
		case VIEW_TOOLBARS_STANDARD:
			g_gui.ShowToolbar(TOOLBAR_STANDARD, event.IsChecked());
			g_settings.setInteger(Config::SHOW_TOOLBAR_STANDARD, event.IsChecked());
			break;
		case VIEW_TOOLBARS_LIGHT:
			g_gui.ShowToolbar(TOOLBAR_LIGHT, event.IsChecked());
			g_settings.setInteger(Config::SHOW_TOOLBAR_LIGHT, event.IsChecked());
			break;
		default:
			break;
	}
}

void ViewSettingsHandler::OnToggleAutomagic(wxCommandEvent& WXUNUSED(event)) {
	using namespace MenuBar;
	g_settings.setInteger(Config::USE_AUTOMAGIC, menuBar->IsItemChecked(AUTOMAGIC));
	g_settings.setInteger(Config::BORDER_IS_GROUND, menuBar->IsItemChecked(AUTOMAGIC));
	if (g_settings.getInteger(Config::USE_AUTOMAGIC)) {
		g_gui.SetStatusText("Automagic enabled.");
	} else {
		g_gui.SetStatusText("Automagic disabled.");
	}
}

void ViewSettingsHandler::OnSelectionTypeChange(wxCommandEvent& WXUNUSED(event)) {
	using namespace MenuBar;
	g_settings.setInteger(Config::COMPENSATED_SELECT, menuBar->IsItemChecked(SELECT_MODE_COMPENSATE));

	if (menuBar->IsItemChecked(SELECT_MODE_CURRENT)) {
		g_settings.setInteger(Config::SELECTION_TYPE, SELECT_CURRENT_FLOOR);
	} else if (menuBar->IsItemChecked(SELECT_MODE_LOWER)) {
		g_settings.setInteger(Config::SELECTION_TYPE, SELECT_ALL_FLOORS);
	} else if (menuBar->IsItemChecked(SELECT_MODE_VISIBLE)) {
		g_settings.setInteger(Config::SELECTION_TYPE, SELECT_VISIBLE_FLOORS);
	}
}

void ViewSettingsHandler::OnSelectionMethodChange(wxCommandEvent& WXUNUSED(event)) {
	using namespace MenuBar;
	const bool pixel_perfect = menuBar->IsItemChecked(SELECT_METHOD_PIXEL_PERFECT);
	g_settings.setInteger(Config::PIXEL_PERFECT_SELECTION, pixel_perfect ? 1 : 0);
	g_gui.RefreshView();
}
