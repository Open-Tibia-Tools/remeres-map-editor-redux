//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"

#include "ui/map_popup_menu.h"

#include <algorithm>
#include <string_view>

#include "editor/editor.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/tile_context_menu_model.h"
#include "brushes/brush.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "map/map.h"
#include "map/tile.h"
#include "util/image_manager.h"

#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/table/table_brush.h"
#include "lua/lua_script_manager.h"

namespace {

	struct EntryPresentation {
		int id = wxID_NONE;
		const char* label = "";
		const char* help = "";
		std::string_view icon;
	};

	TileContextMenu::SelectedItemFacts CollectSelectedItemFacts(Item* item) {
		const Teleport* teleport = dynamic_cast<const Teleport*>(item);
		const Brush* doodad = item->getDoodadBrush();
		return TileContextMenu::SelectedItemFacts {
			.is_rotatable = item->isRoteable(),
			.is_teleport = teleport != nullptr,
			.teleport_has_destination = teleport && teleport->hasDestination(),
			.is_door = item->isDoor(),
			.is_open_door = item->isOpen(),
			.is_brush_door = item->isBrushDoor(),
			.has_door_brush = item->getDoorBrush() != nullptr,
			.has_visible_doodad_brush = doodad && doodad->visibleInPalette(),
			.has_collection_brush = item->hasCollectionBrush(),
		};
	}

	void CollectStackFacts(const Tile* tile, TileContextMenu::Facts& facts) {
		const auto note_brush = [&facts](const Brush* brush, bool& has_kind) {
			if (brush && brush->visibleInPalette()) {
				has_kind = true;
				facts.stack_has_collection = facts.stack_has_collection || brush->hasCollection();
			}
		};

		for (const auto& item : tile->items) {
			if (item->isWall()) {
				note_brush(item->getWallBrush(), facts.has_wall_brush);
			}
			if (item->isTable()) {
				note_brush(item->getTableBrush(), facts.has_table_brush);
			}
			if (item->isCarpet()) {
				note_brush(item->getCarpetBrush(), facts.has_carpet_brush);
			}
			if (const Brush* doodad = item->getDoodadBrush()) {
				facts.stack_has_collection = facts.stack_has_collection || doodad->hasCollection();
			}
			if (item->isSelected()) {
				facts.has_selected_stack_item = true;
			}
		}
	}

	TileContextMenu::Facts CollectFacts(Editor& editor) {
		TileContextMenu::Facts facts;
		facts.selected_tile_count = editor.selection.size();
		facts.can_paste = editor.copybuffer.canPaste();

		if (facts.selected_tile_count != 1) {
			return facts;
		}

		Tile* tile = editor.selection.getSelectedTile();
		if (!tile) {
			return facts;
		}

		const GroundBrush* ground_brush = tile->getGroundBrush();
		facts.has_ground = tile->hasGround();
		facts.has_visible_ground_brush = facts.has_ground && ground_brush && ground_brush->visibleInPalette();
		facts.ground_brush_has_collection = ground_brush && ground_brush->hasCollection();
		facts.has_creature = tile->creature != nullptr;
		facts.has_spawn = tile->spawn != nullptr;
		facts.is_house_tile = tile->isHouseTile();
		CollectStackFacts(tile, facts);

		const ItemVector selected_items = TileOperations::getSelectedItems(tile);
		if (selected_items.size() == 1) {
			facts.selected_item = CollectSelectedItemFacts(selected_items.back());
		}

		if (g_luaScripts.isInitialized()) {
			constexpr std::size_t script_slots = MAP_POPUP_MENU_SCRIPT_LAST - MAP_POPUP_MENU_SCRIPT_FIRST + 1;
			facts.script_count = std::min(g_luaScripts.getContextMenuItems().size(), script_slots);
		}
		return facts;
	}

