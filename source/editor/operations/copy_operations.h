//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_COPY_OPERATIONS_H
#define RME_COPY_OPERATIONS_H

#include "map/position.h"

class Editor;
class CopyBuffer;

struct CopyResult {
	int tile_count = 0;
	int item_count = 0;
	bool empty = false;
};

class CopyOperations {
public:
	static CopyResult copy(Editor& editor, CopyBuffer& buffer, int floor);
	static CopyResult cut(Editor& editor, CopyBuffer& buffer, int floor);
	static void paste(Editor& editor, CopyBuffer& buffer, const Position& toPosition);
};

#endif
