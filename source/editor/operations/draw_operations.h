#ifndef RME_EDITOR_OPERATIONS_DRAW_OPERATIONS_H
#define RME_EDITOR_OPERATIONS_DRAW_OPERATIONS_H

#include "app/rme_forward_declarations.h"
#include "brushes/ground/border_update_policy.h"
#include "map/position.h"
// We don't want partial define of Editor, so forward declare or include?
// Ideally forward declare to avoid circular dependency if Editor includes this.
class Editor;

class DrawOperations {
public:
	static void draw(Editor& editor, Position offset, bool alt, bool dodraw);
	static void draw(Editor& editor, const PositionVector& tilestodraw, bool alt, bool dodraw);
	static void draw(Editor& editor, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool dodraw);
	static void punchGroundToFloorBelow(Editor& editor, const PositionVector& tilestodraw, const PositionVector& tilestoborder);

	// Border update policy for painting with `brush`, read once from the tool
	// options. Only ground brushes get a non-classic policy.
	[[nodiscard]] static BorderUpdate::Options groundBorderOptions(const Brush* brush);
};

#endif
