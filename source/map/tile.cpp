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

#include "brushes/brush.h"

#include "editor/editor.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "ui/gui.h"
#include "ui/managers/minimap_manager.h"
#include "game/creature.h"
#include "game/house.h"
#include "map/basemap.h"
#include "map/map_region.h"
#include "game/spawn.h"
#include <ranges>
#include <algorithm>
#include <iterator>
#include <memory>

namespace {
	bool itemsMatch(const Item* lhs, const Item* rhs) {
		if (lhs == rhs) {
			return true;
		}
		if (!lhs || !rhs) {
			return false;
		}
		if (lhs->getID() != rhs->getID() || lhs->getSubtype() != rhs->getSubtype()) {
			return false;
		}

		const InvalidOTBMItemData* lhsInvalid = lhs->getInvalidOTBMData();
		const InvalidOTBMItemData* rhsInvalid = rhs->getInvalidOTBMData();
		if (static_cast<bool>(lhsInvalid) != static_cast<bool>(rhsInvalid)) {
			return false;
		}
		if (lhsInvalid && rhsInvalid && *lhsInvalid != *rhsInvalid) {
			return false;
		}
		return true;
	}

	uint32_t preservedNodeHeapSize(const PreservedOTBMNode& node) {
		uint32_t bytes = static_cast<uint32_t>(node.rawPayload.capacity());
		bytes += static_cast<uint32_t>(node.children.capacity() * sizeof(PreservedOTBMNode));
		for (const auto& child : node.children) {
			bytes += preservedNodeHeapSize(child);
		}
		return bytes;
	}
}

Tile::Tile(int x, int y, int z) :
	location(nullptr),
	ownedLocation(new TileLocation()),
	ground(nullptr),
	house_id(0),
	mapflags(0),
	statflags(0),
	minimapColor(INVALID_MINIMAP_COLOR) {
	ownedLocation->setPosition(Position(x, y, z));
	location = ownedLocation;
}

Tile::Tile(TileLocation& loc) :
	location(&loc),
	ownedLocation(nullptr),
	ground(nullptr),
	house_id(0),
	mapflags(0),
	statflags(0),
	minimapColor(INVALID_MINIMAP_COLOR) {
	////
}

void Tile::setLocation(TileLocation* where) {
	if (ownedLocation) {
		delete ownedLocation;
		ownedLocation = nullptr;
	}
	location = where;
}

Position Tile::getPosition() const {
	const TileLocation* loc = location ? location : ownedLocation;
	return loc ? loc->getPosition() : Position();
}

int Tile::getX() const {
	const TileLocation* loc = location ? location : ownedLocation;
	return loc ? loc->getPosition().x : 0;
}

int Tile::getY() const {
	const TileLocation* loc = location ? location : ownedLocation;
	return loc ? loc->getPosition().y : 0;
}

int Tile::getZ() const {
	const TileLocation* loc = location ? location : ownedLocation;
	return loc ? loc->getPosition().z : 0;
}

HouseExitList* Tile::getHouseExits() {
	TileLocation* loc = location ? location : ownedLocation;
	return loc ? loc->getHouseExits() : nullptr;
}

const HouseExitList* Tile::getHouseExits() const {
	const TileLocation* loc = location ? location : ownedLocation;
	return loc ? loc->getHouseExits() : nullptr;
}

bool Tile::isHouseExit() const {
	const HouseExitList* house_exits = getHouseExits();
	return house_exits && !house_exits->empty();
}

bool Tile::hasHouseExit(uint32_t exit) const {
	const HouseExitList* house_exits = getHouseExits();
	if (house_exits) {
		return std::ranges::any_of(*house_exits, [exit](uint32_t current_exit) { return current_exit == exit; });
	}
	return false;
}

Tile::~Tile() {
	if (ownedLocation) {
		delete ownedLocation;
		ownedLocation = nullptr;
	}
	// Smart pointers handle deletion
}

