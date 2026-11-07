#ifndef RME_MAP_ITEM_COLLECTOR_H_
#define RME_MAP_ITEM_COLLECTOR_H_

#include "map/position.h"
#include <cstdint>
#include <vector>

class Editor;
class MapCanvas;

namespace rme::replace_tool {

class MapItemCollector {
public:
	/** Collects all map positions in the canvas viewport across all visible floors. */
	static std::vector<Position> CollectViewportPositions(MapCanvas* canvas);

	/** Collects unique item IDs (ground and items) present on tiles within the canvas viewport. */
	static std::vector<uint16_t> CollectViewportItemIds(Editor* editor, MapCanvas* canvas);

	/** Collects unique item IDs (ground and items) present on tiles in the current editor selection. */
	static std::vector<uint16_t> CollectSelectionItemIds(Editor* editor);
};

} // namespace rme::replace_tool

#endif
