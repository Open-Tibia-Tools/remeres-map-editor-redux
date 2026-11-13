#ifndef RME_EDITOR_OPERATIONS_DRAW_OPERATIONS_H
#define RME_EDITOR_OPERATIONS_DRAW_OPERATIONS_H

#include "app/rme_forward_declarations.h"
#include "map/position.h"

class Editor;
class Brush;

class DrawOperations {
public:
	static void draw(Editor& editor, Brush* brush, Position offset, bool alt, bool dodraw, int brush_size = 1);
	static void draw(Editor& editor, Brush* brush, const PositionVector& tilestodraw, bool alt, bool dodraw, int brush_size = 1);
	static void draw(Editor& editor, Brush* brush, const PositionVector& tilestodraw, PositionVector& tilestoborder, bool alt, bool dodraw, int brush_size = 1);
	static void drawGroundHoleToFloorBelow(Editor& editor, Brush* brush, const PositionVector& tilestodraw, const PositionVector& tilestoborder);
	static void punchGroundToFloorBelow(Editor& editor, Brush* brush, const PositionVector& tilestodraw, const PositionVector& tilestoborder) {
		drawGroundHoleToFloorBelow(editor, brush, tilestodraw, tilestoborder);
	}
};

#endif
