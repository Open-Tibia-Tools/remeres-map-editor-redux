//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "item_serialization_otbm.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/tile.h"
#include "io/filehandle.h"
#include <spdlog/spdlog.h>

std::unique_ptr<Item> ItemSerializationOTBM::createFromStream(const IOMap& maphandle, BinaryNode* stream) {
	uint16_t _id;
	if (!stream->getU16(_id)) {
		return nullptr;
	}

	uint8_t _count = 0;
	const auto iType = g_item_definitions.get(_id);
	if (maphandle.version.otbm == MAP_OTBM_1) {
		if (!iType) {
			return nullptr;
		}
		if (iType.hasFlag(ItemFlag::Stackable) || iType.isSplash() || iType.isFluidContainer()) {
			if (!stream->getU8(_count)) {
				return nullptr;
			}
		}
	}
	return Item::Create(_id, _count);
}

bool ItemSerializationOTBM::unserializeItemNode(const IOMap& maphandle, BinaryNode* node, Item& item, int depth) {
	if (depth >= MAX_CONTAINER_DEPTH) {
		return false;
	}

	if (!unserializeAttributes(maphandle, node, item)) {
		return false;
	}

	if (auto container = item.asContainer()) {
		BinaryNode* child = node->getChild();
		if (child) {
			do {
				uint8_t type;
				if (!child->getByte(type)) {
					return false;
				}
				if (type != OTBM_ITEM) {
					return false;
				}

				auto childItem = createFromStream(maphandle, child);
				if (!childItem) {
					return false;
				}

				if (!unserializeItemNode(maphandle, child, *childItem, depth + 1)) {
					return false;
				}

				container->getVector().push_back(std::move(childItem));
			} while (child->advance());
		}
	}
	return true;
}

bool ItemSerializationOTBM::unserializeAttributes(const IOMap& maphandle, BinaryNode* stream, Item& item) {
	uint8_t attribute;
	while (stream->getU8(attribute)) {
		if (attribute == OTBM_ATTR_ATTRIBUTE_MAP) {
			if (!item.unserializeAttributeMap(maphandle, stream)) {
				spdlog::warn("Failed to read attribute map for item id={} ('{}')", item.getID(), item.getName());
				return false;
			}
			const int32_t* serverIdAttr = item.getIntegerAttribute("serverId");
			if (serverIdAttr && *serverIdAttr > 0) {
				item.setID(static_cast<ServerItemId>(*serverIdAttr));
			}
		} else if (!readAttribute(maphandle, static_cast<OTBM_ItemAttribute>(attribute), stream, item)) {
			// Unrecognized or junk byte (e.g. trailing subtype written by some map editors).
			// Stop reading attributes but don't fail â€” keep the item as-is.
			break;
		}
	}
	return true;
}

