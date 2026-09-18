#include "item_definitions/formats/xml/xml_item_parser.h"

#include "ext/pugixml.hpp"
#include "io/xml_file_loader.h"
#include "util/common.h"

#include <string_view>
#include <unordered_map>

namespace {
	constexpr uint64_t flagMask(ItemFlag flag) {
		return uint64_t { 1 } << static_cast<uint8_t>(flag);
	}

	void parseType(XmlItemFragment& fragment, std::string value) {
		to_lower_str(value);
		if (value == "depot") fragment.type = ITEM_TYPE_DEPOT;
		else if (value == "mailbox") fragment.type = ITEM_TYPE_MAILBOX;
		else if (value == "trashholder") fragment.type = ITEM_TYPE_TRASHHOLDER;
		else if (value == "container") fragment.type = ITEM_TYPE_CONTAINER;
		else if (value == "door") fragment.type = ITEM_TYPE_DOOR;
		else if (value == "magicfield") { fragment.group = ITEM_GROUP_MAGICFIELD; fragment.type = ITEM_TYPE_MAGICFIELD; }
		else if (value == "teleport") fragment.type = ITEM_TYPE_TELEPORT;
		else if (value == "bed") fragment.type = ITEM_TYPE_BED;
		else if (value == "key") fragment.type = ITEM_TYPE_KEY;
		else if (value == "podium") fragment.type = ITEM_TYPE_PODIUM;
	}

	void parseSlot(XmlItemFragment& fragment, std::string value) {
		to_lower_str(value);
		uint16_t slot = fragment.slot_position.value_or(0);
		if (value == "head") slot |= SLOTP_HEAD;
		else if (value == "body") slot |= SLOTP_ARMOR;
		else if (value == "legs") slot |= SLOTP_LEGS;
		else if (value == "feet") slot |= SLOTP_FEET;
		else if (value == "backpack") slot |= SLOTP_BACKPACK;
		else if (value == "two-handed") slot |= SLOTP_HAND | SLOTP_TWO_HAND;
		else if (value == "right-hand") { slot |= SLOTP_HAND | SLOTP_RIGHT; slot &= ~SLOTP_LEFT; }
		else if (value == "left-hand") { slot |= SLOTP_HAND | SLOTP_LEFT; slot &= ~SLOTP_RIGHT; }
		else if (value == "necklace") slot |= SLOTP_NECKLACE;
		else if (value == "ring") slot |= SLOTP_RING;
		else if (value == "ammo") slot |= SLOTP_AMMO;
		else if (value == "hand") slot |= SLOTP_HAND;
		fragment.slot_position = slot;
	}

	void parseWeapon(XmlItemFragment& fragment, std::string value) {
		to_lower_str(value);
		if (value == "sword") fragment.weapon_type = WEAPON_SWORD;
		else if (value == "club") fragment.weapon_type = WEAPON_CLUB;
		else if (value == "axe") fragment.weapon_type = WEAPON_AXE;
		else if (value == "shield") fragment.weapon_type = WEAPON_SHIELD;
		else if (value == "distance") fragment.weapon_type = WEAPON_DISTANCE;
		else if (value == "wand") fragment.weapon_type = WEAPON_WAND;
		else if (value == "ammunition") fragment.weapon_type = WEAPON_AMMO;
	}

	void parseFloorChange(XmlItemFragment& fragment, std::string value) {
		to_lower_str(value);
		fragment.flags |= flagMask(ItemFlag::FloorChange);
		if (value == "down") fragment.flags |= flagMask(ItemFlag::FloorChangeDown);
		else if (value == "north") fragment.flags |= flagMask(ItemFlag::FloorChangeNorth);
		else if (value == "south") fragment.flags |= flagMask(ItemFlag::FloorChangeSouth);
		else if (value == "west") fragment.flags |= flagMask(ItemFlag::FloorChangeWest);
		else if (value == "east") fragment.flags |= flagMask(ItemFlag::FloorChangeEast);
	}

