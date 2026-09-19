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

#ifndef RME_ITEM_H_
#define RME_ITEM_H_

#include "item_definitions/core/item_definition_store.h"
#include <memory>
#include <string_view>
#include "io/iomap_otbm.h"
#include "io/otbm/invalid_otbm_content.h"
#include "game/item_attributes.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/raw/raw_brush.h"

class Creature;
class Border;
class Tile;
class Depot;
class Podium;

struct SpriteLight;
class GameSprite;

enum ITEMPROPERTY {
	BLOCKSOLID,
	HASHEIGHT,
	BLOCKPROJECTILE,
	BLOCKPATHFIND,
	PROTECTIONZONE,
	HOOK_SOUTH,
	HOOK_EAST,
	MOVEABLE,
	BLOCKINGANDNOTMOVEABLE
};

enum SplashType {
	LIQUID_NONE = 0,
	LIQUID_WATER = 1,
	LIQUID_BLOOD = 2,
	LIQUID_BEER = 3,
	LIQUID_SLIME = 4,
	LIQUID_LEMONADE = 5,
	LIQUID_MILK = 6,
	LIQUID_MANAFLUID = 7,
	LIQUID_INK = 8,
	LIQUID_WATER2 = 9,
	LIQUID_LIFEFLUID = 10,
	LIQUID_OIL = 11,
	LIQUID_SLIME2 = 12,
	LIQUID_URINE = 13,
	LIQUID_COCONUT_MILK = 14,
	LIQUID_WINE = 15,
	LIQUID_MUD = 19,
	LIQUID_FRUIT_JUICE = 21,
	LIQUID_LAVA = 26,
	LIQUID_RUM = 27,
	LIQUID_SWAMP = 28,
	LIQUID_TEA = 35,
	LIQUID_MEAD = 43,

	LIQUID_FIRST = LIQUID_WATER,
	LIQUID_LAST = LIQUID_MEAD
};

IMPLEMENT_INCREMENT_OP(SplashType)

class Item : public ItemAttributes {
public:
	// Attribute keys
	inline static const std::string ATTR_UID = "uid";
	inline static const std::string ATTR_AID = "aid";
	inline static const std::string ATTR_TEXT = "text";
	inline static const std::string ATTR_DESC = "desc";
	inline static const std::string ATTR_TIER = "tier";

	// Factory member to create item of right type based on type
	static std::unique_ptr<Item> Create(ServerItemId _type, uint16_t _subtype = 0xFFFF);
	static std::unique_ptr<Item> Create(pugi::xml_node);
	static std::unique_ptr<Item> Create_OTBM(const IOMap& maphandle, BinaryNode* stream);

public:
	// Constructor for items
	Item(ServerItemId _type, unsigned short _count);

	virtual ~Item();

	// Deep copy thingy
	virtual std::unique_ptr<Item> deepCopy() const;

	// Get memory footprint size
	virtual uint32_t memsize() const;
	virtual class Container* asContainer() {
		return nullptr;
	}
	virtual const class Container* asContainer() const {
		return nullptr;
	}
	virtual class Teleport* asTeleport() {
		return nullptr;
	}
	virtual const class Teleport* asTeleport() const {
		return nullptr;
	}
	virtual class TrashHolder* asTrashHolder() {
		return nullptr;
	}
	virtual const class TrashHolder* asTrashHolder() const {
		return nullptr;
	}
	virtual class Mailbox* asMailbox() {
		return nullptr;
	}
	virtual const class Mailbox* asMailbox() const {
		return nullptr;
	}
	virtual class Door* asDoor() {
		return nullptr;
	}
	virtual const class Door* asDoor() const {
		return nullptr;
	}
	virtual class MagicField* asMagicField() {
		return nullptr;
	}
	virtual const class MagicField* asMagicField() const {
		return nullptr;
	}
	virtual class Depot* asDepot() {
		return nullptr;
	}
	virtual const class Depot* asDepot() const {
		return nullptr;
	}
	virtual class Podium* asPodium() {
		return nullptr;
	}
	virtual const class Podium* asPodium() const {
		return nullptr;
	}

