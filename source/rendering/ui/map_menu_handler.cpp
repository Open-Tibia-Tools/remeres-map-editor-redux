//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include <set>
#include <vector>

#include "rendering/ui/map_menu_handler.h"
#include "rendering/ui/map_display.h"
#include "rendering/ui/clipboard_handler.h"
#include "rendering/ui/popup_action_handler.h"
#include "rendering/ui/brush_selector.h"

#include "ui/map_tab.h"
#include "ui/map_popup_menu.h"
#include "ui/map_window.h"
#include "map/tile.h"
#include "game/item.h"
#include "editor/editor.h"
#include "ui/gui_ids.h"
#include "ui/gui.h"
#include "ui/tile_properties/tile_properties_panel.h"
#include "lua/lua_script_manager.h"

MapMenuHandler::MapMenuHandler(MapCanvas* canvas, Editor& editor) :
	canvas(canvas),
	editor(editor) {
}

void MapMenuHandler::BindEvents() {
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnCut, this, MAP_POPUP_MENU_CUT);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnCopy, this, MAP_POPUP_MENU_COPY);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnCopyPosition, this, MAP_POPUP_MENU_COPY_POSITION);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnPaste, this, MAP_POPUP_MENU_PASTE);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnDelete, this, MAP_POPUP_MENU_DELETE);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnCopyServerId, this, MAP_POPUP_MENU_COPY_SERVER_ID);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnCopyClientId, this, MAP_POPUP_MENU_COPY_CLIENT_ID);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnCopyName, this, MAP_POPUP_MENU_COPY_NAME);

	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnRotateItem, this, MAP_POPUP_MENU_ROTATE);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnGotoDestination, this, MAP_POPUP_MENU_GOTO);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSwitchDoor, this, MAP_POPUP_MENU_SWITCH_DOOR);

	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectRAWBrush, this, MAP_POPUP_MENU_SELECT_RAW_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectGroundBrush, this, MAP_POPUP_MENU_SELECT_GROUND_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectDoodadBrush, this, MAP_POPUP_MENU_SELECT_DOODAD_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectCollectionBrush, this, MAP_POPUP_MENU_SELECT_COLLECTION_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectDoorBrush, this, MAP_POPUP_MENU_SELECT_DOOR_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectWallBrush, this, MAP_POPUP_MENU_SELECT_WALL_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectCarpetBrush, this, MAP_POPUP_MENU_SELECT_CARPET_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectTableBrush, this, MAP_POPUP_MENU_SELECT_TABLE_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectCreatureBrush, this, MAP_POPUP_MENU_SELECT_CREATURE_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectSpawnBrush, this, MAP_POPUP_MENU_SELECT_SPAWN_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnSelectHouseBrush, this, MAP_POPUP_MENU_SELECT_HOUSE_BRUSH);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnProperties, this, MAP_POPUP_MENU_PROPERTIES);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnAdvancedReplace, this, MAP_POPUP_MENU_ADVANCED_REPLACE);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnAddToCollection, this, MAP_POPUP_MENU_ADD_TO_COLLECTION);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnBrowseTile, this, MAP_POPUP_MENU_BROWSE_TILE);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnTileProperties, this, MAP_POPUP_MENU_TILE_PROPERTIES);
	for (int i = MAP_POPUP_MENU_SCRIPT_FIRST; i <= MAP_POPUP_MENU_SCRIPT_LAST; ++i) {
		canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnScriptMenu, this, i);
	}
}

void MapMenuHandler::OnCopy(wxCommandEvent& WXUNUSED(event)) {
	ClipboardHandler::copy(editor, canvas->GetFloor());
}

void MapMenuHandler::OnCut(wxCommandEvent& WXUNUSED(event)) {
	ClipboardHandler::cut(editor, canvas->GetFloor());
}

void MapMenuHandler::OnPaste(wxCommandEvent& WXUNUSED(event)) {
	ClipboardHandler::paste();
}

void MapMenuHandler::OnDelete(wxCommandEvent& WXUNUSED(event)) {
	ClipboardHandler::doDelete(editor);
}

void MapMenuHandler::OnCopyPosition(wxCommandEvent& WXUNUSED(event)) {
	ClipboardHandler::copyPosition(editor.selection);
}

void MapMenuHandler::OnCopyServerId(wxCommandEvent& WXUNUSED(event)) {
	ClipboardHandler::copyServerId(editor.selection);
}

void MapMenuHandler::OnCopyClientId(wxCommandEvent& WXUNUSED(event)) {
	ClipboardHandler::copyClientId(editor.selection);
}

