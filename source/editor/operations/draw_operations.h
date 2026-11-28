#ifndef RME_EDITOR_OPERATIONS_DRAW_OPERATIONS_H
#define RME_EDITOR_OPERATIONS_DRAW_OPERATIONS_H

#include "app/rme_forward_declarations.h"
#include "map/position.h"

#include <span>

// We don't want partial define of Editor, so forward declare or include?
// Ideally forward declare to avoid circular dependency if Editor includes this.
class Editor;
class Brush;
class DoodadBrush;

// One doodad composite to place: where, and which variation of the brush to roll.
struct DoodadPlacement {
	Position position;
	int variation = 0;
};

class DrawOperations {
public:
	// Draw with the GUI's current brush and the USE_AUTOMAGIC setting (the user's mouse tools).
	static void draw(Editor& editor, Position offset, bool alt, bool dodraw);
	static void draw(Editor& editor, const PositionVector& tilestodraw, bool alt, bool dodraw);
	static void draw(Editor& editor, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool dodraw);
	static void punchGroundToFloorBelow(Editor& editor, const PositionVector& tilestodraw, const PositionVector& tilestoborder);

	// Draw with an explicit brush and automagic, leaving the GUI's tool state untouched. For
	// callers that are not the user's mouse. Same history entries as the overloads above.
	static void drawWithBrush(Editor& editor, Brush& brush, const PositionVector& tilestodraw, bool alt);
	static void drawWithBrush(Editor& editor, Brush& brush, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool automagic);
	// Places one single-tile composite per placement, all in one history entry. Later placements
	// see the earlier ones, like smearing the brush; touched tiles are re-bordered at the end.
	static void drawDoodadsWithBrush(Editor& editor, DoodadBrush& brush, std::span<const DoodadPlacement> placements, bool alt);
};

#endif
