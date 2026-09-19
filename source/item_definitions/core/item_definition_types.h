#ifndef RME_ITEM_DEFINITION_TYPES_H_
#define RME_ITEM_DEFINITION_TYPES_H_

#include "brushes/brush_enums.h"

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>

class Brush;
class RAWBrush;

using ServerItemId = uint32_t;
using ClientItemId = uint32_t;
using DefinitionId = uint32_t;

enum ItemGroup_t {
	ITEM_GROUP_NONE = 0,
	ITEM_GROUP_GROUND,
	ITEM_GROUP_CONTAINER,
	ITEM_GROUP_WEAPON,
	ITEM_GROUP_AMMUNITION,
	ITEM_GROUP_ARMOR,
	ITEM_GROUP_RUNE,
	ITEM_GROUP_TELEPORT,
	ITEM_GROUP_MAGICFIELD,
	ITEM_GROUP_WRITEABLE,
	ITEM_GROUP_KEY,
	ITEM_GROUP_SPLASH,
	ITEM_GROUP_FLUID,
	ITEM_GROUP_DOOR,
	ITEM_GROUP_DEPRECATED,
	ITEM_GROUP_PODIUM,
	ITEM_GROUP_LAST
};

enum ItemTypes_t {
	ITEM_TYPE_NONE = 0,
	ITEM_TYPE_DEPOT,
	ITEM_TYPE_MAILBOX,
	ITEM_TYPE_TRASHHOLDER,
	ITEM_TYPE_CONTAINER,
	ITEM_TYPE_DOOR,
	ITEM_TYPE_MAGICFIELD,
	ITEM_TYPE_TELEPORT,
	ITEM_TYPE_BED,
	ITEM_TYPE_KEY,
	ITEM_TYPE_PODIUM,
	ITEM_TYPE_LAST
};

enum SlotPositionBits : uint32_t {
	SLOTP_WHEREEVER = 0xFFFFFFFF,
	SLOTP_HEAD = 1 << 0,
	SLOTP_NECKLACE = 1 << 1,
	SLOTP_BACKPACK = 1 << 2,
	SLOTP_ARMOR = 1 << 3,
	SLOTP_RIGHT = 1 << 4,
	SLOTP_LEFT = 1 << 5,
	SLOTP_LEGS = 1 << 6,
	SLOTP_FEET = 1 << 7,
	SLOTP_RING = 1 << 8,
	SLOTP_AMMO = 1 << 9,
	SLOTP_DEPOT = 1 << 10,
	SLOTP_TWO_HAND = 1 << 11,
	SLOTP_HAND = (SLOTP_LEFT | SLOTP_RIGHT)
};

enum WeaponType_t : uint8_t {
	WEAPON_NONE,
	WEAPON_SWORD,
	WEAPON_CLUB,
	WEAPON_AXE,
	WEAPON_SHIELD,
	WEAPON_DISTANCE,
	WEAPON_WAND,
	WEAPON_AMMO,
};

enum class ItemDefinitionMode : uint8_t {
	DatOtb,
	DatOnly,
	DatSrv,
	ProtobufOtb,
	ProtobufOnly
};

inline std::string toString(ItemDefinitionMode mode) {
	switch (mode) {
		case ItemDefinitionMode::DatOtb:
			return "dat_otb";
		case ItemDefinitionMode::DatOnly:
			return "dat_only";
		case ItemDefinitionMode::DatSrv:
			return "dat_srv";
		case ItemDefinitionMode::ProtobufOtb:
			return "protobuf_otb";
		case ItemDefinitionMode::ProtobufOnly:
			return "protobuf_only";
	}
	return "dat_otb";
}

inline std::optional<ItemDefinitionMode> parseItemDefinitionMode(std::string value) {
	for (char& ch : value) {
		ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
	}

	if (value == "dat_otb" || value == "otb") {
		return ItemDefinitionMode::DatOtb;
	}
	if (value == "dat_only") {
		return ItemDefinitionMode::DatOnly;
	}
	if (value == "dat_srv") {
		return ItemDefinitionMode::DatSrv;
	}
	if (value == "protobuf" || value == "protobuf_otb") {
		return ItemDefinitionMode::ProtobufOtb;
	}
	if (value == "protobuf_only") {
		return ItemDefinitionMode::ProtobufOnly;
	}
	return std::nullopt;
}

enum class ItemFlag : uint8_t {
	MetaItem,
	ClientChargeable,
	ExtraChargeable,
	ForceUse,
	MultiUse,
	IgnoreLook,
	IsHangable,
	HookEast,
	HookSouth,
	CanReadText,
	CanWriteText,
	AllowDistRead,
	Replaceable,
	Decays,
	Stackable,
	Moveable,
	AlwaysOnBottom,
	Pickupable,
	Rotatable,
	IsBorder,
	IsOptionalBorder,
	IsWall,
	IsBrushDoor,
	IsOpen,
	IsLocked,
	IsTable,
	IsCarpet,
	FloorChangeDown,
	FloorChangeNorth,
	FloorChangeSouth,
	FloorChangeEast,
	FloorChangeWest,
	FloorChange,
	Unpassable,
	BlockPickupable,
	BlockMissiles,
	BlockPathfinder,
	HasElevation,
	FullTile,
	Translucent,
	LensHelp,
	NoMoveAnimation,
	AnimateAlways,
	DontHide,
	Corpse,
	Wrappable,
	Unwrappable,
	TopEffect,
	Ammo,
	Reportable,
	Tooltipable,
	WallHateMe,
	HasRaw,
	InOtherTileset,
	Usable,
	Count,
};

enum class ItemAttributeKey : uint8_t {
	Volume,
	MaxTextLen,
	SlotPosition,
	WeaponType,
	Classification,
	BorderBaseGroundId,
	BorderGroup,
	Weight,
	Attack,
	Defense,
	Armor,
	Charges,
	RotateTo,
	WaySpeed,
	AlwaysOnTopOrder,
	BorderAlignment,
};

struct ItemEditorData {
	Brush* brush = nullptr;
	Brush* doodad_brush = nullptr;
	Brush* collection_brush = nullptr;
	RAWBrush* raw_brush = nullptr;
};

#endif
