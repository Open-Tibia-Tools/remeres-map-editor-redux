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

#ifndef RME_TILE_H
#define RME_TILE_H

#include "map/position.h"
#include "game/item.h"
#include "io/otbm/invalid_otbm_content.h"

class TileLocation;
class GroundBrush;
class WallBrush;
class House;
class Map;
#include "app/rme_forward_declarations.h"
#include <unordered_set>
#include <cstdint>
#include <memory>
#include <vector>

enum {
	TILESTATE_NONE = 0x0000,
	TILESTATE_PROTECTIONZONE = 0x0001,
	TILESTATE_DEPRECATED = 0x0002, // Reserved
	TILESTATE_NOPVP = 0x0004,
	TILESTATE_NOLOGOUT = 0x0008,
	TILESTATE_PVPZONE = 0x0010,
	TILESTATE_REFRESH = 0x0020,
	// Internal
	TILESTATE_SELECTED = 0x0001,
	TILESTATE_UNIQUE = 0x0002,
	TILESTATE_BLOCKING = 0x0004,
	TILESTATE_OP_BORDER = 0x0008, // If this is true, gravel will be placed on the tile!
	TILESTATE_HAS_TABLE = 0x0010,
	TILESTATE_HAS_CARPET = 0x0020,
	TILESTATE_MODIFIED = 0x0040,
	TILESTATE_HOOK_SOUTH = 0x0080,
	TILESTATE_HOOK_EAST = 0x0100,
	TILESTATE_HAS_LIGHT = 0x0200,
};

enum : uint8_t {
	INVALID_MINIMAP_COLOR = 0xFF
};

class Tile {
public: // Members
	TileLocation* location;
	TileLocation* ownedLocation;
	std::unique_ptr<Item> ground;
	std::vector<std::unique_ptr<Item>> items;
	std::unique_ptr<Creature> creature;
	std::unique_ptr<Spawn> spawn;
	uint32_t house_id; // House id for this tile (pointer not safe)
	uint32_t mapflags;
	uint16_t statflags;
	uint8_t minimapColor;
	std::unique_ptr<InvalidZoneState> invalidZones;

public:
	// ALWAYS use this constructor if the Tile is EVER going to be placed on a map
	Tile(TileLocation& location);
	// Use this when the tile is only used internally by the editor (like in certain brushes)
	Tile(int x, int y, int z);

	~Tile();

	Tile(const Tile&) = delete;
	Tile& operator=(const Tile&) = delete;

	std::unique_ptr<Tile> deepCopy() const;

	// The location of the tile
	// Stores state that remains between the tile being moved (like house exits)
	void setLocation(TileLocation* where);
	TileLocation* getLocation() {
		return location;
	}
	const TileLocation* getLocation() const {
		return location;
	}

	// Position of the tile
	Position getPosition() const;
	int getX() const;
	int getY() const;
	int getZ() const;

public: // Functions
	// Compare the content (ground and items) of two tiles
	bool isContentEqual(const Tile* other) const;

	// Has tile been modified since the map was loaded/created?
	bool isModified() const {
		return testFlags(statflags, TILESTATE_MODIFIED);
	}
	void modify();
	void unmodify() {
		statflags &= ~TILESTATE_MODIFIED;
	}

	// Get memory footprint size
	uint32_t memsize() const;
	// Get number of items on the tile
	bool empty() const {
		return size() == 0;
	}
	int size() const;

	// Blocking?
	bool isBlocking() const {
		return testFlags(statflags, TILESTATE_BLOCKING);
	}

	// PZ
	bool isPZ() const {
		return testFlags(mapflags, TILESTATE_PROTECTIONZONE);
	}
	void setPZ(bool pz) {
		if (pz) {
			mapflags |= TILESTATE_PROTECTIONZONE;
		} else {
			mapflags &= ~TILESTATE_PROTECTIONZONE;
		}
	}

	bool hasProperty(enum ITEMPROPERTY prop) const;

	int getIndexOf(Item* item) const;
	Item* getTopItem() const; // Returns the topmost item, or nullptr if the tile is empty
	Item* getItemAt(int index) const;
	void setGround(std::unique_ptr<Item> item);
	void addItem(std::unique_ptr<Item> item);
	void setGroundFast(std::unique_ptr<Item> item);
	void addItemFast(std::unique_ptr<Item> item);
	void reserveItems(size_t count) {
		items.reserve(count);
	}
	InvalidZoneState& getOrCreateInvalidZones();
	void clearInvalidZones();
	void addOpaqueTileAttribute(OpaqueTileAttributeRecord record);
	void addOpaqueChildNode(PreservedOTBMNode node);
	void recordUnknownMapFlags(uint32_t rawFlags, uint32_t unknownBits);
	[[nodiscard]] const InvalidZoneState* getInvalidZones() const;
	[[nodiscard]] bool hasInvalidZones() const;

