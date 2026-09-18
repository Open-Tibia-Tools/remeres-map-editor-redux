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
#include "rendering/core/graphics.h"
#include "ui/gui.h"
#include "map/tile.h"
#include "game/complexitem.h"
#include "io/iomap.h"
#include "game/item.h"
#include "brushes/managers/brush_manager.h"
#include "item_definitions/core/item_definition_store.h"
#include <unordered_map>

#include "brushes/ground/ground_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/wall/wall_brush.h"
#include <array>
#include <string_view>

std::unique_ptr<Item> Item::Create(ServerItemId _type, uint16_t _subtype /*= 0xFFFF*/) {
	if (_type == 0) {
		return nullptr;
	}
	std::unique_ptr<Item> newItem;

	const auto definition = g_item_definitions.get(_type);

	if (definition) {
		if (definition.isDepot()) {
			newItem = std::make_unique<Depot>(_type);
		} else if (definition.isContainer()) {
			newItem = std::make_unique<Container>(_type);
		} else if (definition.isTeleport()) {
			newItem = std::make_unique<Teleport>(_type);
		} else if (definition.isDoor()) {
			newItem = std::make_unique<Door>(_type);
		} else if (definition.isPodium()) {
			newItem = std::make_unique<Podium>(_type);
		} else if (_subtype == 0xFFFF) {
			if (definition.isFluidContainer()) {
				newItem = std::make_unique<Item>(_type, LIQUID_NONE);
			} else if (definition.isSplash()) {
				newItem = std::make_unique<Item>(_type, LIQUID_WATER);
			} else if (definition.attribute(ItemAttributeKey::Charges) > 0) {
				newItem = std::make_unique<Item>(_type, static_cast<uint16_t>(definition.attribute(ItemAttributeKey::Charges)));
			} else {
				newItem = std::make_unique<Item>(_type, 1);
			}
		} else {
			newItem = std::make_unique<Item>(_type, _subtype);
		}
	} else {
		newItem = std::make_unique<Item>(_type, _subtype);
	}

	return newItem;
}

Item::Item(ServerItemId _type, unsigned short _count) :
	id(_type),
	subtype(1),
	selected(false) {
	if (hasSubtype()) {
		subtype = _count;
	}
}

Item::~Item() {
	////
}

std::unique_ptr<Item> Item::deepCopy() const {
	std::unique_ptr<Item> copy = Create(id, subtype);
	if (copy) {
		copy->selected = selected;
		if (invalidOtbmData) {
			copy->invalidOtbmData = std::make_unique<InvalidOTBMItemData>(*invalidOtbmData);
		}
		if (attributes) {
			copy->attributes = newd ItemAttributeMap(*attributes);
		}
	}
	return copy;
}

uint32_t Item::memsize() const {
	uint32_t mem = sizeof(*this);
	if (invalidOtbmData) {
		mem += static_cast<uint32_t>(invalidOtbmData->rawInlineBytes.capacity());
		if (invalidOtbmData->rawNode) {
			std::vector<const PreservedOTBMNode*> stack { &*invalidOtbmData->rawNode };
			while (!stack.empty()) {
				const PreservedOTBMNode* node = stack.back();
				stack.pop_back();
				mem += static_cast<uint32_t>(node->rawPayload.capacity());
				mem += static_cast<uint32_t>(node->children.capacity() * sizeof(PreservedOTBMNode));
				for (const auto& child : node->children) {
					stack.push_back(&child);
				}
			}
		}
	}
	return mem;
}

GameSprite* Item::getSprite() const {
	return g_gui.gfx.getGameSprite(getClientID());
}

void Item::setSubtype(uint16_t n) {
	subtype = n;
}

bool Item::hasSubtype() const {
	const auto definition = getDefinition();
	return definition && (definition.isFluidContainer() || definition.isSplash() || isCharged() || definition.hasFlag(ItemFlag::Stackable) || definition.attribute(ItemAttributeKey::Charges) != 0);
}

uint16_t Item::getSubtype() const {
	if (hasSubtype()) {
		return subtype;
	}
	return 0;
}

bool Item::hasProperty(enum ITEMPROPERTY prop) const {
	const auto definition = getDefinition();
	if (!definition) {
		return false;
	}
	switch (prop) {
		case BLOCKSOLID:
			if (definition.hasFlag(ItemFlag::Unpassable)) {
				return true;
			}
			break;

		case MOVEABLE:
			if (definition.hasFlag(ItemFlag::Moveable) && getUniqueID() == 0) {
				return true;
			}
			break;
			/*
					case HASHEIGHT:
						if(it.height != 0 )
							return true;
						break;
			*/
		case BLOCKPROJECTILE:
			if (definition.hasFlag(ItemFlag::BlockMissiles)) {
				return true;
			}
			break;

		case BLOCKPATHFIND:
			if (definition.hasFlag(ItemFlag::BlockPathfinder)) {
				return true;
			}
			break;

		case HOOK_SOUTH:
			if (definition.hasFlag(ItemFlag::HookSouth)) {
				return true;
			}
			break;

		case HOOK_EAST:
			if (definition.hasFlag(ItemFlag::HookEast)) {
				return true;
			}
			break;

		case BLOCKINGANDNOTMOVEABLE:
			if (definition.hasFlag(ItemFlag::Unpassable) && (!definition.hasFlag(ItemFlag::Moveable) || getUniqueID() != 0)) {
				return true;
			}
			break;

		default:
			return false;
	}
	return false;
}