std::unique_ptr<Tile> Tile::deepCopy() const {
	std::unique_ptr<Tile> copy;
	const TileLocation* loc = location ? location : ownedLocation;
	if (loc) {
		copy = std::make_unique<Tile>(loc->getPosition().x, loc->getPosition().y, loc->getPosition().z);
		std::unique_ptr<TileLocation> clonedLocation = loc->clone();
		if (copy->ownedLocation) {
			delete copy->ownedLocation;
		}
		copy->ownedLocation = clonedLocation.release();
		copy->location = copy->ownedLocation;
	} else {
		// Detached tile: keep a safe owned location even if we do not have one yet.
		copy = std::make_unique<Tile>(0, 0, 0);
	}
	if (ground) {
		copy->ground = ground->deepCopy();
	}
	for (const auto& item : items) {
		copy->items.push_back(item->deepCopy());
	}
	if (creature) {
		copy->creature = creature->deepCopy();
	}
	if (spawn) {
		copy->spawn = spawn->deepCopy();
	}
	copy->house_id = house_id;
	copy->mapflags = mapflags;
	copy->statflags = statflags;
	copy->minimapColor = minimapColor;
	if (invalidZones) {
		copy->invalidZones = std::make_unique<InvalidZoneState>(*invalidZones);
	}
	return copy;
}

uint32_t Tile::memsize() const {
	uint32_t mem = sizeof(*this);
	if (ownedLocation) {
		mem += sizeof(TileLocation);
	}
	if (ground) {
		mem += ground->memsize();
	}

	for (const auto& item : items) {
		mem += item->memsize();
	}

	mem += sizeof(std::unique_ptr<Item>) * items.capacity();
	if (invalidZones) {
		mem += sizeof(InvalidZoneState);
		mem += static_cast<uint32_t>(invalidZones->opaqueTileAttributes.capacity() * sizeof(OpaqueTileAttributeRecord));
		for (const auto& attribute : invalidZones->opaqueTileAttributes) {
			mem += static_cast<uint32_t>(attribute.rawBytes.capacity());
		}
		mem += static_cast<uint32_t>(invalidZones->opaqueChildNodes.capacity() * sizeof(PreservedOTBMNode));
		for (const auto& node : invalidZones->opaqueChildNodes) {
			mem += preservedNodeHeapSize(node);
		}
	}

	return mem;
}

int Tile::size() const {
	int sz = 0;
	if (ground) {
		++sz;
	}
	sz += items.size();
	if (creature) {
		++sz;
	}
	if (spawn) {
		++sz;
	}
	if (house_id != 0) {
		++sz;
	}
	if (mapflags) {
		++sz;
	}
	if (hasInvalidZones()) {
		++sz;
	}
	const TileLocation* loc = location ? location : ownedLocation;
	if (loc) {
		if (loc && loc->getHouseExits()) {
			++sz;
		}
		if (loc && loc->getSpawnCount()) {
			++sz;
		}
		if (loc && loc->getWaypointCount()) {
			++sz;
		}
	}
	return sz;
}

bool Tile::hasProperty(enum ITEMPROPERTY prop) const {
	if (prop == PROTECTIONZONE && isPZ()) {
		return true;
	}

	if (prop == BLOCKSOLID) {
		// Optimization: Use cached blocking state
		// Note: isBlocking() returns true for empty tiles (void), but hasProperty checks if *content* has property.
		return isBlocking() && (ground || !items.empty());
	}

	if (ground && ground->hasProperty(prop)) {
		return true;
	}

	return std::ranges::any_of(items, [prop](const auto& i) {
		return i->hasProperty(prop);
	});
}

int Tile::getIndexOf(Item* item) const {
	if (!item) {
		return wxNOT_FOUND;
	}

	int index = 0;
	if (ground) {
		if (ground.get() == item) {
			return index;
		}
		index++;
	}

	if (!items.empty()) {
		if (auto it = std::ranges::find_if(items, [item](const std::unique_ptr<Item>& i) { return i.get() == item; }); it != items.end()) {
			index += std::distance(items.begin(), it);
			return index;
		}
	}
	return wxNOT_FOUND;
}

Item* Tile::getTopItem() const {
	if (!items.empty() && !items.back()->isMetaItem()) {
		return items.back().get();
	}
	if (ground && !ground->isMetaItem()) {
		return ground.get();
	}
	return nullptr;
}

Item* Tile::getItemAt(int index) const {
	if (index < 0) {
		return nullptr;
	}
	if (ground) {
		if (index == 0) {
			return ground.get();
		}
		index--;
	}
	if (index >= 0 && index < (int)items.size()) {
		return items.at(index).get();
	}
	return nullptr;
}

void Tile::setGround(std::unique_ptr<Item> item) {
	if (!item) {
		ground.reset();
		TileOperations::update(this);
		return;
	}

	ASSERT(item->isGroundTile());
	ground = std::move(item);
	TileOperations::update(this);
}

