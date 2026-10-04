//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"

#include "ui/map_popup_menu.h"

#include "editor/editor.h"
#include "ui/gui.h"
#include "brushes/brush.h"
#include "game/sprites.h"
#include "map/map.h"
#include "map/tile.h"
#include "util/image_manager.h"
#include "ui/browse_tile_window.h"

#include "brushes/doodad/doodad_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/table/table_brush.h"
#include "lua/lua_script_manager.h"

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

	bool anything_selected = editor.selection.size() != 0;

	wxMenuItem* cutItem = Append(MAP_POPUP_MENU_CUT, "&Cut\tCTRL+X", "Cut out all selected items");
	cutItem->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CUT, wxSize(16, 16)));
	cutItem->Enable(anything_selected);

	wxMenuItem* copyItem = Append(MAP_POPUP_MENU_COPY, "&Copy\tCTRL+C", "Copy all selected items");
	copyItem->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_COPY, wxSize(16, 16)));
	copyItem->Enable(anything_selected);

	wxMenuItem* copyPositionItem = Append(MAP_POPUP_MENU_COPY_POSITION, "&Copy Position", "Copy the position as a lua table");
	copyPositionItem->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LOCATION, wxSize(16, 16)));
	copyPositionItem->Enable(anything_selected);

	wxMenuItem* pasteItem = Append(MAP_POPUP_MENU_PASTE, "&Paste\tCTRL+V", "Paste items in the copybuffer here");
	pasteItem->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PASTE, wxSize(16, 16)));
	pasteItem->Enable(editor.copybuffer.canPaste());

	wxMenuItem* deleteItem = Append(MAP_POPUP_MENU_DELETE, "&Delete\tDEL", "Removes all seleceted items");
	deleteItem->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TRASH_CAN, wxSize(16, 16)));
	deleteItem->Enable(anything_selected);

	if (anything_selected) {
		Append(MAP_POPUP_MENU_ADVANCED_REPLACE, "Replace tiles...", "Open Advanced Replace Tool for selected items")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_WAND_MAGIC, wxSize(16, 16)));
	}

	if (anything_selected) {
		if (editor.selection.size() == 1) {
			Tile* tile = editor.selection.getSelectedTile();
			ItemVector selected_items = TileOperations::getSelectedItems(tile);

			bool hasWall = false;
			bool hasCarpet = false;
			bool hasTable = false;
			bool hasCollection = false;
			Item* topItem = nullptr;
			Item* topSelectedItem = (selected_items.size() == 1 ? selected_items.back() : nullptr);
			Creature* topCreature = tile->creature.get();
			Spawn* topSpawn = tile->spawn.get();

			for (const auto& item : tile->items) {
				if (item->isWall()) {
					Brush* wb = item->getWallBrush();
					if (wb && wb->visibleInPalette()) {
						hasWall = true;
						hasCollection = hasCollection || wb->hasCollection();
					}
				}
				if (item->isTable()) {
					Brush* tb = item->getTableBrush();
					if (tb && tb->visibleInPalette()) {
						hasTable = true;
						hasCollection = hasCollection || tb->hasCollection();
					}
				}
				if (item->isCarpet()) {
					Brush* cb = item->getCarpetBrush();
					if (cb && cb->visibleInPalette()) {
						hasCarpet = true;
						hasCollection = hasCollection || cb->hasCollection();
					}
				}
				if (Brush* db = item->getDoodadBrush()) {
					hasCollection = hasCollection || db->hasCollection();
				}
				if (item->isSelected()) {
					topItem = item.get();
				}
			}
			if (!topItem) {
				topItem = tile->ground.get();
			}

			AppendSeparator();

			if (topSelectedItem) {
				Append(MAP_POPUP_MENU_COPY_SERVER_ID, "Copy Item Server Id", "Copy the server id of this item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_SERVER, wxSize(16, 16)));
				Append(MAP_POPUP_MENU_COPY_CLIENT_ID, "Copy Item Client Id", "Copy the client id of this item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DESKTOP, wxSize(16, 16)));
				Append(MAP_POPUP_MENU_COPY_NAME, "Copy Item Name", "Copy the name of this item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TAG, wxSize(16, 16)));
				AppendSeparator();
			}

			if (topSelectedItem || topCreature || topItem) {
				Teleport* teleport = dynamic_cast<Teleport*>(topSelectedItem);
				if (topSelectedItem && (topSelectedItem->isBrushDoor() || topSelectedItem->isRoteable() || teleport)) {

					if (topSelectedItem->isRoteable()) {
						Append(MAP_POPUP_MENU_ROTATE, "&Rotate item", "Rotate this item")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_ROTATE, wxSize(16, 16)));
					}

					if (teleport && teleport->hasDestination()) {
						Append(MAP_POPUP_MENU_GOTO, "&Go To Destination", "Go to the destination of this teleport")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_ARROW_RIGHT_TO_BRACKET, wxSize(16, 16)));
					}

					if (topSelectedItem->isDoor()) {
						if (topSelectedItem->isOpen()) {
							Append(MAP_POPUP_MENU_SWITCH_DOOR, "&Close door", "Close this door")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DOOR_CLOSED, wxSize(16, 16)));
						} else {
							Append(MAP_POPUP_MENU_SWITCH_DOOR, "&Open door", "Open this door")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DOOR_OPEN, wxSize(16, 16)));
						}
						AppendSeparator();
					}
				}

				if (topCreature) {
					Append(MAP_POPUP_MENU_SELECT_CREATURE_BRUSH, "Select Creature", "Uses the current creature as a creature brush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DRAGON, wxSize(16, 16)));
				}

				if (topSpawn) {
					Append(MAP_POPUP_MENU_SELECT_SPAWN_BRUSH, "Select Spawn", "Select the spawn brush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_FIRE, wxSize(16, 16)));
				}

				Append(MAP_POPUP_MENU_SELECT_RAW_BRUSH, "Select RAW", "Uses the top item as a RAW brush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CUBE, wxSize(16, 16)));

				if (hasWall) {
					Append(MAP_POPUP_MENU_SELECT_WALL_BRUSH, "Select Wallbrush", "Uses the current item as a wallbrush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DUNGEON, wxSize(16, 16)));
				}

				if (hasCarpet) {
					Append(MAP_POPUP_MENU_SELECT_CARPET_BRUSH, "Select Carpetbrush", "Uses the current item as a carpetbrush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_RUG, wxSize(16, 16)));
				}

				if (hasTable) {
					Append(MAP_POPUP_MENU_SELECT_TABLE_BRUSH, "Select Tablebrush", "Uses the current item as a tablebrush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TABLE, wxSize(16, 16)));
				}

				if (topSelectedItem && topSelectedItem->getDoodadBrush() && topSelectedItem->getDoodadBrush()->visibleInPalette()) {
					Append(MAP_POPUP_MENU_SELECT_DOODAD_BRUSH, "Select Doodadbrush", "Use this doodad brush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TREE, wxSize(16, 16)));
				}

				if (topSelectedItem && topSelectedItem->isBrushDoor() && topSelectedItem->getDoorBrush()) {
					Append(MAP_POPUP_MENU_SELECT_DOOR_BRUSH, "Select Doorbrush", "Use this door brush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DOOR_CLOSED, wxSize(16, 16)));
				}

				if (tile->hasGround() && tile->getGroundBrush() && tile->getGroundBrush()->visibleInPalette()) {
					Append(MAP_POPUP_MENU_SELECT_GROUND_BRUSH, "Select Groundbrush", "Uses the current item as a groundbrush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LAYER_GROUP, wxSize(16, 16)));
				}

				if (hasCollection || topSelectedItem && topSelectedItem->hasCollectionBrush() || tile->getGroundBrush() && tile->getGroundBrush()->hasCollection()) {
					Append(MAP_POPUP_MENU_SELECT_COLLECTION_BRUSH, "Select Collection", "Use this collection")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LAYER_GROUP, wxSize(16, 16)));
				}

				if (tile->isHouseTile()) {
					Append(MAP_POPUP_MENU_SELECT_HOUSE_BRUSH, "Select House", "Draw with the house on this tile.")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_HOUSE, wxSize(16, 16)));
				}

				AppendSeparator();
				Append(MAP_POPUP_MENU_PROPERTIES, "&Properties", "Properties for the current object")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_GEAR, wxSize(16, 16)));
			} else {

				if (topCreature) {
					Append(MAP_POPUP_MENU_SELECT_CREATURE_BRUSH, "Select Creature", "Uses the current creature as a creature brush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DRAGON, wxSize(16, 16)));
				}

				if (topSpawn) {
					Append(MAP_POPUP_MENU_SELECT_SPAWN_BRUSH, "Select Spawn", "Select the spawn brush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_FIRE, wxSize(16, 16)));
				}

				Append(MAP_POPUP_MENU_SELECT_RAW_BRUSH, "Select RAW", "Uses the top item as a RAW brush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CUBE, wxSize(16, 16)));
				if (hasWall) {
					Append(MAP_POPUP_MENU_SELECT_WALL_BRUSH, "Select Wallbrush", "Uses the current item as a wallbrush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DUNGEON, wxSize(16, 16)));
				}
				if (tile->hasGround() && tile->getGroundBrush() && tile->getGroundBrush()->visibleInPalette()) {
					Append(MAP_POPUP_MENU_SELECT_GROUND_BRUSH, "Select Groundbrush", "Uses the current tile as a groundbrush")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LAYER_GROUP, wxSize(16, 16)));
				}

				if (hasCollection || tile->getGroundBrush() && tile->getGroundBrush()->hasCollection()) {
					Append(MAP_POPUP_MENU_SELECT_COLLECTION_BRUSH, "Select Collection", "Use this collection")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LAYER_GROUP, wxSize(16, 16)));
				}

				if (tile->isHouseTile()) {
					Append(MAP_POPUP_MENU_SELECT_HOUSE_BRUSH, "Select House", "Draw with the house on this tile.")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_HOUSE, wxSize(16, 16)));
				}

				if (tile->hasGround() || topCreature || topSpawn) {
					AppendSeparator();
					Append(MAP_POPUP_MENU_PROPERTIES, "&Properties", "Properties for the current object")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_GEAR, wxSize(16, 16)));
				}
			}

			AppendSeparator();

			wxMenuItem* browseTile = Append(MAP_POPUP_MENU_BROWSE_TILE, "Browse Field", "Navigate from tile items");
			browseTile->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_SEARCH, wxSize(16, 16)));
			browseTile->Enable(anything_selected);

			wxMenuItem* tileProps = Append(MAP_POPUP_MENU_TILE_PROPERTIES, "Tile Properties", "Show tile properties panel");
			tileProps->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LIST, wxSize(16, 16)));
			tileProps->Enable(anything_selected);

			// Add Lua Context Menu Items
			if (g_luaScripts.isInitialized()) {
				const auto& menuItems = g_luaScripts.getContextMenuItems();
				if (!menuItems.empty()) {
					AppendSeparator();

					for (size_t i = 0; i < menuItems.size(); ++i) {
						int id = MAP_POPUP_MENU_SCRIPT_FIRST + i;
						if (id > MAP_POPUP_MENU_SCRIPT_LAST) break;

						Append(id, wxString::FromUTF8(menuItems[i].label));
					}
				}
			}
		}
	}
}
