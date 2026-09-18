#include "item_definitions/formats/otb/otb_item_parser.h"

#include "item_definitions/formats/otb/otb_item_format.h"
#include "io/filehandle.h"

#include <array>
#include <format>
#include <limits>

namespace {
	constexpr uint64_t flagMask(ItemFlag flag) {
		return uint64_t { 1 } << static_cast<uint8_t>(flag);
	}
	constexpr uint16_t kOtbVersionFieldBytes = static_cast<uint16_t>(sizeof(uint32_t) * 3);
	constexpr uint16_t kOtbVersionTailBytes = 128;
	constexpr uint16_t kExpectedOtbVersionHeaderLength = kOtbVersionFieldBytes + kOtbVersionTailBytes;

	void setMappedFlag(uint64_t& target, bool value, ItemFlag flag) {
		if (value) {
			target |= flagMask(flag);
		}
	}

	template <typename T>
	bool readFixedPayload(BinaryNode* node, uint16_t length, T& value) {
		if (length != sizeof(T)) {
			return false;
		}
		return node->getRAW(reinterpret_cast<uint8_t*>(&value), sizeof(T));
	}

	bool skipPayload(BinaryNode* node, uint16_t length) {
		return length == 0 || node->skip(length);
	}
}

bool OtbItemParser::parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, wxString& error, std::vector<std::string>& warnings) const {
	DiskNodeFileReadHandle file(input.otb_path.GetFullPath().ToStdString(), StringVector(1, "OTBI"));
	if (!file.isOk()) {
		error = wxString::FromUTF8(std::format("Couldn't open items.otb: {}", file.getErrorMessage()));
		return false;
	}

	BinaryNode* root = file.getRootNode();
	root->skip(1);
	root->skip(4);

	uint8_t root_attr = 0;
	if (!root->getU8(root_attr) || root_attr != ROOT_ATTR_VERSION) {
		error = "Expected ROOT_ATTR_VERSION as first node of items.otb.";
		return false;
	}

	uint16_t version_header_length = 0;
	uint32_t major_version = 0;
	uint32_t minor_version = 0;
	uint32_t build_number = 0;
	if (!root->getU16(version_header_length) || !root->getU32(major_version) || !root->getU32(minor_version) || !root->getU32(build_number)) {
		error = "Invalid items.otb version header.";
		return false;
	}
	if (version_header_length != kExpectedOtbVersionHeaderLength) {
		error = "Invalid items.otb version payload.";
		return false;
	}
	if (!root->skip(version_header_length - kOtbVersionFieldBytes)) {
		error = "Invalid items.otb version payload.";
		return false;
	}
	fragments.version.major_version = major_version;
	fragments.version.minor_version = minor_version;
	fragments.version.build_number = build_number;

	OtbFileFormatVersion version = OtbFileFormatVersion::V1;
	switch (major_version) {
		case 1: version = OtbFileFormatVersion::V1; break;
		case 2: version = OtbFileFormatVersion::V2; break;
		case 3: version = OtbFileFormatVersion::V3; break;
		default:
			error = wxString::FromUTF8(std::format("Unsupported items.otb version {}", major_version));
			return false;
	}

	for (BinaryNode* item_node = root->getChild(); item_node != nullptr; item_node = item_node->advance()) {
		uint8_t group_raw = 0;
		if (!item_node->getU8(group_raw)) {
			warnings.push_back("Invalid item group in items.otb.");
			continue;
		}
		if (static_cast<ItemGroup_t>(group_raw) == ITEM_GROUP_DEPRECATED) {
			continue;
		}

		OtbItemFragment fragment;
		fragment.group = static_cast<ItemGroup_t>(group_raw);
		if (fragment.group == ITEM_GROUP_CONTAINER) {
			fragment.type = ITEM_TYPE_CONTAINER;
		} else if (fragment.group == ITEM_GROUP_DOOR) {
			fragment.type = ITEM_TYPE_DOOR;
		} else if (fragment.group == ITEM_GROUP_TELEPORT) {
			fragment.type = ITEM_TYPE_TELEPORT;
		} else if (fragment.group == ITEM_GROUP_MAGICFIELD) {
			fragment.type = ITEM_TYPE_MAGICFIELD;
		}

		uint32_t flags = 0;
		if (item_node->getU32(flags)) {
			setMappedFlag(fragment.flags, (flags & FLAG_UNPASSABLE) != 0, ItemFlag::Unpassable);
			setMappedFlag(fragment.flags, (flags & FLAG_BLOCK_MISSILES) != 0, ItemFlag::BlockMissiles);
			setMappedFlag(fragment.flags, (flags & FLAG_BLOCK_PATHFINDER) != 0, ItemFlag::BlockPathfinder);
			setMappedFlag(fragment.flags, (flags & FLAG_HAS_ELEVATION) != 0, ItemFlag::HasElevation);
			setMappedFlag(fragment.flags, (flags & FLAG_USEABLE) != 0, ItemFlag::ForceUse);
			setMappedFlag(fragment.flags, (flags & FLAG_PICKUPABLE) != 0, ItemFlag::Pickupable);
			setMappedFlag(fragment.flags, (flags & FLAG_MOVEABLE) != 0, ItemFlag::Moveable);
			setMappedFlag(fragment.flags, (flags & FLAG_STACKABLE) != 0, ItemFlag::Stackable);
			setMappedFlag(fragment.flags, (flags & FLAG_FLOORCHANGEDOWN) != 0, ItemFlag::FloorChangeDown);
			setMappedFlag(fragment.flags, (flags & FLAG_FLOORCHANGENORTH) != 0, ItemFlag::FloorChangeNorth);
			setMappedFlag(fragment.flags, (flags & FLAG_FLOORCHANGEEAST) != 0, ItemFlag::FloorChangeEast);
			setMappedFlag(fragment.flags, (flags & FLAG_FLOORCHANGESOUTH) != 0, ItemFlag::FloorChangeSouth);
			setMappedFlag(fragment.flags, (flags & FLAG_FLOORCHANGEWEST) != 0, ItemFlag::FloorChangeWest);
			setMappedFlag(fragment.flags, (flags & FLAG_ALWAYSONTOP) != 0, ItemFlag::AlwaysOnBottom);
			setMappedFlag(fragment.flags, (flags & FLAG_HANGABLE) != 0, ItemFlag::IsHangable);
			setMappedFlag(fragment.flags, (flags & FLAG_HOOK_EAST) != 0, ItemFlag::HookEast);
			setMappedFlag(fragment.flags, (flags & FLAG_HOOK_SOUTH) != 0, ItemFlag::HookSouth);
			setMappedFlag(fragment.flags, (flags & FLAG_ALLOWDISTREAD) != 0, ItemFlag::AllowDistRead);
			setMappedFlag(fragment.flags, (flags & FLAG_ROTABLE) != 0, ItemFlag::Rotatable);
			setMappedFlag(fragment.flags, (flags & FLAG_READABLE) != 0, ItemFlag::CanReadText);
			if (version >= OtbFileFormatVersion::V3) {
				setMappedFlag(fragment.flags, (flags & FLAG_CLIENTCHARGES) != 0, ItemFlag::ClientChargeable);
				setMappedFlag(fragment.flags, (flags & FLAG_IGNORE_LOOK) != 0, ItemFlag::IgnoreLook);
			}
			if ((fragment.flags & flagMask(ItemFlag::FloorChangeDown)) || (fragment.flags & flagMask(ItemFlag::FloorChangeNorth)) ||
				(fragment.flags & flagMask(ItemFlag::FloorChangeEast)) || (fragment.flags & flagMask(ItemFlag::FloorChangeSouth)) ||
				(fragment.flags & flagMask(ItemFlag::FloorChangeWest))) {
				fragment.flags |= flagMask(ItemFlag::FloorChange);
			}
		}

		for (uint8_t attr = 0; item_node->getU8(attr);) {
			uint16_t length = 0;
			if (!item_node->getU16(length)) {
				warnings.push_back("Invalid item attribute length in items.otb.");
				break;
			}

			switch (attr) {
				case ITEM_ATTR_SERVERID: {
					if (length == sizeof(uint16_t)) {
						uint16_t id16 = 0;
						if (item_node->getU16(id16)) {
							fragment.server_id = id16;
						} else {
							warnings.push_back("Invalid server id in items.otb.");
						}
					} else if (length == sizeof(uint32_t)) {
						uint32_t id32 = 0;
						if (item_node->getU32(id32)) {
							fragment.server_id = id32;
						} else {
							warnings.push_back("Invalid server id in items.otb.");
						}
					} else {
						warnings.push_back("Invalid server id in items.otb.");
						skipPayload(item_node, length);
					}
					break;
				}
				case ITEM_ATTR_CLIENTID: {
					if (length == sizeof(uint16_t)) {
						uint16_t id16 = 0;
						if (item_node->getU16(id16)) {
							fragment.client_id = id16;
						} else {
							warnings.push_back("Invalid client id in items.otb.");
						}
					} else if (length == sizeof(uint32_t)) {
						uint32_t id32 = 0;
						if (item_node->getU32(id32)) {
							fragment.client_id = id32;
						} else {
							warnings.push_back("Invalid client id in items.otb.");
						}
					} else {
						warnings.push_back("Invalid client id in items.otb.");
						skipPayload(item_node, length);
					}
					break;
				}
				case ITEM_ATTR_NAME: {
					std::string value;
					if (!item_node->getRAW(value, length)) {
						warnings.push_back("Invalid name in items.otb.");
					}
					fragment.name = value;
					break;
				}
				case ITEM_ATTR_DESCR: {
					std::string value;
					if (!item_node->getRAW(value, length)) {
						warnings.push_back("Invalid description in items.otb.");
					}
					fragment.description = value;
					break;
				}
				case ITEM_ATTR_SPEED:
					if (!readFixedPayload(item_node, length, fragment.way_speed)) {
						warnings.push_back("Invalid speed in items.otb.");
						skipPayload(item_node, length);
					}
					break;
				case ITEM_ATTR_MAXITEMS:
					if (!readFixedPayload(item_node, length, fragment.volume)) {
						warnings.push_back("Invalid volume in items.otb.");
						skipPayload(item_node, length);
					}
					break;
				case ITEM_ATTR_WEIGHT: {
					double weight = 0.0;
					if (!readFixedPayload(item_node, length, weight)) {
						warnings.push_back("Invalid weight in items.otb.");
						skipPayload(item_node, length);
					}
					fragment.weight = static_cast<float>(weight);
					break;
				}
				case ITEM_ATTR_ROTATETO:
					if (!readFixedPayload(item_node, length, fragment.rotate_to)) {
						warnings.push_back("Invalid rotateTo in items.otb.");
						skipPayload(item_node, length);
					}
					break;
				case ITEM_ATTR_DECAY:
				case ITEM_ATTR_DECAY2:
					fragment.flags |= flagMask(ItemFlag::Decays);
					if (!skipPayload(item_node, length)) {
						warnings.push_back("Invalid decay attribute in items.otb.");
					}
					break;
				case ITEM_ATTR_TOPORDER: {
					uint8_t value = 0;
					if (!readFixedPayload(item_node, length, value)) {
						warnings.push_back("Invalid top order in items.otb.");
						skipPayload(item_node, length);
					}
					fragment.always_on_top_order = value;
					break;
				}
				case ITEM_ATTR_WRITEABLE3: {
					std::array<uint16_t, 2> payload {};
					if (!readFixedPayload(item_node, length, payload)) {
						warnings.push_back("Invalid writeable3 in items.otb.");
						skipPayload(item_node, length);
					} else {
						fragment.max_text_len = payload[1];
					}
					break;
				}
				case ITEM_ATTR_CLASSIFICATION: {
					uint8_t value = 0;
					if (!readFixedPayload(item_node, length, value)) {
						warnings.push_back("Invalid classification in items.otb.");
						skipPayload(item_node, length);
					}
					fragment.classification = value;
					break;
				}
				default:
					if (!item_node->skip(length)) {
						warnings.push_back("Failed to skip unknown items.otb attribute payload.");
					}
					break;
			}
		}

		if (fragment.server_id != 0) {
			fragments.otb[fragment.server_id] = std::move(fragment);
		}
	}

	if (fragments.otb.empty()) {
		error = "No item definitions were parsed from items.otb.";
		return false;
	}

	return true;
}