void Tile::addItem(std::unique_ptr<Item> item) {
	if (!item) {
		return;
	}
	if (item->isGroundTile()) {
		// Generic insertion preserves literal tile identity. Terrain policy lives in brushes/ground.
		setGround(std::move(item));
		return;
	}
	auto it = items.begin();
	
	// At the very bottom!
	if (item->isAlwaysOnBottom()) {
		// Find insertion point for always-on-bottom items
		// They are sorted by TopOrder, and come before normal items.
		it = std::ranges::find_if(items, [&](const std::unique_ptr<Item>& i) {
			if (!i->isAlwaysOnBottom()) {
				return true; // Found a normal item, insert before it
			}
			return item->getTopOrder() < i->getTopOrder(); // Found a bottom item with higher order
		});
	} else {
		it = items.end();
	}

	items.insert(it, std::move(item));
	TileOperations::update(this);
}

uint8_t Tile::getMiniMapColor() const {
	if (minimapColor != INVALID_MINIMAP_COLOR) {
		return minimapColor;
	}

	auto view = std::ranges::reverse_view(items);
	auto it = std::ranges::find_if(view, [](const std::unique_ptr<Item>& i) {
		return i->getMiniMapColor() != 0;
	});

	if (it != view.end()) {
		return it->get()->getMiniMapColor();
	}

	// check ground too
	if (hasGround()) {
		return ground->getMiniMapColor();
	}

	return 0;
}

void Tile::modify() {
	statflags |= TILESTATE_MODIFIED;
	minimapColor = INVALID_MINIMAP_COLOR;

	if (!ownedLocation) {
		if (Editor* editor = g_gui.GetCurrentEditor()) {
			const Position position = getPosition();
			if (editor->map.getTile(position) == this) {
				g_minimap.MarkTileDirty(editor->map, position);
			}
		}
	}

	if (isSelected()) {
		TileOperations::markSelectionChanged(this);
	}
}

bool tilePositionLessThan(const Tile* a, const Tile* b) {
	return a->getPosition() < b->getPosition();
}

bool tilePositionVisualLessThan(const Tile* a, const Tile* b) {
	Position pa = a->getPosition();
	Position pb = b->getPosition();

	if (pa.z > pb.z) {
		return true;
	}
	if (pa.z < pb.z) {
		return false;
	}

	if (pa.y < pb.y) {
		return true;
	}
	if (pa.y > pb.y) {
		return false;
	}

	if (pa.x < pb.x) {
		return true;
	}

	return false;
}

GroundBrush* Tile::getGroundBrush() const {
	if (ground) {
		if (ground->getGroundBrush()) {
			return ground->getGroundBrush();
		}
	}
	return nullptr;
}

Item* Tile::getWall() const {
	auto it = std::ranges::find_if(items, [](const std::unique_ptr<Item>& i) {
		return i->isWall();
	});
	return (it != items.end()) ? it->get() : nullptr;
}

Item* Tile::getCarpet() const {
	auto it = std::ranges::find_if(items, [](const std::unique_ptr<Item>& i) {
		return i->isCarpet();
	});
	return (it != items.end()) ? it->get() : nullptr;
}

Item* Tile::getTable() const {
	auto it = std::ranges::find_if(items, [](const std::unique_ptr<Item>& i) {
		return i->isTable();
	});
	return (it != items.end()) ? it->get() : nullptr;
}

void Tile::setHouse(House* _house) {
	house_id = (_house ? _house->getID() : 0);
}

void Tile::setHouseID(uint32_t newHouseId) {
	house_id = newHouseId;
}

bool Tile::isTownExit(Map& map) const {
	const TileLocation* loc = location ? location : ownedLocation;
	return loc && loc->getTownCount() > 0;
}

bool Tile::isContentEqual(const Tile* other) const {
	if (!other) {
		return false;
	}

	if (static_cast<bool>(invalidZones) != static_cast<bool>(other->invalidZones)) {
		return false;
	}
	if (invalidZones && other->invalidZones && *invalidZones != *other->invalidZones) {
		return false;
	}

	// Compare ground
	if (!itemsMatch(ground.get(), other->ground.get())) {
		return false;
	}

	// Compare items
	return std::ranges::equal(items, other->items, [](const std::unique_ptr<Item>& it1, const std::unique_ptr<Item>& it2) {
		return itemsMatch(it1.get(), it2.get());
	});
}
