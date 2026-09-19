#include "header_serialization_otbm.h"

#include "io/iomap_otbm.h"
#include "io/otbm/fast_otbm_reader.h"

#include "map/map.h"
#include "item_definitions/core/item_definition_store.h"
#include "ui/dialog_util.h"
#include <spdlog/spdlog.h>

namespace {
int toDisplayOTBMVersion(uint32_t raw_version) {
	return raw_version <= static_cast<uint32_t>(MAP_OTBM_4) ? static_cast<int>(raw_version) + 1 : static_cast<int>(raw_version);
}
}

bool HeaderSerializationOTBM::getVersionInfo(NodeFileReadHandle& f, MapVersion& out_ver) {
	BinaryNode* root = f.getRootNode();
	if (!root) {
		return false;
	}

	if (!root->skip(1)) { // Skip the type byte
		return false;
	}

	uint32_t u32;

	if (!root->getU32(u32)) { // Version
		return false;
	}
	out_ver.otbm = static_cast<MapVersionID>(u32);

	uint16_t u16;
	if (!root->getU16(u16) || !root->getU16(u16) || !root->getU32(u32)) {
		return false;
	}

	if (!root->getU32(u32)) { // OTB minor version
		return false;
	}

	out_ver.client = static_cast<OtbVersionID>(u32);
	return true;
}

bool HeaderSerializationOTBM::peekStartupInfo(NodeFileReadHandle& f, OTBMStartupPeekResult& out_info) {
	BinaryNode* root = f.getRootNode();
	if (!root) {
		return false;
	}

	uint8_t root_type = 0;
	if (!root->getByte(root_type)) {
		return false;
	}

	uint32_t raw_otbm_version = 0;
	if (!root->getU32(raw_otbm_version)) {
		return false;
	}
	out_info.otbm_version = toDisplayOTBMVersion(raw_otbm_version);

	if (!root->getU16(out_info.width) || !root->getU16(out_info.height) || !root->getU32(out_info.items_major_version) || !root->getU32(out_info.items_minor_version)) {
		return false;
	}

	BinaryNode* map_header_node = root->getChild();
	if (!map_header_node) {
		return true;
	}

	uint8_t node_type = 0;
	if (!map_header_node->getByte(node_type) || node_type != OTBM_MAP_DATA) {
		return false;
	}

	uint8_t attribute = 0;
	while (map_header_node->getU8(attribute)) {
		switch (attribute) {
			case OTBM_ATTR_DESCRIPTION: {
				std::string description;
				if (!map_header_node->getString(description)) {
					return false;
				}
				out_info.description = wxstr(description);
				break;
			}
			case OTBM_ATTR_EXT_SPAWN_FILE: {
				std::string spawn_file;
				if (!map_header_node->getString(spawn_file)) {
					return false;
				}
				out_info.spawn_xml_file = wxstr(spawn_file);
				break;
			}
			case OTBM_ATTR_EXT_HOUSE_FILE: {
				std::string house_file;
				if (!map_header_node->getString(house_file)) {
					return false;
				}
				out_info.house_xml_file = wxstr(house_file);
				break;
			}
			case OTBM_ATTR_EXT_SPAWN_NPC_FILE: {
				std::string ignored_string;
				if (!map_header_node->getString(ignored_string)) {
					return false;
				}
				break;
			}
			default:
				return true;
		}
	}

	return true;
}

