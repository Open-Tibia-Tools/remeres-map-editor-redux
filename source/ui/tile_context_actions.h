//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_TILE_CONTEXT_ACTIONS_H_
#define RME_UI_TILE_CONTEXT_ACTIONS_H_

#include "map/position.h"

#include <wx/gdicmn.h>

class Editor;

// Where a tile context menu was opened. Every action reads the tile and item it works on from
// editor.selection; the surface (map canvas or Tile Properties list) supplies the rest.
struct TileContextTarget {
	Editor& editor;
	// Floor handed to the copy buffer by Cut and Copy.
	int floor;
	// Position Paste places the copy buffer at.
	Position paste_position;
	// Position handed to Lua context-menu callbacks (as the tile on it, when one exists).
	Position script_position;
	// Where the Browse Field dialog opens.
	wxPoint dialog_position;
};

namespace TileContextActions {
	// Runs the MAP_POPUP_MENU_* command `menu_id` (including the Lua script range) for `target`.
	// Map changes go through the editor's action queue, so they are undoable.
	void Execute(int menu_id, const TileContextTarget& target);
}

#endif
