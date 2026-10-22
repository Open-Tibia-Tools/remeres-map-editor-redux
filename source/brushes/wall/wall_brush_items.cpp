//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "brushes/wall/wall_brush_items.h"
#include "app/main.h" // For random

void WallBrushItems::addWallItem(int alignment, uint16_t id, int chance) {
	if (alignment < 0 || alignment >= 17) {
		return;
	}

	WallNode& node = wall_items[alignment];
	node.total_chance += chance;

	WallItem item;
	item.id = id;
	item.chance = node.total_chance;
	node.items.push_back(item);
}

void WallBrushItems::addDoorItem(int alignment, uint16_t id, ::DoorType type, bool locked) {
	if (alignment < 0 || alignment >= WALL_ALIGNMENT_COUNT) {
		return;
	}

	DoorItem item;
	item.type = type;
	item.id = id;
	item.locked = locked;
	door_items[alignment].push_back(item);
	door_type_map.try_emplace(id, type);
}

uint16_t WallBrushItems::getRandomWallId(int alignment) const {
	if (alignment < 0 || alignment >= 17) {
		return 0;
	}

	const WallNode& wn = wall_items[alignment];
	if (wn.total_chance <= 0) {
		if (wn.items.empty()) {
			return 0;
		}
		return wn.items.front().id;
	}

	int chance = random(1, wn.total_chance);
	for (const auto& item : wn.items) {
		if (chance <= item.chance) {
			return item.id;
		}
	}
	return 0;
}

bool WallBrushItems::hasWall(uint16_t id, int alignment) const {
	if (alignment < 0 || alignment >= 17) {
		return false;
	}

	const WallNode& wn = wall_items[alignment];
	for (const auto& item : wn.items) {
		if (id == item.id) {
			return true;
		}
	}
	return hasDoor(id, alignment);
}

bool WallBrushItems::hasDoor(uint16_t id, int alignment) const {
	if (alignment < 0 || alignment >= WALL_ALIGNMENT_COUNT) {
		return false;
	}
	if (!door_type_map.contains(id)) {
		return false;
	}
	const auto& doors = door_items[alignment];
	for (const auto& door : doors) {
		if (door.id == id) {
			return true;
		}
	}
	return false;
}

::DoorType WallBrushItems::getDoorTypeFromID(uint16_t id) const {
	const auto it = door_type_map.find(id);
	return (it != door_type_map.end()) ? it->second : WALL_UNDEFINED;
}