	EntryPresentation Present(TileContextMenu::Command command) {
		using TileContextMenu::Command;
		switch (command) {
			case Command::Cut:
				return { MAP_POPUP_MENU_CUT, "&Cut\tCTRL+X", "Cut out all selected items", ICON_CUT };
			case Command::Copy:
				return { MAP_POPUP_MENU_COPY, "&Copy\tCTRL+C", "Copy all selected items", ICON_COPY };
			case Command::CopyPosition:
				return { MAP_POPUP_MENU_COPY_POSITION, "&Copy Position", "Copy the position as a lua table", ICON_LOCATION };
			case Command::Paste:
				return { MAP_POPUP_MENU_PASTE, "&Paste\tCTRL+V", "Paste items in the copybuffer here", ICON_PASTE };
			case Command::Delete:
				return { MAP_POPUP_MENU_DELETE, "&Delete\tDEL", "Removes all seleceted items", ICON_TRASH_CAN };
			case Command::AdvancedReplace:
				return { MAP_POPUP_MENU_ADVANCED_REPLACE, "Replace tiles...", "Open Advanced Replace Tool for selected items", ICON_WAND_MAGIC };
			case Command::AddToCollection:
				return { MAP_POPUP_MENU_ADD_TO_COLLECTION, "Add to tileset...", "Add selected items and brushes to a tileset", ICON_LAYER_GROUP };
			case Command::CopyServerId:
				return { MAP_POPUP_MENU_COPY_SERVER_ID, "Copy Item Server Id", "Copy the server id of this item", ICON_SERVER };
			case Command::CopyClientId:
				return { MAP_POPUP_MENU_COPY_CLIENT_ID, "Copy Item Client Id", "Copy the client id of this item", ICON_DESKTOP };
			case Command::CopyName:
				return { MAP_POPUP_MENU_COPY_NAME, "Copy Item Name", "Copy the name of this item", ICON_TAG };
			case Command::Rotate:
				return { MAP_POPUP_MENU_ROTATE, "&Rotate item", "Rotate this item", ICON_ROTATE };
			case Command::GotoDestination:
				return { MAP_POPUP_MENU_GOTO, "&Go To Destination", "Go to the destination of this teleport", ICON_ARROW_RIGHT_TO_BRACKET };
			case Command::OpenDoor:
				return { MAP_POPUP_MENU_SWITCH_DOOR, "&Open door", "Open this door", ICON_DOOR_OPEN };
			case Command::CloseDoor:
				return { MAP_POPUP_MENU_SWITCH_DOOR, "&Close door", "Close this door", ICON_DOOR_CLOSED };
			case Command::SelectCreature:
				return { MAP_POPUP_MENU_SELECT_CREATURE_BRUSH, "Select Creature", "Uses the current creature as a creature brush", ICON_DRAGON };
			case Command::SelectSpawn:
				return { MAP_POPUP_MENU_SELECT_SPAWN_BRUSH, "Select Spawn", "Select the spawn brush", ICON_FIRE };
			case Command::SelectRaw:
				return { MAP_POPUP_MENU_SELECT_RAW_BRUSH, "Select RAW", "Uses the top item as a RAW brush", ICON_CUBE };
			case Command::SelectWall:
				return { MAP_POPUP_MENU_SELECT_WALL_BRUSH, "Select Wallbrush", "Uses the current item as a wallbrush", ICON_DUNGEON };
			case Command::SelectCarpet:
				return { MAP_POPUP_MENU_SELECT_CARPET_BRUSH, "Select Carpetbrush", "Uses the current item as a carpetbrush", ICON_RUG };
			case Command::SelectTable:
				return { MAP_POPUP_MENU_SELECT_TABLE_BRUSH, "Select Tablebrush", "Uses the current item as a tablebrush", ICON_TABLE };
			case Command::SelectDoodad:
				return { MAP_POPUP_MENU_SELECT_DOODAD_BRUSH, "Select Doodadbrush", "Use this doodad brush", ICON_TREE };
			case Command::SelectDoor:
				return { MAP_POPUP_MENU_SELECT_DOOR_BRUSH, "Select Doorbrush", "Use this door brush", ICON_DOOR_CLOSED };
			case Command::SelectGround:
				return { MAP_POPUP_MENU_SELECT_GROUND_BRUSH, "Select Groundbrush", "Uses the current item as a groundbrush", ICON_LAYER_GROUP };
			case Command::SelectCollection:
				return { MAP_POPUP_MENU_SELECT_COLLECTION_BRUSH, "Select Collection", "Use this collection", ICON_LAYER_GROUP };
			case Command::SelectHouse:
				return { MAP_POPUP_MENU_SELECT_HOUSE_BRUSH, "Select House", "Draw with the house on this tile.", ICON_HOUSE };
			case Command::Properties:
				return { MAP_POPUP_MENU_PROPERTIES, "&Properties", "Properties for the current object", ICON_GEAR };
			case Command::BrowseField:
				return { MAP_POPUP_MENU_BROWSE_TILE, "Browse Field", "Navigate from tile items", ICON_SEARCH };
			case Command::TileProperties:
				return { MAP_POPUP_MENU_TILE_PROPERTIES, "Tile Properties", "Show tile properties panel", ICON_LIST };
			case Command::Separator:
			case Command::Script:
				break;
		}
		return {};
	}

}

MapPopupMenu::MapPopupMenu(Editor& editor) :
	wxMenu(""), editor(editor) {
	////
}

MapPopupMenu::~MapPopupMenu() {
	////
}

void MapPopupMenu::Update() {
	// Clear the menu of all items
	while (GetMenuItemCount() != 0) {
		wxMenuItem* m_item = FindItemByPosition(0);
		// If you add a submenu, this won't delete it.
		Delete(m_item);
	}

	for (const TileContextMenu::Entry& entry : TileContextMenu::Build(CollectFacts(editor))) {
		if (entry.command == TileContextMenu::Command::Separator) {
			AppendSeparator();
			continue;
		}

		if (entry.command == TileContextMenu::Command::Script) {
			const auto& script_items = g_luaScripts.getContextMenuItems();
			Append(MAP_POPUP_MENU_SCRIPT_FIRST + static_cast<int>(entry.script_index), wxString::FromUTF8(script_items[entry.script_index].label));
			continue;
		}

		const EntryPresentation presentation = Present(entry.command);
		wxMenuItem* menu_item = Append(presentation.id, presentation.label, presentation.help);
		menu_item->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(presentation.icon));
		menu_item->Enable(entry.enabled);
	}
}