bool Item::isLocked() const {
	if (getActionID() != 0 || getUniqueID() != 0) {
		return true;
	}
	if (const Door* door = asDoor()) {
		return door->getDoorID() != 0;
	}
	return false;
}

std::pair<int, int> Item::getDrawOffset() const {
	if (GameSprite* sprite = getSprite()) {
		return sprite->getDrawOffset();
	}
	return std::make_pair(0, 0);
}

bool Item::hasLight() const {
	if (auto* sprite = getSprite()) {
		return sprite->hasLight();
	}
	return false;
}

SpriteLight Item::getLight() const {
	if (auto* sprite = getSprite()) {
		return sprite->getLight();
	}
	return SpriteLight { 0, 0 };
}

double Item::getWeight() const {
	const auto definition = getDefinition();
	if (!definition) {
		return 0.0;
	}
	const double base_weight = static_cast<double>(definition.attribute(ItemAttributeKey::Weight)) / 1000.0;
	if (definition.hasFlag(ItemFlag::Stackable)) {
		return base_weight * std::max(1, static_cast<int>(subtype));
	}

	return base_weight;
}

int Item::getHeight() const {
	if (!getDefinition().hasFlag(ItemFlag::HasElevation)) {
		return 0;
	}

	if (const GameSprite* sprite = getSprite()) {
		const int draw_height = sprite->getDrawHeight();
		if (draw_height > 0) {
			return draw_height;
		}
	}

	// Older DAT formats may mark elevation without storing an explicit draw height.
	return 8;
}

void Item::setUniqueID(unsigned short n) {
	setAttribute(ATTR_UID, n);
}

void Item::setActionID(unsigned short n) {
	setAttribute(ATTR_AID, n);
}

void Item::setText(const std::string& str) {
	setAttribute(ATTR_TEXT, str);
}

void Item::setDescription(const std::string& str) {
	setAttribute(ATTR_DESC, str);
}

void Item::setTier(unsigned short n) {
	setAttribute(ATTR_TIER, n);
}

double Item::getWeight() {
	return const_cast<const Item*>(this)->getWeight();
}

bool Item::canHoldText() const {
	return isReadable() || canWriteText();
}

bool Item::canHoldDescription() const {
	return getDefinition().hasFlag(ItemFlag::AllowDistRead);
}

uint8_t Item::getMiniMapColor() const {
	GameSprite* spr = getSprite();
	if (spr) {
		return spr->getMiniMapColor();
	}
	return 0;
}

GroundBrush* Item::getGroundBrush() const {
	const auto definition = getDefinition();
	if (definition.isGroundTile() && definition.editorData().brush && definition.editorData().brush->is<GroundBrush>()) {
		return definition.editorData().brush->as<GroundBrush>();
	}
	return nullptr;
}

TableBrush* Item::getTableBrush() const {
	const auto definition = getDefinition();
	if (definition.hasFlag(ItemFlag::IsTable) && definition.editorData().brush && definition.editorData().brush->is<TableBrush>()) {
		return definition.editorData().brush->as<TableBrush>();
	}
	return nullptr;
}

CarpetBrush* Item::getCarpetBrush() const {
	const auto definition = getDefinition();
	if (definition.hasFlag(ItemFlag::IsCarpet) && definition.editorData().brush && definition.editorData().brush->is<CarpetBrush>()) {
		return definition.editorData().brush->as<CarpetBrush>();
	}
	return nullptr;
}

DoorBrush* Item::getDoorBrush() const {
	const auto definition = getDefinition();
	if (!definition.hasFlag(ItemFlag::IsWall) || !definition.hasFlag(ItemFlag::IsBrushDoor) || !definition.editorData().brush || !definition.editorData().brush->is<WallBrush>()) {
		return nullptr;
	}

	DoorType door_type = definition.editorData().brush->as<WallBrush>()->getDoorTypeFromID(id);
	DoorBrush* door_brush = nullptr;
	// Quite a horrible dependency on a global here, meh.
	switch (door_type) {
		case WALL_DOOR_NORMAL: {
			door_brush = g_brush_manager.normal_door_brush;
			break;
		}
		case WALL_DOOR_LOCKED: {
			door_brush = g_brush_manager.locked_door_brush;
			break;
		}
		case WALL_DOOR_QUEST: {
			door_brush = g_brush_manager.quest_door_brush;
			break;
		}
		case WALL_DOOR_MAGIC: {
			door_brush = g_brush_manager.magic_door_brush;
			break;
		}
		case WALL_DOOR_NORMAL_ALT: {
			door_brush = g_brush_manager.normal_door_alt_brush;
			break;
		}
		case WALL_ARCHWAY: {
			door_brush = g_brush_manager.archway_door_brush;
			break;
		}
		case WALL_WINDOW: {
			door_brush = g_brush_manager.window_door_brush;
			break;
		}
		case WALL_HATCH_WINDOW: {
			door_brush = g_brush_manager.hatch_door_brush;
			break;
		}
		default: {
			break;
		}
	}
	return door_brush;
}

