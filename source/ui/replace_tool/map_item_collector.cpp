#include "app/main.h"
#include "ui/replace_tool/map_item_collector.h"
#include "editor/editor.h"
#include "editor/selection.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "rendering/ui/map_display.h"
#include "rendering/core/floor_visibility_mode.h"
#include "app/settings.h"

#include <algorithm>
#include <set>

namespace rme::replace_tool {

std::vector<Position> MapItemCollector::CollectViewportPositions(MapCanvas* canvas) {
	if (!canvas) {
		return {};
	}

	const wxSize canvasSize = canvas->GetClientSize();
	const int cameraFloor = canvas->GetFloor();
	const bool showAllFloors = g_settings.getBoolean(Config::SHOW_ALL_FLOORS);
	const auto visibilityMode = SanitizeFloorVisibilityMode(g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE));
	const auto floorRange = BuildFloorVisibilityRange(cameraFloor, showAllFloors, visibilityMode);

	std::vector<Position> positions;
	for (int z = floorRange.start_floor; z >= floorRange.end_floor; --z) {
		int startX = 0, startY = 0, endX = 0, endY = 0;
		canvas->ScreenToMap(0, 0, z, &startX, &startY);
		canvas->ScreenToMap(canvasSize.x, canvasSize.y, z, &endX, &endY);

		if (startX > endX) {
			std::swap(startX, endX);
		}
		if (startY > endY) {
			std::swap(startY, endY);
		}

		for (int x = startX; x <= endX; ++x) {
			for (int y = startY; y <= endY; ++y) {
				positions.emplace_back(x, y, z);
			}
		}
	}
	return positions;
}

std::vector<uint16_t> MapItemCollector::CollectViewportItemIds(Editor* editor, MapCanvas* canvas) {
	if (!editor || !canvas) {
		return {};
	}

	const std::vector<Position> positions = CollectViewportPositions(canvas);
	std::set<uint16_t> uniqueIds;

	for (const Position& pos : positions) {
		Tile* tile = editor->map.getTile(pos.x, pos.y, pos.z);
		if (tile) {
			if (tile->ground) {
				uniqueIds.insert(tile->ground->getID());
			}
			for (const auto& item : tile->items) {
				uniqueIds.insert(item->getID());
			}
		}
	}

	return { uniqueIds.begin(), uniqueIds.end() };
}

std::vector<uint16_t> MapItemCollector::CollectSelectionItemIds(Editor* editor) {
	if (!editor) {
		return {};
	}

	std::set<uint16_t> uniqueIds;
	for (Tile* tile : editor->selection.getTiles()) {
		if (tile) {
			if (tile->ground) {
				uniqueIds.insert(tile->ground->getID());
			}
			for (const auto& item : tile->items) {
				uniqueIds.insert(item->getID());
			}
		}
	}

	return { uniqueIds.begin(), uniqueIds.end() };
}

} // namespace rme::replace_tool
