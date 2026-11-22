#ifndef RME_SELECTION_OPERATIONS_H
#define RME_SELECTION_OPERATIONS_H

#include "map/position.h"
#include <list>

class Editor;
class Tile;
class DoodadBrush;

struct DestroySelectionResult {
	int tile_count = 0;
	int item_count = 0;
	bool empty = false;
};

class SelectionOperations {
public:
	static void moveSelection(Editor& editor, Position offset);
	static DestroySelectionResult destroySelection(Editor& editor);
	static bool borderizeSelection(Editor& editor);
	static bool randomizeSelection(Editor& editor);
	static bool rotateSelection(Editor& editor, bool clockwise);

	// Helper functions used by Editor::drawInternal
	static void removeDuplicateWalls(Tile* buffer_tile, Tile* new_tile);
	static void doSurroundingBorders(DoodadBrush* doodad_brush, std::list<Position>& tilestoborder, Tile* buffer_tile, Tile* new_tile);
};

#endif
