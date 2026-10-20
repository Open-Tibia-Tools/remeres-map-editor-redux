//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include <limits>

#include "game/complexitem.h" // Door
#include "game/house.h"
#include "map/tile.h"
#include "map/map.h"

#include <format>
#include <algorithm>

Houses::Houses(Map& map) :
	map(map),
	max_house_id(0) {
	////
}

Houses::~Houses() {
}

uint32_t Houses::getEmptyID() {
	// ids were edited or planned id is taken, search forwards
	if (max_house_id == 0 || houses.contains(max_house_id + 1)) {
		while (houses.contains(++max_house_id)) {
			// do nothing, we search for an empty slot
		}

		// houses[n] was nullptr, we return n
		return max_house_id;
	}

	return ++max_house_id;
}

bool Houses::addHouse(std::unique_ptr<House> new_house) {
	ASSERT(new_house);
	HouseMap::iterator it = houses.find(new_house->id);
	if (it != houses.end()) {
		return false;
	}
	new_house->map = &map;
	if (new_house->id > max_house_id) {
		max_house_id = new_house->id;
	}
	houses[new_house->id] = std::move(new_house);
	return true;
}

void Houses::removeHouse(House* house_to_remove) {
	HouseMap::iterator it = houses.find(house_to_remove->id);
	if (it != houses.end()) {
		house_to_remove->clean();
		houses.erase(it);
	}
}

void Houses::changeId(House* house, uint32_t newID) {
	ASSERT(house);
	auto it = houses.find(house->id);
	if (it != houses.end()) {
		auto house_ptr = std::move(it->second);
		houses.erase(it);
		house_ptr->setID(newID);
		houses[newID] = std::move(house_ptr);
	}

	// id list structure changed, prepare search for new free slot
	max_house_id = 0;
}

House* Houses::getHouse(uint32_t houseid) {
	HouseMap::iterator it = houses.find(houseid);
	if (it != houses.end()) {
		return it->second.get();
	}
	return nullptr;
}

const House* Houses::getHouse(uint32_t houseid) const {
	HouseMap::const_iterator it = houses.find(houseid);
	if (it != houses.end()) {
		return it->second.get();
	}

	return nullptr;
}

House::House(Map& map) :
	id(0),
	rent(0),
	townid(0),
	guildhall(false),
	map(&map),
	exit(0, 0, 0) {
	////
}

House::~House() {
	////
}

void House::clean() {
	invalidateSummary();
	for (const auto& pos : tiles) {
		Tile* tile = map->getTile(pos);
		if (tile) {
			tile->setHouse(nullptr);
		}
	}

	Tile* tile = map->getTile(exit);
	if (tile) {
		TileOperations::removeHouseExit(tile, this);
	}
}

size_t House::size() const {
	size_t count = 0;
	for (const auto& pos : tiles) {
		Tile* tile = map->getTile(pos);
		if (tile && !tile->isBlocking()) {
			++count;
		}
	}
	return count;
}

void House::addTile(Tile* tile) {
	ASSERT(tile);
	tile->setHouse(this);
	tiles.push_back(tile->getPosition());
	invalidateSummary();
}

void House::removeTile(Tile* tile) {
	ASSERT(tile);
	auto it = std::ranges::find(tiles, tile->getPosition());
	if (it != tiles.end()) {
		tiles.erase(it);
		tile->setHouse(nullptr);
		invalidateSummary();
	}
}

const House::FloorSummary& House::getFloorSummary(int z) const {
	if (summary_valid_ && summary_z == z) {
		return summary_;
	}

	summary_ = FloorSummary {};
	summary_z = z;
	summary_valid_ = true;

	double sum_x = 0.0;
	double sum_y = 0.0;
	int min_x = std::numeric_limits<int>::max();
	int max_x = std::numeric_limits<int>::min();
	int min_y = std::numeric_limits<int>::max();
	int max_y = std::numeric_limits<int>::min();

	for (const auto& pos : tiles) {
		if (pos.z != z) {
			continue;
		}
		++summary_.count;
		min_x = std::min(min_x, pos.x);
		max_x = std::max(max_x, pos.x);
		min_y = std::min(min_y, pos.y);
		max_y = std::max(max_y, pos.y);
		sum_x += pos.x;
		sum_y += pos.y;
	}

	if (summary_.count == 0) {
		return summary_;
	}

	summary_.min_x = min_x;
	summary_.max_x = max_x;
	summary_.min_y = min_y;
	summary_.max_y = max_y;

	// Tile of the room closest to its centroid, so the label sits inside the room.
	const double avg_x = sum_x / static_cast<double>(summary_.count);
	const double avg_y = sum_y / static_cast<double>(summary_.count);

	double min_dist_sq = std::numeric_limits<double>::infinity();
	for (const auto& pos : tiles) {
		if (pos.z != z) {
			continue;
		}
		const double dx = static_cast<double>(pos.x) - avg_x;
		const double dy = static_cast<double>(pos.y) - avg_y;
		const double dist_sq = dx * dx + dy * dy;
		if (dist_sq < min_dist_sq) {
			min_dist_sq = dist_sq;
			summary_.best_tile = pos;
		}
	}

	return summary_;
}

uint8_t House::getEmptyDoorID() const {
	std::set<uint8_t> taken;
	for (const auto& pos : tiles) {
		if (const Tile* tile = map->getTile(pos)) {
			for (const auto& item : tile->items) {
				if (Door* door = dynamic_cast<Door*>(item.get())) {
					taken.insert(door->getDoorID());
				}
			}
		}
	}

	for (int i = 1; i < 256; ++i) {
		if (!taken.contains(uint8_t(i))) {
			// Free ID!
			return i;
		}
	}
	return 255;
}

Position House::getDoorPositionByID(uint8_t id) const {
	for (const auto& pos : tiles) {
		if (const Tile* tile = map->getTile(pos)) {
			for (const auto& item : tile->items) {
				if (Door* door = dynamic_cast<Door*>(item.get())) {
					if (door->getDoorID() == id) {
						return pos;
					}
				}
			}
		}
	}
	return Position();
}

std::string House::getDescription() {
	return std::format("{} (ID:{}; Rent: {})", name, id, rent);
}

void House::setExit(Map* targetmap, const Position& pos) {
	// This might fail when the user decides to put an exit at 0,0,0, let's just hope noone does (Noone SHOULD, so there is no problem? Hm?)
	if (pos == exit || pos == Position()) {
		return;
	}

	if (exit != Position()) {
		Tile* oldexit = targetmap->getTile(exit);
		if (oldexit) {
			TileOperations::removeHouseExit(oldexit, this);
		}
	}

	Tile* newexit = targetmap->getTile(pos);
	if (!newexit) {
		newexit = targetmap->createTile(pos.x, pos.y, pos.z);
	}

	TileOperations::addHouseExit(newexit, this);
	exit = pos;
}

void House::setExit(const Position& pos) {
	setExit(map, pos);
}
