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

bool HeaderSerializationOTBM::getVersionInfo(const FileName& filename, MapVersion& out_ver) {
#ifdef _WIN32
	FILE* f = _wfopen(filename.GetFullPath().wc_str(), L"rb");
#else
	FILE* f = fopen(filename.GetFullPath().mb_str(), "rb");
#endif
	if (!f) {
		return false;
	}
	uint8_t buffer[512];
	size_t bytesRead = std::fread(buffer, 1, sizeof(buffer), f);
	std::fclose(f);

	return getVersionInfo(buffer, bytesRead, out_ver);
}

bool HeaderSerializationOTBM::getVersionInfo(const uint8_t* data, size_t size, MapVersion& out_ver) {
	if (size < 4) {
		return false;
	}
	size_t offset = 0;
	if (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0) {
		offset = 4;
	}
	if (offset + 4 > size || std::memcmp(data + offset, "OTBM", 4) != 0) {
		return false;
	}
	offset += 4;

	FastOTBMStream stream(data + offset, size - offset);
	if (stream.readByte() != OTBM_NODE_START) {
		return false;
	}
	stream.readByte(); // skip root type byte

	uint32_t u32;
	if (!stream.getU32(u32)) {
		return false;
	}
	out_ver.otbm = static_cast<MapVersionID>(u32);

	uint16_t u16;
	if (!stream.getU16(u16) || !stream.getU16(u16) || !stream.getU32(u32)) {
		return false;
	}

	if (!stream.getU32(u32)) { // OTB minor version
		return false;
	}

	out_ver.client = static_cast<OtbVersionID>(u32);
	return true;
}

bool HeaderSerializationOTBM::peekStartupInfo(const FileName& identifier, OTBMStartupPeekResult& out_info) {
	out_info = {};
	out_info.map_name = identifier.GetName();

	wxDateTime modified_time;
	if (identifier.GetTimes(nullptr, &modified_time, nullptr)) {
		out_info.modified_time = modified_time;
	}

#ifdef _WIN32
	FILE* f = _wfopen(identifier.GetFullPath().wc_str(), L"rb");
#else
	FILE* f = fopen(identifier.GetFullPath().mb_str(), "rb");
#endif
	if (!f) {
		out_info.has_error = true;
		out_info.error_message = "Could not open map file for reading.";
		return false;
	}
	uint8_t buffer[4096];
	size_t bytesRead = std::fread(buffer, 1, sizeof(buffer), f);
	std::fclose(f);

	if (bytesRead < 4) {
		out_info.has_error = true;
		out_info.error_message = "File is too small to be a valid OTBM map.";
		return false;
	}

	size_t offset = 0;
	if (buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 0 && buffer[3] == 0) {
		offset = 4;
	}
	if (offset + 4 > bytesRead || std::memcmp(buffer + offset, "OTBM", 4) != 0) {
		out_info.has_error = true;
		out_info.error_message = "File is not a valid OTBM map.";
		return false;
	}
	offset += 4;

	FastOTBMStream stream(buffer + offset, bytesRead - offset);
	if (stream.readByte() != OTBM_NODE_START) {
		out_info.has_error = true;
		out_info.error_message = "Could not read root node in OTBM header.";
		return false;
	}
	uint8_t root_type = stream.readByte();

	uint32_t raw_otbm_version = 0;
	if (!stream.getU32(raw_otbm_version)) {
		out_info.has_error = true;
		out_info.error_message = "Could not read OTBM version.";
		return false;
	}
	out_info.otbm_version = toDisplayOTBMVersion(raw_otbm_version);

	if (!stream.getU16(out_info.width) || !stream.getU16(out_info.height) ||
		!stream.getU32(out_info.items_major_version) || !stream.getU32(out_info.items_minor_version)) {
		out_info.has_error = true;
		out_info.error_message = "Could not read OTBM header dimensions or item versions.";
		return false;
	}

	FastOTBMNode rootNode(root_type, stream.p, stream.end);
	rootNode.forEachChild([&](FastOTBMNode& child) {
		if (child.type == OTBM_MAP_DATA) {
			uint8_t attribute = 0;
			while (child.stream.getU8(attribute)) {
				switch (attribute) {
					case OTBM_ATTR_DESCRIPTION: {
						std::string description;
						if (child.stream.getString(description)) {
							out_info.description = wxstr(description);
						}
						break;
					}
					case OTBM_ATTR_EXT_SPAWN_FILE: {
						std::string spawn_file;
						if (child.stream.getString(spawn_file)) {
							out_info.spawn_xml_file = wxstr(spawn_file);
						}
						break;
					}
					case OTBM_ATTR_EXT_HOUSE_FILE: {
						std::string house_file;
						if (child.stream.getString(house_file)) {
							out_info.house_xml_file = wxstr(house_file);
						}
						break;
					}
					case OTBM_ATTR_EXT_SPAWN_NPC_FILE: {
						std::string ignored_string;
						child.stream.getString(ignored_string);
						break;
					}
					default:
						return;
				}
			}
		}
	});

	return true;
}

bool HeaderSerializationOTBM::readMapAttributes(Map& map, FastOTBMStream& stream) {
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
