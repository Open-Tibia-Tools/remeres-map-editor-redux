#include "app/main.h"

#include "ui/context_menu_presentation.h"

#include "ui/gui_ids.h"
#include "util/image_manager.h"

std::optional<ContextMenu::EntryPresentation> ContextMenu::PresentMenuEntry(int menu_id) {
	switch (menu_id) {
		case MAP_POPUP_MENU_CUT:
			return EntryPresentation { menu_id, "&Cut\tCTRL+X", "Cut out all selected items", ICON_CUT };
		case MAP_POPUP_MENU_COPY:
			return EntryPresentation { menu_id, "&Copy\tCTRL+C", "Copy all selected items", ICON_COPY };
		case MAP_POPUP_MENU_COPY_POSITION:
			return EntryPresentation { menu_id, "&Copy Position", "Copy the position as a lua table", ICON_LOCATION };
		case MAP_POPUP_MENU_PASTE:
			return EntryPresentation { menu_id, "&Paste\tCTRL+V", "Paste items in the copybuffer here", ICON_PASTE };
		case MAP_POPUP_MENU_DELETE:
			return EntryPresentation { menu_id, "&Delete\tDEL", "Removes all seleceted items", ICON_TRASH_CAN };
		case MAP_POPUP_MENU_ADVANCED_REPLACE:
			return EntryPresentation { menu_id, "Replace tiles...", "Open Advanced Replace Tool for selected items", ICON_WAND_MAGIC };
		case MAP_POPUP_MENU_ADD_TO_COLLECTION:
			return EntryPresentation { menu_id, "Add to tileset...", "Add selected items and brushes to a tileset", ICON_LAYER_GROUP };
		case MAP_POPUP_MENU_COPY_SERVER_ID:
			return EntryPresentation { menu_id, "Copy Item Server Id", "Copy the server id of this item", ICON_SERVER };
		case MAP_POPUP_MENU_COPY_CLIENT_ID:
			return EntryPresentation { menu_id, "Copy Item Client Id", "Copy the client id of this item", ICON_DESKTOP };
		case MAP_POPUP_MENU_COPY_NAME:
			return EntryPresentation { menu_id, "Copy Item Name", "Copy the name of this item", ICON_TAG };
		case MAP_POPUP_MENU_ROTATE:
			return EntryPresentation { menu_id, "&Rotate item", "Rotate this item", ICON_ROTATE };
		case MAP_POPUP_MENU_GOTO:
			return EntryPresentation { menu_id, "&Go To Destination", "Go to the destination of this teleport", ICON_ARROW_RIGHT_TO_BRACKET };
		case MAP_POPUP_MENU_SELECT_CREATURE_BRUSH:
			return EntryPresentation { menu_id, "Select Creature", "Uses the current creature as a creature brush", ICON_DRAGON };
		case MAP_POPUP_MENU_SELECT_SPAWN_BRUSH:
			return EntryPresentation { menu_id, "Select Spawn", "Select the spawn brush", ICON_FIRE };
		case MAP_POPUP_MENU_SELECT_RAW_BRUSH:
			return EntryPresentation { menu_id, "Select RAW", "Uses the top item as a RAW brush", ICON_CUBE };
		case MAP_POPUP_MENU_SELECT_WALL_BRUSH:
			return EntryPresentation { menu_id, "Select Wallbrush", "Uses the current item as a wallbrush", ICON_DUNGEON };
		case MAP_POPUP_MENU_SELECT_CARPET_BRUSH:
			return EntryPresentation { menu_id, "Select Carpetbrush", "Uses the current item as a carpetbrush", ICON_RUG };
		case MAP_POPUP_MENU_SELECT_TABLE_BRUSH:
			return EntryPresentation { menu_id, "Select Tablebrush", "Uses the current item as a tablebrush", ICON_TABLE };
		case MAP_POPUP_MENU_SELECT_DOODAD_BRUSH:
			return EntryPresentation { menu_id, "Select Doodadbrush", "Use this doodad brush", ICON_TREE };
		case MAP_POPUP_MENU_SELECT_DOOR_BRUSH:
			return EntryPresentation { menu_id, "Select Doorbrush", "Use this door brush", ICON_DOOR_CLOSED };
		case MAP_POPUP_MENU_SELECT_GROUND_BRUSH:
			return EntryPresentation { menu_id, "Select Groundbrush", "Uses the current item as a groundbrush", ICON_LAYER_GROUP };
		case MAP_POPUP_MENU_SELECT_COLLECTION_BRUSH:
			return EntryPresentation { menu_id, "Select Collection", "Use this collection", ICON_LAYER_GROUP };
		case MAP_POPUP_MENU_SELECT_HOUSE_BRUSH:
			return EntryPresentation { menu_id, "Select House", "Draw with the house on this tile.", ICON_HOUSE };
		case MAP_POPUP_MENU_PROPERTIES:
			return EntryPresentation { menu_id, "&Properties", "Properties for the current object", ICON_GEAR };
		case MAP_POPUP_MENU_BROWSE_TILE:
			return EntryPresentation { menu_id, "Browse Field", "Navigate from tile items", ICON_SEARCH };
		case MAP_POPUP_MENU_TILE_PROPERTIES:
			return EntryPresentation { menu_id, "Tile Properties", "Show tile properties panel", ICON_LIST };
		default:
			return std::nullopt;
	}
}
