//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_OPERATIONS_MAGIC_WAND_H
#define RME_EDITOR_OPERATIONS_MAGIC_WAND_H

#include "editor/operations/magic_wand_plan.h"

class Editor;

// Magic wand selection: one Tab-held click selects everything that belongs to the entity under the
// cursor, limited to the visible part of the floor.
//  - ground brush: four-connected patch plus its border pieces on the patch and its ring;
//  - wall / carpet / table brush: four-connected run;
//  - doodad and creature: every matching tile in the viewport;
//  - items no brush owns (raw, decorations, same-name variants): four-connected family run.
namespace MagicWand {

	// Selects everything that belongs to the hit entity as one undoable ACTION_SELECT. Replaces the
	// current selection unless `add_to_selection`. Shows a cancellable progress bar for long
	// searches; cancelling leaves the selection untouched.
	void Select(Editor& editor, const WandHit& hit, const PositionRect& viewport, bool add_to_selection);

} // namespace MagicWand

#endif
