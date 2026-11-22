//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "ui/tile_context_actions.h"

#include <set>
#include <vector>

#include "editor/editor.h"
#include "game/item.h"
#include "lua/lua_script_manager.h"
#include "map/map.h"
#include "map/tile.h"
#include "rendering/ui/brush_selector.h"
#include "rendering/ui/clipboard_handler.h"
#include "rendering/ui/popup_action_handler.h"
#include "ui/dialog_util.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/map_tab.h"
#include "ui/tile_properties/tile_properties_panel.h"

namespace {

	void ShowAdvancedReplace(Editor& editor) {
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

	void ShowCollectionStudio(Editor& editor) {
		MapTab* tab = g_gui.GetCurrentMapTab();
		if (tab) {
			tab->ShowCollectionStudio(&editor);
		}
	}

	void ShowTileProperties(Editor& editor) {
		if (g_gui.tile_properties_panel) {
			wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(g_gui.tile_properties_panel);
			info.Show();
			g_gui.tile_properties_panel->UpdateFromEditor(&editor);
			g_gui.aui_manager->Update();
		}
	}

	void RunScript(int index, Editor& editor, const Position& position) {
		const auto& menuItems = g_luaScripts.getContextMenuItems();
		if (index < 0 || index >= static_cast<int>(menuItems.size())) {
			return;
		}

		try {
			const auto& item = menuItems[index];
			if (item.callback.valid()) {
				Tile* tile = editor.map.getTile(position);
				if (tile) {
					item.callback(tile);
				} else {
					item.callback(position);
				}
			}
		} catch (const sol::error& e) {
			DialogUtil::PopupDialog("Lua Error", wxString("Script execution error: ") + e.what(), wxOK | wxICON_ERROR);
		}
	}

}

void TileContextActions::Execute(int menu_id, const TileContextTarget& target) {
	Editor& editor = target.editor;
	switch (menu_id) {
		case MAP_POPUP_MENU_CUT:
			ClipboardHandler::cut(editor, target.floor);
			return;
		case MAP_POPUP_MENU_COPY:
			ClipboardHandler::copy(editor, target.floor);
			return;
		case MAP_POPUP_MENU_COPY_POSITION:
			ClipboardHandler::copyPosition(editor.selection);
			return;
		case MAP_POPUP_MENU_PASTE:
			ClipboardHandler::paste(editor, target.paste_position);
			return;
		case MAP_POPUP_MENU_DELETE:
			ClipboardHandler::doDelete(editor);
			return;
		case MAP_POPUP_MENU_COPY_SERVER_ID:
			ClipboardHandler::copyServerId(editor.selection);
			return;
		case MAP_POPUP_MENU_COPY_CLIENT_ID:
			ClipboardHandler::copyClientId(editor.selection);
			return;
		case MAP_POPUP_MENU_COPY_NAME:
			ClipboardHandler::copyName(editor.selection);
			return;
		case MAP_POPUP_MENU_BROWSE_TILE:
			PopupActionHandler::BrowseTile(editor, target.dialog_position.x, target.dialog_position.y);
			return;
		case MAP_POPUP_MENU_TILE_PROPERTIES:
			ShowTileProperties(editor);
			return;
		case MAP_POPUP_MENU_SWITCH_DOOR:
			PopupActionHandler::SwitchDoor(editor);
			return;
		case MAP_POPUP_MENU_ROTATE:
			PopupActionHandler::RotateItem(editor);
			return;
		case MAP_POPUP_MENU_GOTO:
			PopupActionHandler::GotoDestination(editor);
			return;
		case MAP_POPUP_MENU_SELECT_RAW_BRUSH:
			BrushSelector::SelectRAWBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_CREATURE_BRUSH:
			BrushSelector::SelectCreatureBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_SPAWN_BRUSH:
			BrushSelector::SelectSpawnBrush();
			return;
		case MAP_POPUP_MENU_SELECT_GROUND_BRUSH:
			BrushSelector::SelectGroundBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_DOODAD_BRUSH:
			BrushSelector::SelectDoodadBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_DOOR_BRUSH:
			BrushSelector::SelectDoorBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_WALL_BRUSH:
			BrushSelector::SelectWallBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_TABLE_BRUSH:
			BrushSelector::SelectTableBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_CARPET_BRUSH:
			BrushSelector::SelectCarpetBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_HOUSE_BRUSH:
			BrushSelector::SelectHouseBrush(editor, editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_COLLECTION_BRUSH:
			BrushSelector::SelectCollectionBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_PROPERTIES:
			PopupActionHandler::OpenProperties(editor);
			return;
		case MAP_POPUP_MENU_ADVANCED_REPLACE:
			ShowAdvancedReplace(editor);
			return;
		case MAP_POPUP_MENU_ADD_TO_COLLECTION:
			ShowCollectionStudio(editor);
			return;
		default:
			break;
	}

	if (menu_id >= MAP_POPUP_MENU_SCRIPT_FIRST && menu_id <= MAP_POPUP_MENU_SCRIPT_LAST) {
		RunScript(menu_id - MAP_POPUP_MENU_SCRIPT_FIRST, editor, target.script_position);
	}
}