	// OTBM map interface
	// Serialize and unserialize (for save/load)
	// Used internally
	virtual bool unserializeItemNode_OTBM(const IOMap& maphandle, BinaryNode* node);

	// Will return a node containing this item
	virtual bool serializeItemNode_OTBM(const IOMap& maphandle, NodeFileWriteHandle& f) const;
	// Will write this item to the stream supplied in the argument
	virtual void serializeItemCompact_OTBM(const IOMap& maphandle, NodeFileWriteHandle& f) const;

	// Static conversions
	static std::string_view LiquidID2Name(uint16_t id);
	static uint16_t LiquidName2ID(std::string id);

	// IDs
	[[nodiscard]] ServerItemId getID() const {
		return id;
	}
	void setID(ServerItemId new_id) {
		id = new_id;
	}
	// Type access via stable index — no cached pointer, safe across vector resizes
	[[nodiscard]] ItemDefinitionView getDefinition() const {
		return g_item_definitions.get(id);
	}
	[[nodiscard]] ClientItemId getClientID() const {
		if (const auto definition = getDefinition()) {
			return definition.clientId();
		}
		return 0;
	}
	[[nodiscard]] GameSprite* getSprite() const;

	bool typeExists() const {
		return g_item_definitions.typeExists(id);
	}
	[[nodiscard]] bool isInvalidOTBMItem() const {
		return invalidOtbmData != nullptr;
	}
	[[nodiscard]] const InvalidOTBMItemData* getInvalidOTBMData() const {
		return invalidOtbmData.get();
	}
	void setInvalidOTBMData(InvalidOTBMItemData data) {
		invalidOtbmData = std::make_unique<InvalidOTBMItemData>(std::move(data));
	}
	void clearInvalidOTBMData() {
		invalidOtbmData.reset();
	}
	[[nodiscard]] InvalidOTBMItemMarkerColor invalidOTBMMarkerColor() const {
		return invalidOtbmData ? invalidOtbmData->markerColor() : InvalidOTBMItemMarkerColor::None;
	}

	// Usual attributes
	[[nodiscard]] virtual double getWeight() const;
	int getAttack() const {
		return static_cast<int>(getDefinition().attribute(ItemAttributeKey::Attack));
	}
	int getArmor() const {
		return static_cast<int>(getDefinition().attribute(ItemAttributeKey::Armor));
	}
	int getDefense() const {
		return static_cast<int>(getDefinition().attribute(ItemAttributeKey::Defense));
	}
	uint16_t getSlotPosition() const {
		return static_cast<uint16_t>(getDefinition().attribute(ItemAttributeKey::SlotPosition));
	}
	uint8_t getWeaponType() const {
		return static_cast<uint8_t>(getDefinition().attribute(ItemAttributeKey::WeaponType));
	}
	uint8_t getClassification() const {
		return static_cast<uint8_t>(getDefinition().attribute(ItemAttributeKey::Classification));
	} // 12.81

	// Item g_settings
	bool canHoldText() const;
	bool canHoldDescription() const;
	bool isReadable() const {
		return getDefinition().hasFlag(ItemFlag::CanReadText);
	}
	bool canWriteText() const {
		return getDefinition().hasFlag(ItemFlag::CanWriteText);
	}
	uint32_t getMaxWriteLength() const {
		return static_cast<uint32_t>(getDefinition().attribute(ItemAttributeKey::MaxTextLen));
	}
	Brush* getBrush() const {
		return getDefinition().editorData().brush;
	}
	GroundBrush* getGroundBrush() const;
	WallBrush* getWallBrush() const;
	DoorBrush* getDoorBrush() const;
	TableBrush* getTableBrush() const;
	CarpetBrush* getCarpetBrush() const;
	Brush* getDoodadBrush() const {
		return getDefinition().editorData().doodad_brush;
	} // This is not necessarily a doodad brush
	RAWBrush* getRAWBrush() const {
		return getDefinition().editorData().raw_brush;
	}
	bool hasCollectionBrush() const {
		return getDefinition().editorData().collection_brush != nullptr;
	}
	uint16_t borderBaseGroundId() const {
		return static_cast<uint16_t>(getDefinition().attribute(ItemAttributeKey::BorderBaseGroundId));
	}
	uint32_t getBorderGroup() const {
		return static_cast<uint32_t>(getDefinition().attribute(ItemAttributeKey::BorderGroup));
	}

