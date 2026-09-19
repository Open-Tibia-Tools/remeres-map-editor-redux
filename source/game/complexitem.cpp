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

#include "app/main.h"

#include "game/complexitem.h"

#include "io/iomap.h"
#include "brushes/brush_enums.h"
#include "brushes/wall/wall_brush.h"

// Container
Container::Container(const ServerItemId type) :
	Item(type, 0) {
	////
}

Container::~Container() {
	// std::unique_ptr handles cleanup
}

std::unique_ptr<Item> Container::deepCopy() const {
	std::unique_ptr<Item> copy = Item::deepCopy();
	Container* copyContainer = dynamic_cast<Container*>(copy.get());
	if (copyContainer) {
		copyContainer->contents.reserve(contents.size());
		for (const auto& item : contents) {
			copyContainer->contents.push_back(item->deepCopy());
		}
	}
	return copy;
}

Item* Container::getItem(size_t index) const {
	if (index < contents.size()) {
		return contents[index].get();
	}
	return nullptr;
}

double Container::getWeight() const {
	double total_weight = Item::getWeight();
	for (const auto& item : contents) {
		if (item) {
			total_weight += item->getWeight();
		}
	}
	return total_weight;
}

uint32_t Container::memsize() const {
	uint32_t size = sizeof(*this);
	size += sizeof(std::unique_ptr<Item>) * contents.capacity();
	for (const auto& item : contents) {
		size += item->memsize();
	}
	return size;
}

// Teleport
Teleport::Teleport(const ServerItemId type) :
	Item(type, 0),
	destination(0, 0, 0) {
	////
}

std::unique_ptr<Item> Teleport::deepCopy() const {
	std::unique_ptr<Item> copy = Item::deepCopy();
	if (Teleport* teleport_copy = dynamic_cast<Teleport*>(copy.get())) {
		teleport_copy->destination = destination;
	}
	return copy;
}

// Door
Door::Door(const ServerItemId type) :
	Item(type, 0),
	doorId(0) {
	////
}

std::unique_ptr<Item> Door::deepCopy() const {
	std::unique_ptr<Item> copy = Item::deepCopy();
	if (Door* door_copy = dynamic_cast<Door*>(copy.get())) {
		door_copy->doorId = doorId;
	}
	return copy;
}

void Door::setDoorID(uint8_t id) {
	doorId = isRealDoor() ? id : 0;
}

uint8_t Door::getDoorID() const {
	return isRealDoor() ? doorId : 0;
}

bool Door::isRealDoor() const {
	const DoorType dt = getDoorType();
	// doors with no wallbrush will appear as WALL_UNDEFINED
	// this is for compatibility
	switch (dt) {
		case WALL_UNDEFINED:
		case WALL_DOOR_NORMAL:
		case WALL_DOOR_LOCKED:
		case WALL_DOOR_QUEST:
		case WALL_DOOR_MAGIC:
		case WALL_DOOR_NORMAL_ALT:
			return true;
		default:
			return false;
	}
}

DoorType Door::getDoorType() const {
	WallBrush* wb = getWallBrush();
	if (!wb) {
		return WALL_UNDEFINED;
	}

	return wb->getDoorTypeFromID(id);
}

// Depot
Depot::Depot(const ServerItemId type) :
	Item(type, 0),
	depotId(0) {
	////
}

std::unique_ptr<Item> Depot::deepCopy() const {
	std::unique_ptr<Item> copy = Item::deepCopy();
	if (Depot* copy_depot = dynamic_cast<Depot*>(copy.get())) {
		copy_depot->depotId = depotId;
	}
	return copy;
}

// Podium
Podium::Podium(const ServerItemId type) :
	Item(type, 0),
	outfit(Outfit()), direction(0), showOutfit(true), showMount(true), showPlatform(true) {
	////
}

std::unique_ptr<Item> Podium::deepCopy() const {
	std::unique_ptr<Item> copy = Item::deepCopy();
	if (Podium* copy_podium = dynamic_cast<Podium*>(copy.get())) {
		copy_podium->outfit = outfit;
		copy_podium->showOutfit = showOutfit;
		copy_podium->showMount = showMount;
		copy_podium->showPlatform = showPlatform;
		copy_podium->direction = direction;
	}
	return copy;
}
