#ifndef RME_MAP_ITEM_COLLECTOR_H_
#define RME_MAP_ITEM_COLLECTOR_H_

#include "map/position.h"
#include <cstdint>
#include <vector>

class Editor;
class MapCanvas;
class Tile;

namespace rme::replace_tool {

class MapItemCollector {
public:
	/** Collects all map positions in the canvas viewport across all visible floors. */
	static std::vector<Position> CollectViewportPositions(MapCanvas* canvas);

	/** Collects unique item IDs (ground and items) present on tiles within the canvas viewport. */
	static std::vector<uint16_t> CollectViewportItemIds(Editor* editor, MapCanvas* canvas);

	/**
	 * Collects unique IDs of the entities that are actually selected (ground, items) on the
	 * editor selection's tiles. Item-level selections (wand, click, Ctrl-click) only stage the
	 * selected entities; a rectangle selects the whole tile, so every entity is staged.
	 */
	static std::vector<uint16_t> CollectSelectionItemIds(Editor* editor);

	/**
	 * Same rule as the editor overload, for callers that already hold the selected tiles.
	 * A wand or click selection marks single items, so collecting every entity of the touched
	 * tiles would offer unrelated ground and stack items to the replace rules.
	 */
	static std::vector<uint16_t> CollectSelectedItemIds(const std::vector<Tile*>& tiles);
};

} // namespace rme::replace_tool

#endif