	// Drawing related
	uint8_t getMiniMapColor() const;
	int getHeight() const;
	std::pair<int, int> getDrawOffset() const;

	bool hasLight() const;
	SpriteLight getLight() const;

	// Item types
	bool hasProperty(enum ITEMPROPERTY prop) const;
	bool isBlocking() const {
		return getDefinition().hasFlag(ItemFlag::Unpassable);
	}
	bool isStackable() const {
		return getDefinition().hasFlag(ItemFlag::Stackable);
	}
	bool isClientCharged() const {
		return getDefinition().isClientCharged();
	}
	bool isExtraCharged() const {
		return getDefinition().isExtraCharged();
	}
	bool isCharged() const {
		return isClientCharged() || isExtraCharged();
	}
	bool isFluidContainer() const {
		return getDefinition().isFluidContainer();
	}
	bool isAlwaysOnBottom() const {
		return getDefinition().hasFlag(ItemFlag::AlwaysOnBottom);
	}
	int getTopOrder() const {
		return static_cast<int>(getDefinition().attribute(ItemAttributeKey::AlwaysOnTopOrder));
	}
	bool isGroundTile() const {
		return getDefinition().isGroundTile() || (invalidOtbmData && invalidOtbmData->isGroundLike());
	}
	bool isTranslucent() const {
		return getDefinition().hasFlag(ItemFlag::Translucent);
	}
	bool hasLensHelp() const {
		return getDefinition().hasFlag(ItemFlag::LensHelp);
	}
	bool blocksLightFromBelow() const {
		return isGroundTile() && !isTranslucent();
	}
	bool isSplash() const {
		return getDefinition().isSplash();
	}
	bool isMagicField() const {
		return getDefinition().isMagicField();
	}
	bool isTeleport() const {
		return getDefinition().isTeleport();
	}
	bool isNotMoveable() const {
		return !getDefinition().hasFlag(ItemFlag::Moveable);
	}
	bool isMoveable() const {
		return getDefinition().hasFlag(ItemFlag::Moveable);
	}
	bool isPickupable() const {
		return getDefinition().hasFlag(ItemFlag::Pickupable);
	}
	// bool isWeapon() const {return (g_items[id].weaponType != WEAPON_NONE && g_items[id].weaponType != WEAPON_AMMO);}
	// bool isUseable() const {return g_items[id].useable;}
	bool isHangable() const {
		return getDefinition().hasFlag(ItemFlag::IsHangable);
	}
	bool isRoteable() const {
		const auto definition = getDefinition();
		return definition.hasFlag(ItemFlag::Rotatable) && definition.attribute(ItemAttributeKey::RotateTo) != 0;
	}
	void doRotate() {
		if (isRoteable()) {
			id = static_cast<uint16_t>(getDefinition().attribute(ItemAttributeKey::RotateTo));
		}
	}
	bool hasCharges() const {
		return getDefinition().attribute(ItemAttributeKey::Charges) != 0;
	}
	bool isBorder() const {
		return getDefinition().hasFlag(ItemFlag::IsBorder) || (isAlwaysOnBottom() && getTopOrder() == 1);
	}
	bool isOptionalBorder() const {
		return getDefinition().hasFlag(ItemFlag::IsOptionalBorder);
	}
	bool isWall() const {
		return getDefinition().hasFlag(ItemFlag::IsWall);
	}
	bool isDoor() const {
		return getDefinition().isDoor();
	}
	bool isOpen() const {
		return getDefinition().hasFlag(ItemFlag::IsOpen);
	}
	bool isBrushDoor() const {
		return getDefinition().hasFlag(ItemFlag::IsBrushDoor);
	}
	bool isTable() const {
		return getDefinition().hasFlag(ItemFlag::IsTable);
	}
	bool isCarpet() const {
		return getDefinition().hasFlag(ItemFlag::IsCarpet);
	}
	bool isMetaItem() const {
		return getDefinition().isMetaItem();
	}

	// Logic for UI overlays
	virtual bool isLocked() const;