bool ItemSerializationOTBM::readAttribute(const IOMap& maphandle, OTBM_ItemAttribute attr, BinaryNode* stream, Item& item) {
	switch (attr) {
		case OTBM_ATTR_COUNT: {
			uint8_t subtype;
			if (!stream->getU8(subtype)) {
				return false;
			}
			item.setSubtype(subtype);
			break;
		}
		case OTBM_ATTR_ACTION_ID: {
			uint16_t aid;
			if (!stream->getU16(aid)) {
				return false;
			}
			item.setActionID(aid);
			break;
		}
		case OTBM_ATTR_UNIQUE_ID: {
			uint16_t uid;
			if (!stream->getU16(uid)) {
				return false;
			}
			item.setUniqueID(uid);
			break;
		}
		case OTBM_ATTR_CHARGES: {
			uint16_t charges;
			if (!stream->getU16(charges)) {
				return false;
			}
			item.setSubtype(charges);
			break;
		}
		case OTBM_ATTR_TEXT: {
			std::string text;
			if (!stream->getString(text)) {
				return false;
			}
			item.setText(text);
			break;
		}
		case OTBM_ATTR_DESC: {
			std::string desc;
			if (!stream->getString(desc)) {
				return false;
			}
			item.setDescription(desc);
			break;
		}
		case OTBM_ATTR_TIER: {
			uint8_t tier;
			if (!stream->getU8(tier)) {
				return false;
			}
			item.setTier(static_cast<uint16_t>(tier));
			break;
		}
		case OTBM_ATTR_TELE_DEST: {
			if (auto tele = item.asTeleport()) {
				uint16_t x, y;
				uint8_t z;
				if (!stream->getU16(x) || !stream->getU16(y) || !stream->getU8(z)) {
					return false;
				}
				tele->setDestination(Position(x, y, z));
			} else {
				return stream->skip(5);
			}
			break;
		}
		case OTBM_ATTR_HOUSEDOORID: {
			if (auto door = item.asDoor()) {
				uint8_t id;
				if (!stream->getU8(id)) {
					return false;
				}
				door->setDoorID(id);
			} else {
				return stream->skip(1);
			}
			break;
		}
		case OTBM_ATTR_DEPOT_ID: {
			if (auto depot = item.asDepot()) {
				uint16_t id;
				if (!stream->getU16(id)) {
					return false;
				}
				if (id > 255) {
					spdlog::error("ItemSerializationOTBM: Depot ID too large: {}", id);
					return false;
				}
				depot->setDepotID(static_cast<uint8_t>(id));
			} else {
				return stream->skip(2);
			}
			break;
		}
		case OTBM_ATTR_PODIUMOUTFIT: {
			if (auto podium = item.asPodium()) {
				uint8_t flags;
				uint8_t direction;
				if (!stream->getU8(flags) || !stream->getU8(direction)) {
					return false;
				}

				uint16_t lookType, lookMount;
				uint8_t lookHead, lookBody, lookLegs, lookFeet, lookAddon;
				uint8_t lookMountHead, lookMountBody, lookMountLegs, lookMountFeet;

				if (!stream->getU16(lookType) || !stream->getU8(lookHead) || !stream->getU8(lookBody) || !stream->getU8(lookLegs) || !stream->getU8(lookFeet) || !stream->getU8(lookAddon) || !stream->getU16(lookMount) || !stream->getU8(lookMountHead) || !stream->getU8(lookMountBody) || !stream->getU8(lookMountLegs) || !stream->getU8(lookMountFeet)) {
					return false;
				}

				Outfit newOutfit;
				newOutfit.lookType = lookType;
				newOutfit.lookHead = lookHead;
				newOutfit.lookBody = lookBody;
				newOutfit.lookLegs = lookLegs;
				newOutfit.lookFeet = lookFeet;
				newOutfit.lookAddon = lookAddon;
				newOutfit.lookMount = lookMount;
				newOutfit.lookMountHead = lookMountHead;
				newOutfit.lookMountBody = lookMountBody;
				newOutfit.lookMountLegs = lookMountLegs;
				newOutfit.lookMountFeet = lookMountFeet;

				podium->setShowOutfit((flags & PODIUM_SHOW_OUTFIT) != 0);
				podium->setShowMount((flags & PODIUM_SHOW_MOUNT) != 0);
				podium->setShowPlatform((flags & PODIUM_SHOW_PLATFORM) != 0);
				podium->setDirection(direction);
				podium->setOutfit(newOutfit);
			} else {
				return stream->skip(15);
			}
			break;
		}
		default:
			return false;
	}
	return true;
}

bool ItemSerializationOTBM::serializeItemNode(const IOMap& maphandle, NodeFileWriteHandle& f, const Item& item) {
	f.addNode(OTBM_ITEM);
	f.addU16(static_cast<uint16_t>(item.getID() > 0xFFFF ? 0 : item.getID()));
	if (maphandle.version.otbm == MAP_OTBM_1) {
		const auto iType = item.getDefinition();
		if (iType.hasFlag(ItemFlag::Stackable) || iType.isSplash() || iType.isFluidContainer()) {
			f.addU8(item.getSubtype());
		}
	}
	serializeItemAttributes(maphandle, f, item);

	if (auto container = item.asContainer()) {
		for (const auto& child : container->getVector()) {
			if (!serializeItemNode(maphandle, f, *child)) {
				return false;
			}
		}
	}
	f.endNode();
	return true;
}

void ItemSerializationOTBM::serializeItemCompact(const IOMap& /*maphandle*/, NodeFileWriteHandle& f, const Item& item) {
	f.addU16(static_cast<uint16_t>(item.getID() > 0xFFFF ? 0 : item.getID()));
}