void MapMenuHandler::OnCopyName(wxCommandEvent& WXUNUSED(event)) {
	ClipboardHandler::copyName(editor.selection);
}

void MapMenuHandler::OnBrowseTile(wxCommandEvent& WXUNUSED(event)) {
	PopupActionHandler::BrowseTile(editor, canvas->cursor_x, canvas->cursor_y);
}

void MapMenuHandler::OnTileProperties(wxCommandEvent& WXUNUSED(event)) {
	if (g_gui.tile_properties_panel) {
		wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(g_gui.tile_properties_panel);
		info.Show();
		g_gui.tile_properties_panel->UpdateFromEditor(&editor);
		g_gui.aui_manager->Update();
	}
}

void MapMenuHandler::OnScriptMenu(wxCommandEvent& event) {
	int index = event.GetId() - MAP_POPUP_MENU_SCRIPT_FIRST;
	const auto& menuItems = g_luaScripts.getContextMenuItems();

	if (index >= 0 && index < static_cast<int>(menuItems.size())) {
		try {
			const auto& item = menuItems[index];
			if (item.callback.valid()) {
				Tile* tile = editor.map.getTile(canvas->last_click_map_x, canvas->last_click_map_y, canvas->last_click_map_z);
				if (tile) {
					item.callback(tile);
				} else {
					item.callback(Position(canvas->last_click_map_x, canvas->last_click_map_y, canvas->last_click_map_z));
				}
			}
		} catch (const sol::error& e) {
			wxMessageBox(wxString("Script execution error: ") + e.what(), "Lua Error", wxOK | wxICON_ERROR);
		}
	}
}

void MapMenuHandler::OnRotateItem(wxCommandEvent& WXUNUSED(event)) {
	PopupActionHandler::RotateItem(editor);
}

void MapMenuHandler::OnGotoDestination(wxCommandEvent& WXUNUSED(event)) {
	PopupActionHandler::GotoDestination(editor);
}

void MapMenuHandler::OnSwitchDoor(wxCommandEvent& WXUNUSED(event)) {
	PopupActionHandler::SwitchDoor(editor);
}

void MapMenuHandler::OnSelectRAWBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectRAWBrush(editor.selection);
}

void MapMenuHandler::OnSelectGroundBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectGroundBrush(editor.selection);
}

void MapMenuHandler::OnSelectDoodadBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectDoodadBrush(editor.selection);
}

void MapMenuHandler::OnSelectDoorBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectDoorBrush(editor.selection);
}

void MapMenuHandler::OnSelectWallBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectWallBrush(editor.selection);
}

void MapMenuHandler::OnSelectCarpetBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectCarpetBrush(editor.selection);
}

void MapMenuHandler::OnSelectTableBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectTableBrush(editor.selection);
}

void MapMenuHandler::OnSelectHouseBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectHouseBrush(editor, editor.selection);
}

void MapMenuHandler::OnSelectCollectionBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectCollectionBrush(editor.selection);
}

void MapMenuHandler::OnSelectCreatureBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectCreatureBrush(editor.selection);
}

void MapMenuHandler::OnSelectSpawnBrush(wxCommandEvent& WXUNUSED(event)) {
	BrushSelector::SelectSpawnBrush();
}

void MapMenuHandler::OnProperties(wxCommandEvent& WXUNUSED(event)) {
	PopupActionHandler::OpenProperties(editor);
}

void MapMenuHandler::OnAdvancedReplace(wxCommandEvent& WXUNUSED(event)) {
	std::set<uint16_t> uniqueIds;
	for (Tile* tile : editor.selection.getTiles()) {
		if (tile->ground) {
			uint16_t id = tile->ground->getID();
			if (id != 0) {
				uniqueIds.insert(id);
			}
		}
		for (const auto& item : tile->items) {
			uint16_t id = item->getID();
			if (id != 0) {
				uniqueIds.insert(id);
			}
		}
	}

	if (uniqueIds.empty()) {
		return;
	}

	std::vector<uint16_t> sortedIds(uniqueIds.begin(), uniqueIds.end());
	MapTab* tab = g_gui.GetCurrentMapTab();
	if (tab) {
		tab->ShowAdvancedReplaceForSelection(sortedIds);
	}
}

void MapMenuHandler::OnAddToCollection(wxCommandEvent& WXUNUSED(event)) {
	MapTab* tab = g_gui.GetCurrentMapTab();
	if (tab) {
		tab->ShowCollectionStudio(&editor);
	}
}