	void applyAttribute(XmlItemFragment& fragment, pugi::xml_node attribute_node) {
		const std::string key = as_lower_str(attribute_node.attribute("key").as_string());
		const std::string value = attribute_node.attribute("value").as_string();

		if (key == "type") parseType(fragment, value);
		else if (key == "name") fragment.name = value;
		else if (key == "description") fragment.description = value;
		else if (key == "speed") fragment.way_speed = attribute_node.attribute("value").as_ushort();
		else if (key == "weight") fragment.weight = attribute_node.attribute("value").as_int() / 100.f;
		else if (key == "attack") fragment.attack = attribute_node.attribute("value").as_int();
		else if (key == "armor") fragment.armor = attribute_node.attribute("value").as_int();
		else if (key == "defense") fragment.defense = attribute_node.attribute("value").as_int();
		else if (key == "slottype") parseSlot(fragment, value);
		else if (key == "weapontype") parseWeapon(fragment, value);
		else if (key == "rotateto") fragment.rotate_to = attribute_node.attribute("value").as_ushort();
		else if (key == "containersize") fragment.volume = attribute_node.attribute("value").as_ushort();
		else if (key == "readable" && attribute_node.attribute("value").as_bool()) fragment.flags |= flagMask(ItemFlag::CanReadText);
		else if (key == "writeable" && attribute_node.attribute("value").as_bool()) fragment.flags |= flagMask(ItemFlag::CanReadText) | flagMask(ItemFlag::CanWriteText);
		else if (key == "maxtextlen" || key == "maxtextlength") fragment.max_text_len = attribute_node.attribute("value").as_ushort();
		else if (key == "allowdistread" && attribute_node.attribute("value").as_bool()) fragment.flags |= flagMask(ItemFlag::AllowDistRead);
		else if (key == "forceuse" && attribute_node.attribute("value").as_bool()) fragment.flags |= flagMask(ItemFlag::ForceUse);
		else if (key == "multiuse" && attribute_node.attribute("value").as_bool()) fragment.flags |= flagMask(ItemFlag::MultiUse);
		else if (key == "usable" && attribute_node.attribute("value").as_bool()) fragment.flags |= flagMask(ItemFlag::ForceUse);
		else if ((key == "fulltile" || key == "fullground") && attribute_node.attribute("value").as_bool()) fragment.flags |= flagMask(ItemFlag::FullTile);
		else if (key == "charges") { fragment.charges = attribute_node.attribute("value").as_uint(); fragment.flags |= flagMask(ItemFlag::ExtraChargeable); }
		else if ((key == "decayto" || key == "decaytime" || key == "decay") && !value.empty()) fragment.flags |= flagMask(ItemFlag::Decays);
		else if (key == "floorchange") parseFloorChange(fragment, value);
	}
}

namespace {

[[nodiscard]] std::vector<wxFileName> xmlInputPaths(const ItemDefinitionLoadInput& input) {
	if (!input.xml_paths.empty()) {
		return input.xml_paths;
	}
	return { input.xml_path };
}

} // namespace

bool XmlItemParser::parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, wxString& error, std::vector<std::string>& warnings) const {
	const auto visitor = [&](const FileName&, pugi::xml_node item_node, wxString& visit_error, std::vector<std::string>& visit_warnings) {
		if (as_lower_str(item_node.name()) != "item") {
			return true;
		}

		uint32_t from_id = 0;
		uint32_t to_id = 0;
		if (const auto id_attr = item_node.attribute("id")) {
			from_id = to_id = id_attr.as_uint();
		} else {
			from_id = item_node.attribute("fromid").as_uint();
			to_id = item_node.attribute("toid").as_uint();
		}

		uint32_t from_client_id = 0;
		uint32_t to_client_id = 0;
		if (const auto client_id_attr = item_node.attribute("clientid")) {
			from_client_id = to_client_id = client_id_attr.as_uint();
		} else {
			from_client_id = item_node.attribute("fromclientid").as_uint();
			to_client_id = item_node.attribute("toclientid").as_uint();
		}

		if (from_id == 0 || to_id == 0) {
			visit_error = "Could not read XML item id range.";
			return false;
		}

		for (uint32_t server_id = from_id; server_id <= to_id; ++server_id) {
			XmlItemFragment fragment;
			fragment.server_id = static_cast<ServerItemId>(server_id);
			fragment.name = item_node.attribute("name").as_string();
			fragment.editor_suffix = item_node.attribute("editorsuffix").as_string();
			if (from_client_id != 0) {
				const uint32_t offset = server_id - from_id;
				fragment.client_id = static_cast<ClientItemId>(from_client_id + offset);
			}

			for (pugi::xml_node attribute_node = item_node.first_child(); attribute_node; attribute_node = attribute_node.next_sibling()) {
				if (!attribute_node.attribute("key")) {
					continue;
				}
				applyAttribute(fragment, attribute_node);
			}

			fragments.xml[fragment.server_id] = std::move(fragment);
		}

		return true;
	};

	bool parsedAny = false;
	for (const wxFileName& xmlPath : xmlInputPaths(input)) {
		if (!XmlFileLoader::visitElements(FileName(xmlPath.GetFullPath()), "items", visitor, error, warnings)) {
			if (error.empty()) {
				error = "Could not load items XML file: " + xmlPath.GetFullPath();
			}
			return false;
		}
		parsedAny = true;
	}

	if (parsedAny && fragments.xml.empty()) {
		warnings.push_back("items.xml did not contain any item definitions.");
	}

	return true;
}
