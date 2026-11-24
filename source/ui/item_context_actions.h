#ifndef RME_UI_ITEM_CONTEXT_ACTIONS_H_
#define RME_UI_ITEM_CONTEXT_ACTIONS_H_

#include "map/position.h"

class Editor;

namespace ItemContextActions {

	// Runs the item-scoped MAP_POPUP_MENU_* command `menu_id` for list row `row` of the tile at
	// `position`. The tile and the row are resolved again so a command never acts on a stale
	// pointer. Map changes go through the editor's action queue.
	void Execute(int menu_id, Editor& editor, const Position& position, int row);

} // namespace ItemContextActions

#endif
