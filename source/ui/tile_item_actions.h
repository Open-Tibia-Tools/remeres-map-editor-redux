#ifndef RME_UI_TILE_ITEM_ACTIONS_H_
#define RME_UI_TILE_ITEM_ACTIONS_H_

class Editor;
class Tile;

namespace TileItemActions {

	// Removes the item list row `row` (ground row or stacked item) from `tile` through the editor's
	// action queue, so the change is undoable. Returns true when a change was committed.
	bool DeleteRow(Editor& editor, Tile* tile, int row);

} // namespace TileItemActions

#endif