bool HeaderSerializationOTBM::loadMapRoot(Map& map, NodeFileReadHandle& f, MapVersion& version, BinaryNode*& root, BinaryNode*& mapHeaderNode) {
	root = f.getRootNode();
	if (!root) {
		spdlog::error("Could not read root node.");
		return false;
	}
	if (!root->skip(1)) { // Skip the type byte
		return false;
	}

	uint32_t u32;
	if (!root->getU32(u32)) {
		return false;
	}

	version.otbm = static_cast<MapVersionID>(u32);

	if (version.otbm > MAP_OTBM_4) {
		// Failed to read version
		if (DialogUtil::PopupDialog("Map error", "The loaded map appears to be a OTBM format that is not supported by the editor.\n"
												 "Do you still want to attempt to load the map?",
									wxYES | wxNO)
			== wxID_YES) {
			spdlog::warn("Unsupported or damaged map version: {}", static_cast<int>(version.otbm));
		} else {
			spdlog::error("Unsupported OTBM version {}, could not load map", static_cast<int>(version.otbm));
			return false;
		}
	}

	uint16_t u16;
	if (!root->getU16(u16)) {
		return false;
	}
	map.width = u16;

	if (!root->getU16(u16)) {
		return false;
	}
	map.height = u16;

	if (!root->getU32(u32)) {
		return false;
	}

	if (u32 > static_cast<uint32_t>(g_item_definitions.MajorVersion)) {
		if (DialogUtil::PopupDialog("Map error", "The loaded map appears to be a items.otb format that deviates from the "
												 "items.otb loaded by the editor. Do you still want to attempt to load the map?",
									wxYES | wxNO)
			== wxID_YES) {
			spdlog::warn("Unsupported or damaged OTB major version: {}", u32);
		} else {
			spdlog::error("Outdated items.otb (major {}), could not load map", u32);
			return false;
		}
	}

	if (!root->getU32(u32)) {
		return false;
	}

	if (u32 > static_cast<uint32_t>(g_item_definitions.MinorVersion)) {
		spdlog::warn("This editor needs an updated items.otb version (found minor {})", u32);
	}
	if (u32 == 0) {
		spdlog::warn("Invalid OTB version ID (0) in map header.");
	}
	version.client = static_cast<OtbVersionID>(u32);

	mapHeaderNode = root->getChild();
	uint8_t u8;
	if (mapHeaderNode == nullptr || !mapHeaderNode->getByte(u8) || u8 != OTBM_MAP_DATA) {
		spdlog::error("Could not get root child node (OTBM_MAP_DATA). Cannot recover from fatal error!");
		return false;
	}
	return true;
}

bool HeaderSerializationOTBM::readMapAttributes(Map& map, BinaryNode* mapHeaderNode) {
	uint8_t attribute;
	while (mapHeaderNode->getU8(attribute)) {
		switch (attribute) {
			case OTBM_ATTR_DESCRIPTION: {
				if (!mapHeaderNode->getString(map.description)) {
					spdlog::warn("Invalid map description tag");
					return true;
				}
				break;
			}
			case OTBM_ATTR_EXT_SPAWN_FILE: {
				if (!mapHeaderNode->getString(map.spawnfile)) {
					spdlog::warn("Invalid map spawnfile tag");
					return true;
				}
				break;
			}
			case OTBM_ATTR_EXT_HOUSE_FILE: {
				if (!mapHeaderNode->getString(map.housefile)) {
					spdlog::warn("Invalid map housefile tag");
					return true;
				}
				break;
			}
			case OTBM_ATTR_EXT_SPAWN_NPC_FILE: {
				// compatibility: skip Canary RME NPC spawn file tag
				std::string stringToSkip;
				if (!mapHeaderNode->getString(stringToSkip)) {
					spdlog::warn("Invalid map NPC spawnfile tag");
					return true;
				}
				break;
			}
			default: {
				spdlog::warn("Unknown header attribute: {}. Continuing map load without parsing the remaining header attributes.", static_cast<int>(attribute));
				return true;
			}
		}
	}
	return true;
}

bool HeaderSerializationOTBM::readMapAttributesFast(Map& map, FastOTBMStream& stream) {
	uint8_t attribute;
	while (stream.getU8(attribute)) {
		switch (attribute) {
			case OTBM_ATTR_DESCRIPTION: {
				if (!stream.getString(map.description)) {
					spdlog::warn("Invalid map description tag");
					return true;
				}
				break;
			}
			case OTBM_ATTR_EXT_SPAWN_FILE: {
				if (!stream.getString(map.spawnfile)) {
					spdlog::warn("Invalid map spawnfile tag");
					return true;
				}
				break;
			}
			case OTBM_ATTR_EXT_HOUSE_FILE: {
				if (!stream.getString(map.housefile)) {
					spdlog::warn("Invalid map housefile tag");
					return true;
				}
				break;
			}
			case OTBM_ATTR_EXT_SPAWN_NPC_FILE: {
				// compatibility: skip Canary RME NPC spawn file tag
				std::string stringToSkip;
				if (!stream.getString(stringToSkip)) {
					spdlog::warn("Invalid map NPC spawnfile tag");
					return true;
				}
				break;
			}
			default: {
				spdlog::warn("Unknown header attribute: {}. Continuing map load without parsing the remaining header attributes.", static_cast<int>(attribute));
				return true;
			}
		}
	}
	return true;
}
