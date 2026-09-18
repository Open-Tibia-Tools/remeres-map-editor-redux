#ifndef RME_EDITOR_OPERATIONS_DRAW_OPERATIONS_H
#define RME_EDITOR_OPERATIONS_DRAW_OPERATIONS_H

#include "app/rme_forward_declarations.h"
#include "map/position.h"
// We don't want partial define of Editor, so forward declare or include?
// Ideally forward declare to avoid circular dependency if Editor includes this.
class Editor;

class DrawOperations {
public:
	static void draw(Editor& editor, Brush* brush, Position offset, bool alt, bool dodraw, int brush_size = 1);
	static void draw(Editor& editor, Brush* brush, const PositionVector& tilestodraw, bool alt, bool dodraw, int brush_size = 1);
	static void draw(Editor& editor, Brush* brush, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool dodraw, int brush_size = 1);
};

#endif
