//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/definitions.h"

#include "map/operations/map_processor.h"
#include "editor/editor.h"
#include "map/map.h"
#include "map/tile_operations.h"
#include "brushes/ground/ground_brush.h"

void MapProcessor::borderizeMap(Editor& editor, std::function<void(int)> on_progress) {
	uint64_t tiles_done = 0;
	const auto total_tiles = editor.map.getTileCount();
	for (TileLocation& tileLocation : editor.map) {
		if (on_progress && tiles_done % 4096 == 0 && total_tiles > 0) {
			on_progress(static_cast<int32_t>(tiles_done / double(total_tiles) * 100.0));
		}

		Tile* tile = tileLocation.get();
		ASSERT(tile);

		TileOperations::borderize(tile, &editor.map);
		++tiles_done;
	}
}

void MapProcessor::randomizeMap(Editor& editor, std::function<void(int)> on_progress) {
	uint64_t tiles_done = 0;
	const auto total_tiles = editor.map.getTileCount();
	for (TileLocation& tileLocation : editor.map) {
		if (on_progress && tiles_done % 4096 == 0 && total_tiles > 0) {
			on_progress(static_cast<int32_t>(tiles_done / double(total_tiles) * 100.0));
		}

		Tile* tile = tileLocation.get();
		ASSERT(tile);

		GroundBrush* groundBrush = tile->getGroundBrush();
		if (groundBrush) {
			Item* oldGround = tile->ground.get();

			uint16_t actionId, uniqueId;
			if (oldGround) {
				actionId = oldGround->getActionID();
				uniqueId = oldGround->getUniqueID();
			} else {
				actionId = 0;
				uniqueId = 0;
			}
			groundBrush->draw(&editor.map, tile, nullptr);

			Item* newGround = tile->ground.get();
			if (newGround) {
				newGround->setActionID(actionId);
				newGround->setUniqueID(uniqueId);
			}
			TileOperations::update(tile);
		}
		++tiles_done;
	}
}

void MapProcessor::clearInvalidHouseTiles(Editor& editor, std::function<void(int)> on_progress) {
	Houses& houses = editor.map.houses;

	HouseMap::iterator iter = houses.begin();
	while (iter != houses.end()) {
		House* h = iter->second.get();
		if (editor.map.towns.getTown(h->townid) == nullptr) {
			iter = houses.erase(iter);
		} else {
			++iter;
		}
	}

	uint64_t tiles_done = 0;
	const auto total_tiles = editor.map.getTileCount();
	for (auto& tileLocation : editor.map) {
		if (on_progress && tiles_done % 4096 == 0 && total_tiles > 0) {
			on_progress(static_cast<int32_t>(tiles_done / double(total_tiles) * 100.0));
		}

		Tile* tile = tileLocation.get();
		ASSERT(tile);
		if (tile->isHouseTile()) {
			if (houses.getHouse(tile->getHouseID()) == nullptr) {
				tile->setHouse(nullptr);
			}
		}
		++tiles_done;
	}
}

void MapProcessor::clearModifiedTileState(Editor& editor, std::function<void(int)> on_progress) {
	uint64_t tiles_done = 0;
	const auto total_tiles = editor.map.getTileCount();
	for (auto& tileLocation : editor.map) {
		if (on_progress && tiles_done % 4096 == 0 && total_tiles > 0) {
			on_progress(static_cast<int32_t>(tiles_done / double(total_tiles) * 100.0));
		}

		Tile* tile = tileLocation.get();
		ASSERT(tile);
		tile->unmodify();
		++tiles_done;
	}
}
