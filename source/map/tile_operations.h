//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_TILE_OPERATIONS_H
#define RME_TILE_OPERATIONS_H

#include "game/item.h"
#include <memory>
#include <vector>

class Tile;
class BaseMap;
class WallBrush;
class House;

namespace TileOperations {
	void borderize(Tile* tile, BaseMap* map);
	void cleanBorders(Tile* tile);

	void wallize(Tile* tile, BaseMap* map);
	void cleanWalls(Tile* tile);
	void cleanWalls(Tile* tile, WallBrush* wb);

	void tableize(Tile* tile, BaseMap* map);
	void cleanTables(Tile* tile);

	void carpetize(Tile* tile, BaseMap* map);

	// Moved from Tile class
	std::unique_ptr<Tile> deepCopy(const Tile* tile, BaseMap& map);
	void merge(Tile* dest, Tile* src, const BaseMap* target_map = nullptr);
	Item* transformItem(Item* old_item, uint16_t new_id, Tile* parent = nullptr);

	void select(Tile* tile);
	void deselect(Tile* tile);
	void selectGround(Tile* tile);
	void deselectGround(Tile* tile);
	void markSelectionChanged(Tile* tile);

	std::vector<std::unique_ptr<Item>> popSelectedItems(Tile* tile, bool ignoreTileSelected = false);
	ItemVector getSelectedItems(Tile* tile, bool unzoomed = false);
	Item* getTopSelectedItem(Tile* tile);

	void addBorderItem(Tile* tile, std::unique_ptr<Item> item);
	void addWallItem(Tile* tile, std::unique_ptr<Item> item);

	void addHouseExit(Tile* tile, House* h);
	void removeHouseExit(Tile* tile, House* h);

	void update(Tile* tile);
	void updateSelectionState(Tile* tile);
}

#endif
