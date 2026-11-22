//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_OPERATIONS_MAGIC_WAND_H
#define RME_EDITOR_OPERATIONS_MAGIC_WAND_H

class Editor;
class Position;

// Magic wand selection: one click selects everything that belongs to the brush under the cursor.
// The topmost doodad item wins, then the topmost wall item, then the ground.
//  - Ground: the four-connected patch of that ground on the floor, plus its border pieces
//    on the patch and on the eight-neighbour ring around it.
//  - Wall:   the four-connected run of tiles holding a wall of that brush.
//  - Doodad: every item of that doodad brush on the floor (doodads are scattered, so not connected).
namespace MagicWand {

	// Selects the target under `origin` as one undoable ACTION_SELECT. Replaces the current
	// selection unless `add_to_selection`. Shows a cancellable progress bar for long searches;
	// cancelling leaves the selection untouched.
	void Select(Editor& editor, const Position& origin, bool add_to_selection);

	// The tool is a global toggle (Config::MAGIC_WAND_SELECTION), like the selection method.
	[[nodiscard]] bool IsToolEnabled();
	// Stores the toggle, switches the current map to selection mode when enabling,
	// refreshes the menu check and reports the state in the status bar.
	void SetToolEnabled(bool enabled);

} // namespace MagicWand

#endif
