#ifndef RME_MAP_PROCESSOR_H
#define RME_MAP_PROCESSOR_H

#include <functional>

class Editor;

class MapProcessor {
public:
	static void borderizeMap(Editor& editor, std::function<void(int)> on_progress = nullptr);
	static void randomizeMap(Editor& editor, std::function<void(int)> on_progress = nullptr);
	static void clearInvalidHouseTiles(Editor& editor, std::function<void(int)> on_progress = nullptr);
	static void clearModifiedTileState(Editor& editor, std::function<void(int)> on_progress = nullptr);
};

#endif
