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

#ifndef RME_CONTAINER_H_
#define RME_CONTAINER_H_

#include "map/position.h"
#include "game/item.h"
#include "game/outfit.h"

#pragma pack(1)

struct OTBM_TeleportDestination {
	uint16_t x;
	uint16_t y;
	uint8_t z;
};

#pragma pack()

class Container : public Item {
public:
	Container(const ServerItemId type);
	~Container();

	std::unique_ptr<Item> deepCopy() const;
	Item* getItem(size_t index) const;

	Container* asContainer() override {
		return this;
	}
	const Container* asContainer() const override {
		return this;
	}

	size_t getItemCount() const {
		return contents.size();
	}
	size_t getVolume() const {
		return static_cast<size_t>(getDefinition().attribute(ItemAttributeKey::Volume));
	}

	std::vector<std::unique_ptr<Item>>& getVector() {
		return contents;
	}
	const std::vector<std::unique_ptr<Item>>& getVector() const {
		return contents;
	}
	double getWeight() const;
	uint32_t memsize() const override;

	virtual bool unserializeItemNode_OTBM(const IOMap& maphandle, BinaryNode* node);
	virtual bool serializeItemNode_OTBM(const IOMap& maphandle, NodeFileWriteHandle& f) const;

protected:
	std::vector<std::unique_ptr<Item>> contents;
};

class Teleport : public Item {
public:
	Teleport(const ServerItemId type);

	std::unique_ptr<Item> deepCopy() const;

	Teleport* asTeleport() override {
		return this;
	}
	const Teleport* asTeleport() const override {
		return this;
	}

	int32_t getX() const {
		return destination.x;
	}
	int32_t getY() const {
		return destination.y;
	}
	int32_t getZ() const {
		return destination.z;
	}

	Position getDestination() const {
		return destination;
	}
	void setDestination(const Position& position) {
		destination = position;
	}

	bool hasDestination() const {
		return destination != Position();
	}

protected:
	// We could've made this public and skip the functions, but that would
	// make the handling of aid/uid/text different from handling teleports,
	// which would be weird.
	Position destination;
};

class Door : public Item {
public:
	Door(const ServerItemId type);

	std::unique_ptr<Item> deepCopy() const;

	Door* asDoor() override {
		return this;
	}
	const Door* asDoor() const override {
		return this;
	}

	uint8_t getDoorID() const;
	void setDoorID(uint8_t id);

	DoorType getDoorType() const;
	bool isRealDoor() const;

protected:
	uint8_t doorId;
};

class Depot : public Item {
public:
	Depot(const ServerItemId _type);

	std::unique_ptr<Item> deepCopy() const;

	Depot* asDepot() override {
		return this;
	}
	const Depot* asDepot() const override {
		return this;
	}

	uint8_t getDepotID() const {
		return depotId;
	}
	void setDepotID(uint8_t id) {
		depotId = id;
	}

protected:
	uint8_t depotId;
};

class Podium : public Item {
public:
	Podium(const ServerItemId _type);

	std::unique_ptr<Item> deepCopy() const;

	Podium* asPodium() override {
		return this;
	}
	const Podium* asPodium() const override {
		return this;
	}

	const Outfit& getOutfit() const {
		return outfit;
	}
	void setOutfit(const Outfit& newOutfit) {
		outfit = newOutfit;
	}

	uint8_t getDirection() const {
		return direction;
	}
	void setDirection(uint8_t newDirection) {
		direction = newDirection;
	}

	bool hasShowOutfit() const {
		return showOutfit && outfit.lookType != 0;
	}
	bool hasShowMount() const {
		return showMount && outfit.lookMount != 0;
	}
	bool hasShowPlatform() const {
		return showPlatform;
	}

	bool getShowOutfit() const {
		return showOutfit;
	}
	bool getShowMount() const {
		return showMount;
	}
	bool getShowPlatform() const {
		return showPlatform;
	}

	void setShowOutfit(bool newState) {
		showOutfit = newState;
	}
	void setShowMount(bool newState) {
		showMount = newState;
	}
	void setShowPlatform(bool newState) {
		showPlatform = newState;
	}

protected:
	Outfit outfit;
	uint8_t direction;
	bool showOutfit = true;
	bool showMount = true;
	bool showPlatform = true;
};
#endif