WallBrush* Item::getWallBrush() const {
	const auto definition = getDefinition();
	if (definition.hasFlag(ItemFlag::IsWall) && definition.editorData().brush && definition.editorData().brush->is<WallBrush>()) {
		return definition.editorData().brush->as<WallBrush>();
	}
	return nullptr;
}

BorderType Item::getWallAlignment() const {
	const auto definition = getDefinition();
	if (!definition.hasFlag(ItemFlag::IsWall)) {
		return BORDER_NONE;
	}
	return static_cast<BorderType>(definition.attribute(ItemAttributeKey::BorderAlignment));
}

BorderType Item::getBorderAlignment() const {
	return static_cast<BorderType>(getDefinition().attribute(ItemAttributeKey::BorderAlignment));
}

// ============================================================================
// Static conversions

std::string_view Item::LiquidID2Name(uint16_t id) {
	static constexpr std::array<std::string_view, 44> liquid_names = {
		"None", // LIQUID_NONE (0)
		"Water", // LIQUID_WATER (1)
		"Blood", // LIQUID_BLOOD (2)
		"Beer", // LIQUID_BEER (3)
		"Slime", // LIQUID_SLIME (4)
		"Lemonade", // LIQUID_LEMONADE (5)
		"Milk", // LIQUID_MILK (6)
		"Manafluid", // LIQUID_MANAFLUID (7)
		"Ink", // LIQUID_INK (8)
		"Water", // LIQUID_WATER2 (9)
		"Lifefluid", // LIQUID_LIFEFLUID (10)
		"Oil", // LIQUID_OIL (11)
		"Slime", // LIQUID_SLIME2 (12)
		"Urine", // LIQUID_URINE (13)
		"Coconut Milk", // LIQUID_COCONUT_MILK (14)
		"Wine", // LIQUID_WINE (15)
		"Unknown", // 16
		"Unknown", // 17
		"Unknown", // 18
		"Mud", // LIQUID_MUD (19)
		"Unknown", // 20
		"Fruit Juice", // LIQUID_FRUIT_JUICE (21)
		"Unknown", // 22
		"Unknown", // 23
		"Unknown", // 24
		"Unknown", // 25
		"Lava", // LIQUID_LAVA (26)
		"Rum", // LIQUID_RUM (27)
		"Swamp", // LIQUID_SWAMP (28)
		"Unknown", // 29
		"Unknown", // 30
		"Unknown", // 31
		"Unknown", // 32
		"Unknown", // 33
		"Unknown", // 34
		"Tea", // LIQUID_TEA (35)
		"Unknown", // 36
		"Unknown", // 37
		"Unknown", // 38
		"Unknown", // 39
		"Unknown", // 40
		"Unknown", // 41
		"Unknown", // 42
		"Mead" // LIQUID_MEAD (43)
	};

	if (id < liquid_names.size()) {
		return liquid_names[id];
	}
	return "Unknown";
}

uint16_t Item::LiquidName2ID(std::string liquid) {
	to_lower_str(liquid);
	static const std::unordered_map<std::string, uint16_t> liquid_map = {
		{ "none", LIQUID_NONE },
		{ "water", LIQUID_WATER },
		{ "blood", LIQUID_BLOOD },
		{ "beer", LIQUID_BEER },
		{ "slime", LIQUID_SLIME },
		{ "lemonade", LIQUID_LEMONADE },
		{ "milk", LIQUID_MILK },
		{ "manafluid", LIQUID_MANAFLUID },
		{ "lifefluid", LIQUID_LIFEFLUID },
		{ "oil", LIQUID_OIL },
		{ "urine", LIQUID_URINE },
		{ "coconut milk", LIQUID_COCONUT_MILK },
		{ "wine", LIQUID_WINE },
		{ "mud", LIQUID_MUD },
		{ "fruit juice", LIQUID_FRUIT_JUICE },
		{ "lava", LIQUID_LAVA },
		{ "rum", LIQUID_RUM },
		{ "swamp", LIQUID_SWAMP },
		{ "ink", LIQUID_INK },
		{ "tea", LIQUID_TEA },
		{ "mead", LIQUID_MEAD }
	};

	auto it = liquid_map.find(liquid);
	if (it != liquid_map.end()) {
		return it->second;
	}
	return LIQUID_NONE;
}

// ============================================================================
// XML Saving & loading

std::unique_ptr<Item> Item::Create(pugi::xml_node xml) {
	pugi::xml_attribute attribute;

	int16_t id = 0;
	if ((attribute = xml.attribute("id"))) {
		id = attribute.as_ushort();
	}

	int16_t count = 1;
	if ((attribute = xml.attribute("count")) || (attribute = xml.attribute("subtype"))) {
		count = attribute.as_ushort();
	}

	return Create(id, count);
}
