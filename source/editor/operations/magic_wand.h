//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_OPERATIONS_MAGIC_WAND_H
#define RME_EDITOR_OPERATIONS_MAGIC_WAND_H

#include "editor/operations/magic_wand_plan.h"

class Editor;

// Magic wand selection: one Tab-held click selects everything that belongs to the entity under
// the cursor, collected with the user's SearchMode:
//  - Viewport: every match in the visible part of the floor, adjacent or not;
//  - Neighbour: only the part connected to the click, diagonally touching tiles included (a
//    ground takes its border pieces on the patch and its ring).
// Identity is the brush (or family/creature name) plus the clicked item's definition name, so
// grounds, borders and variants that share a name join even when their brushes differ; items
// without a definition name share the empty name and form one family.
namespace MagicWand {

	// Selects everything that belongs to the hit entity as one undoable ACTION_SELECT. Replaces the
	// current selection unless `add_to_selection`. Shows a cancellable progress bar for long
	// searches; cancelling leaves the selection untouched.
	void Select(Editor& editor, const WandHit& hit, const PositionRect& viewport, bool add_to_selection, SearchMode mode);

} // namespace MagicWand

#endif