	bool isSelected() const {
		return testFlags(statflags, TILESTATE_SELECTED);
	}
	void select() {
		statflags |= TILESTATE_SELECTED;
	}
	void deselect() {
		statflags &= ~TILESTATE_SELECTED;
	}
	bool hasUniqueItem() const {
		return testFlags(statflags, TILESTATE_UNIQUE);
	}

	uint8_t getMiniMapColor() const;

	// Does this tile have ground?
	bool hasGround() const {
		return ground != nullptr;
	}
	bool hasBorders() const {
		return items.size() && items[0]->isBorder();
	}

	// Get the border brush of this tile
	GroundBrush* getGroundBrush() const;

	bool hasTable() const {
		return testFlags(statflags, TILESTATE_HAS_TABLE);
	}
	Item* getTable() const;

	bool hasCarpet() const {
		return testFlags(statflags, TILESTATE_HAS_CARPET);
	}
	Item* getCarpet() const;

	bool hasHookSouth() const {
		return testFlags(statflags, TILESTATE_HOOK_SOUTH);
	}

	bool hasHookEast() const {
		return testFlags(statflags, TILESTATE_HOOK_EAST);
	}

	bool hasLight() const {
		return testFlags(statflags, TILESTATE_HAS_LIGHT);
	}

	bool hasOptionalBorder() const {
		return testFlags(statflags, TILESTATE_OP_BORDER);
	}
	void setOptionalBorder(bool b) {
		if (b) {
			statflags |= TILESTATE_OP_BORDER;
		} else {
			statflags &= ~TILESTATE_OP_BORDER;
		}
	}

	// Get the (first) wall of this tile
	Item* getWall() const;
	bool hasWall() const;

	// Has to do with houses
	bool isHouseTile() const;
	uint32_t getHouseID() const;
	void setHouseID(uint32_t newHouseId);

	bool isHouseExit() const;
	bool isTownExit(Map& map) const;
	const HouseExitList* getHouseExits() const;
	HouseExitList* getHouseExits();
	bool hasHouseExit(uint32_t exit) const;
	void setHouse(House* house);

	// Mapflags (PZ, PVPZONE etc.)
	void setMapFlags(uint32_t _flags);
	void unsetMapFlags(uint32_t _flags);
	uint32_t getMapFlags() const;

	// Statflags (You really ought not to touch this)
	void setStatFlags(uint16_t _flags);
	void unsetStatFlags(uint16_t _flags);
	uint16_t getStatFlags() const;
};

bool tilePositionLessThan(const Tile* a, const Tile* b);
// This sorts them by draw order
bool tilePositionVisualLessThan(const Tile* a, const Tile* b);

using TileVector = std::vector<Tile*>;
using TileSet = std::vector<Tile*>;
using TileList = std::list<Tile*>;

inline bool Tile::hasWall() const {
	return getWall() != nullptr;
}

inline bool Tile::isHouseTile() const {
	return house_id != 0;
}

inline uint32_t Tile::getHouseID() const {
	return house_id;
}

inline InvalidZoneState& Tile::getOrCreateInvalidZones() {
	if (!invalidZones) {
		invalidZones = std::make_unique<InvalidZoneState>();
	}
	return *invalidZones;
}

inline void Tile::clearInvalidZones() {
	if (invalidZones && invalidZones->unknownMapFlagBits != 0) {
		unsetMapFlags(invalidZones->unknownMapFlagBits);
	}
	invalidZones.reset();
}

inline void Tile::addOpaqueTileAttribute(OpaqueTileAttributeRecord record) {
	auto& state = getOrCreateInvalidZones();
	state.hasStructuralMismatch = true;
	state.opaqueTileAttributes.push_back(std::move(record));
}

inline void Tile::addOpaqueChildNode(PreservedOTBMNode node) {
	auto& state = getOrCreateInvalidZones();
	state.hasStructuralMismatch = true;
	state.opaqueChildNodes.push_back(std::move(node));
}

inline void Tile::recordUnknownMapFlags(uint32_t rawFlags, uint32_t unknownBits) {
	auto& state = getOrCreateInvalidZones();
	state.rawMapFlags = rawFlags;
	state.unknownMapFlagBits |= unknownBits;
	state.hasStructuralMismatch = state.hasStructuralMismatch || unknownBits != 0;
}

inline const InvalidZoneState* Tile::getInvalidZones() const {
	return invalidZones.get();
}

inline bool Tile::hasInvalidZones() const {
	return invalidZones && invalidZones->hasContent();
}

inline void Tile::setMapFlags(uint32_t _flags) {
	const uint32_t preservedUnknownBits = invalidZones ? invalidZones->unknownMapFlagBits : 0;
	mapflags = _flags | preservedUnknownBits;
}

inline void Tile::unsetMapFlags(uint32_t _flags) {
	mapflags &= ~_flags;
}

inline uint32_t Tile::getMapFlags() const {
	return mapflags;
}

inline void Tile::setStatFlags(uint16_t _flags) {
	statflags = _flags | statflags;
}

inline void Tile::unsetStatFlags(uint16_t _flags) {
	statflags &= ~_flags;
}

inline uint16_t Tile::getStatFlags() const {
	return statflags;
}

#endif
