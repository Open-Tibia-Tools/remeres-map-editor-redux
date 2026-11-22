#include "app/main.h"

#include "ui/item_context_actions.h"

#include "editor/editor.h"
#include "game/item.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/tile_stack_rows.h"
#include "rendering/ui/brush_selector.h"
#include "rendering/ui/clipboard_handler.h"
#include "rendering/ui/popup_action_handler.h"
#include "ui/gui_ids.h"
#include "ui/tile_item_actions.h"

void ItemContextActions::Execute(int menu_id, Editor& editor, const Position& position, int row) {
	Tile* tile = editor.map.getTile(position);
	if (!tile) {
		return;
	}

	const auto resolved = TileStackRows::Resolve(row, tile->ground != nullptr, tile->items.size());
	if (!resolved) {
		return;
	}
	Item* item = resolved->is_ground ? tile->ground.get() : tile->items[resolved->item_index].get();
	if (!item) {
		return;
	}

	// Deleting the row is the only command that must not go through the selection: the list row,
	// not the editor selection, decides what disappears.
	if (menu_id == MAP_POPUP_MENU_DELETE) {
		TileItemActions::DeleteRow(editor, tile, row);
		return;
	}

	// Every other command works on exactly this row, without the BORDER_IS_GROUND expansion that
	// would drag the ground and the border layer into the selection.
	editor.selection.setSingleItem(tile, item);

	switch (menu_id) {
		case MAP_POPUP_MENU_COPY:
			ClipboardHandler::copy(editor, position.z);
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
		case MAP_POPUP_MENU_ROTATE:
			PopupActionHandler::RotateItem(editor);
			return;
		case MAP_POPUP_MENU_GOTO:
			PopupActionHandler::GotoDestination(editor);
			return;
		case MAP_POPUP_MENU_SWITCH_DOOR:
			PopupActionHandler::SwitchDoor(editor);
			return;
		case MAP_POPUP_MENU_SELECT_RAW_BRUSH:
			BrushSelector::SelectRAWBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_DOODAD_BRUSH:
			BrushSelector::SelectDoodadBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_DOOR_BRUSH:
			BrushSelector::SelectDoorBrush(editor.selection);
			return;
		case MAP_POPUP_MENU_SELECT_WALL_BRUSH:
			BrushSelector::SelectWallBrush(*item);
			return;
		case MAP_POPUP_MENU_SELECT_CARPET_BRUSH:
			BrushSelector::SelectCarpetBrush(*item);
			return;
		case MAP_POPUP_MENU_SELECT_TABLE_BRUSH:
			BrushSelector::SelectTableBrush(*item);
			return;
		case MAP_POPUP_MENU_SELECT_GROUND_BRUSH:
			BrushSelector::SelectGroundBrush(*item);
			return;
		case MAP_POPUP_MENU_SELECT_COLLECTION_BRUSH:
			BrushSelector::SelectCollectionBrush(*item);
			return;
		default:
			return;
	}
}