	// Slot-based Item Types
	bool isWeapon() const {
		uint8_t weaponType = getWeaponType();
		return weaponType != WEAPON_NONE && weaponType != WEAPON_AMMO;
	}
	bool isAmmunition() const {
		return getWeaponType() == WEAPON_AMMO;
	}
	bool isWearableEquipment() const { // Determine if the item is wearable piece of armor
		uint16_t slotPosition = getSlotPosition();
		return slotPosition & SLOTP_HEAD || slotPosition & SLOTP_NECKLACE ||
			// slotPosition & SLOTP_BACKPACK || // handled as container in properties window
			slotPosition & SLOTP_ARMOR || slotPosition & SLOTP_LEGS || slotPosition & SLOTP_FEET || slotPosition & SLOTP_RING || (slotPosition & SLOTP_AMMO && !isAmmunition()); // light sources that give stats
	}

	// Wall alignment (vertical, horizontal, pole, corner)
	BorderType getWallAlignment() const;
	// Border aligment (south, west etc.)
	BorderType getBorderAlignment() const;

	// Get the name!
	std::string_view getName() const {
		if (const auto definition = getDefinition()) {
			return definition.name();
		}
		if (invalidOtbmData) {
			return "Invalid Item";
		}
		return {};
	}
	const std::string getFullName() const {
		const auto definition = getDefinition();
		if (!definition) {
			return invalidOtbmData ? std::string("Invalid Item") : std::string();
		}
		return std::string(definition.name()) + std::string(definition.editorSuffix());
	}

	// Selection
	bool isSelected() const {
		return selected;
	}
	void select() {
		selected = true;
	}
	void deselect() {
		selected = false;
	}
	void toggleSelection() {
		selected = !selected;
	}

	// Item properties!
	virtual bool isComplex() const {
		return attributes && attributes->size();
	} // If this item requires full save (not compact)

	// Weight
	bool hasWeight() {
		return isPickupable();
	}
	[[nodiscard]] virtual double getWeight();

	// Subtype (count, fluid, charges)
	int getCount() const;
	uint16_t getSubtype() const;
	void setSubtype(uint16_t n);
	bool hasSubtype() const;

	// Unique ID
	void setUniqueID(uint16_t n);
	uint16_t getUniqueID() const;

	// Action ID
	void setActionID(uint16_t n);
	uint16_t getActionID() const;

	// Tier (12.81)
	void setTier(uint16_t n);
	uint16_t getTier() const;

	// Text
	void setText(const std::string& str);
	std::string_view getText() const;

	// Description
	void setDescription(const std::string& str);
	std::string_view getDescription() const;

protected:
	ServerItemId id; // the same id as in ItemDefinitionStore
	// Subtype is either fluid type, count, subtype or charges
	uint16_t subtype;
	bool selected;
	std::unique_ptr<InvalidOTBMItemData> invalidOtbmData;

private:
	Item& operator=(const Item& i); // Can't copy
	Item(const Item& i); // Can't copy-construct
	Item& operator==(const Item& i); // Can't compare
};

using ItemVector = std::vector<Item*>;
using ItemList = std::list<Item*>;

inline int Item::getCount() const {
	if (isStackable() || isExtraCharged() || isClientCharged()) {
		return subtype;
	}
	return 1;
}

inline uint16_t Item::getUniqueID() const {
	const int32_t* a = getIntegerAttribute(ATTR_UID);
	if (a) {
		return *a;
	}
	return 0;
}

inline uint16_t Item::getActionID() const {
	const int32_t* a = getIntegerAttribute(ATTR_AID);
	if (a) {
		return *a;
	}
	return 0;
}

inline uint16_t Item::getTier() const {
	const int32_t* a = getIntegerAttribute(ATTR_TIER);
	if (a) {
		return *a;
	}
	return 0;
}

inline std::string_view Item::getText() const {
	const std::string* a = getStringAttribute(ATTR_TEXT);
	if (a) {
		return *a;
	}
	return {};
}

inline std::string_view Item::getDescription() const {
	const std::string* a = getStringAttribute(ATTR_DESC);
	if (a) {
		return *a;
	}
	return {};
}

#endif