void ItemSerializationOTBM::serializeItemAttributes(const IOMap& maphandle, NodeFileWriteHandle& f, const Item& item) {
	if (maphandle.version.otbm >= MAP_OTBM_2) {
		const auto iType = item.getDefinition();
		if (iType.hasFlag(ItemFlag::Stackable) || iType.isSplash() || iType.isFluidContainer()) {
			f.addU8(OTBM_ATTR_COUNT);
			f.addU8(item.getSubtype());
		}
	}

	if (maphandle.version.otbm >= MAP_OTBM_4) {
		if (item.getID() > 0xFFFF && !item.hasIntegerAttribute("serverId")) {
			const_cast<Item&>(item).setAttribute("serverId", static_cast<int32_t>(item.getID()));
		}
		if (item.hasAttributes()) {
			f.addU8(OTBM_ATTR_ATTRIBUTE_MAP);
			item.serializeAttributeMap(maphandle, f);
		}
	} else {
		if (g_item_definitions.MinorVersion >= CLIENT_VERSION_820 && item.isCharged()) {
			f.addU8(OTBM_ATTR_CHARGES);
			f.addU16(item.getSubtype());
		}

		uint16_t actionId = item.getActionID();
		if (actionId > 0) {
			f.addU8(OTBM_ATTR_ACTION_ID);
			f.addU16(actionId);
		}

		uint16_t uniqueId = item.getUniqueID();
		if (uniqueId > 0) {
			f.addU8(OTBM_ATTR_UNIQUE_ID);
			f.addU16(uniqueId);
		}

		std::string text(item.getText());
		if (!text.empty()) {
			f.addU8(OTBM_ATTR_TEXT);
			f.addString(text);
		}

		std::string description(item.getDescription());
		if (!description.empty()) {
			f.addU8(OTBM_ATTR_DESC);
			f.addString(description);
		}

		uint16_t tier = item.getTier();
		if (tier > 0) {
			if (tier <= 0xFF) {
				f.addU8(OTBM_ATTR_TIER);
				f.addU8(static_cast<uint8_t>(tier));
			} else {
				spdlog::warn("ItemSerializationOTBM: Item '{}' has tier {} which is too large for uint8_t, truncating to 255", item.getName(), tier);
				f.addU8(OTBM_ATTR_TIER);
				f.addU8(0xFF);
			}
		}
	}

	// Specific attributes
	if (auto tele = item.asTeleport()) {
		f.addU8(OTBM_ATTR_TELE_DEST);
		f.addU16(tele->getDestination().x);
		f.addU16(tele->getDestination().y);
		f.addU8(tele->getDestination().z);
	} else if (auto door = item.asDoor()) {
		if (door->getDoorID() != 0) {
			f.addU8(OTBM_ATTR_HOUSEDOORID);
			f.addU8(door->getDoorID());
		}
	} else if (auto depot = item.asDepot()) {
		if (depot->getDepotID() != 0) {
			f.addU8(OTBM_ATTR_DEPOT_ID);
			f.addU16(depot->getDepotID());
		}
	} else if (auto podium = item.asPodium()) {
		uint8_t flags = 0;
		if (podium->getShowOutfit()) {
			flags |= PODIUM_SHOW_OUTFIT;
		}
		if (podium->getShowMount()) {
			flags |= PODIUM_SHOW_MOUNT;
		}
		if (podium->getShowPlatform()) {
			flags |= PODIUM_SHOW_PLATFORM;
		}

		const Outfit& outfit = podium->getOutfit();
		f.addU8(OTBM_ATTR_PODIUMOUTFIT);
		f.addU8(flags);
		f.addU8(podium->getDirection());

		f.addU16(outfit.lookType);
		f.addU8(outfit.lookHead);
		f.addU8(outfit.lookBody);
		f.addU8(outfit.lookLegs);
		f.addU8(outfit.lookFeet);
		f.addU8(outfit.lookAddon);

		f.addU16(outfit.lookMount);
		f.addU8(outfit.lookMountHead);
		f.addU8(outfit.lookMountBody);
		f.addU8(outfit.lookMountLegs);
		f.addU8(outfit.lookMountFeet);
	}
}
