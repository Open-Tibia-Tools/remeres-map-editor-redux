#include "item_definitions/formats/srv/srv_item_parser.h"

#include "item_definitions/formats/srv/objects_srv_reader.h"
#include "util/common.h"

#include <format>
#include <fstream>
#include <iterator>

namespace {

	// Types below 100 are the server's map, body and creature containers; they
	// never appear on a map and have no client graphics.
	constexpr uint16_t kFirstClientTypeId = 100;

	int attributeOr(const rme::srv::ObjectType& type, std::string_view name, int fallback) {
		return static_cast<int>(type.attribute(name).value_or(fallback));
	}

	bool isDoor(const rme::srv::ObjectType& type) {
		return type.hasFlag("KeyDoor") || type.hasFlag("NameDoor") || type.hasFlag("LevelDoor") || type.hasFlag("QuestDoor");
	}

	void assignGroup(const rme::srv::ObjectType& type, OtbItemFragment& fragment) {
		if (type.hasFlag("Bank")) {
			fragment.group = ITEM_GROUP_GROUND;
		} else if (type.hasFlag("Container") || type.hasFlag("Chest")) {
			fragment.group = ITEM_GROUP_CONTAINER;
		} else if (type.hasFlag("LiquidContainer")) {
			fragment.group = ITEM_GROUP_FLUID;
		} else if (type.hasFlag("LiquidPool")) {
			fragment.group = ITEM_GROUP_SPLASH;
		} else if (type.hasFlag("TeleportAbsolute") || type.hasFlag("TeleportRelative")) {
			fragment.group = ITEM_GROUP_TELEPORT;
			fragment.type = ITEM_TYPE_TELEPORT;
		} else if (type.hasFlag("MagicField")) {
			fragment.group = ITEM_GROUP_MAGICFIELD;
			fragment.type = ITEM_TYPE_MAGICFIELD;
		} else if (isDoor(type)) {
			fragment.group = ITEM_GROUP_DOOR;
			fragment.type = ITEM_TYPE_DOOR;
		} else if (type.hasFlag("Key")) {
			fragment.group = ITEM_GROUP_KEY;
			fragment.type = ITEM_TYPE_KEY;
		} else if (type.hasFlag("Rune")) {
			fragment.group = ITEM_GROUP_RUNE;
		} else if (type.hasFlag("Write") || type.hasFlag("WriteOnce")) {
			fragment.group = ITEM_GROUP_WRITEABLE;
		}

		// Some grounds are chests too (a small hole, a loose board) and hold items.
		if (type.hasFlag("Container") || type.hasFlag("Chest")) {
			fragment.type = ITEM_TYPE_CONTAINER;
		} else if (type.hasFlag("Bed") && fragment.type == ITEM_TYPE_NONE) {
			fragment.type = ITEM_TYPE_BED;
		}
	}

	void assignFlags(const rme::srv::ObjectType& type, OtbItemFragment& fragment) {
		uint64_t& flags = fragment.flags;
		setMappedFlag(flags, type.hasFlag("Cumulative"), ItemFlag::Stackable);
		setMappedFlag(flags, type.hasFlag("Take"), ItemFlag::Pickupable);
		setMappedFlag(flags, !type.hasFlag("Unmove"), ItemFlag::Moveable);
		setMappedFlag(flags, type.hasFlag("Unpass"), ItemFlag::Unpassable);
		setMappedFlag(flags, type.hasFlag("Unthrow"), ItemFlag::BlockMissiles);
		setMappedFlag(flags, type.hasFlag("Avoid"), ItemFlag::BlockPathfinder);
		setMappedFlag(flags, type.hasFlag("Height"), ItemFlag::HasElevation);
		setMappedFlag(flags, type.hasFlag("Hang"), ItemFlag::IsHangable);
		setMappedFlag(flags, type.hasFlag("HookEast"), ItemFlag::HookEast);
		setMappedFlag(flags, type.hasFlag("HookSouth"), ItemFlag::HookSouth);
		setMappedFlag(flags, type.hasFlag("Rotate"), ItemFlag::Rotatable);
		setMappedFlag(flags, type.hasFlag("MultiUse"), ItemFlag::MultiUse);
		setMappedFlag(flags, type.hasFlag("ForceUse"), ItemFlag::ForceUse);
		setMappedFlag(flags, type.hasFlag("Corpse"), ItemFlag::Corpse);
		setMappedFlag(flags, type.hasFlag("Rune"), ItemFlag::ClientChargeable);

		const bool writable = type.hasFlag("Write") || type.hasFlag("WriteOnce");
		setMappedFlag(flags, type.hasFlag("Text") || writable, ItemFlag::CanReadText);
		setMappedFlag(flags, writable, ItemFlag::CanWriteText);

		// Clip, Bottom and Top are the server's stack priorities above ground.
		if (type.hasFlag("Clip")) {
			fragment.always_on_top_order = 1;
		} else if (type.hasFlag("Bottom")) {
			fragment.always_on_top_order = 2;
		} else if (type.hasFlag("Top")) {
			fragment.always_on_top_order = 3;
		}
		setMappedFlag(flags, fragment.always_on_top_order != 0, ItemFlag::AlwaysOnBottom);
	}

	OtbItemFragment toFragment(const rme::srv::ObjectType& type) {
		OtbItemFragment fragment;
		fragment.server_id = type.type_id;
		fragment.client_id = type.hasFlag("Disguise") ? static_cast<ClientItemId>(type.attribute("DisguiseTarget").value_or(type.type_id)) : type.type_id;
		fragment.name = type.name;
		fragment.description = type.description;

		assignGroup(type, fragment);
		assignFlags(type, fragment);

		fragment.volume = static_cast<uint16_t>(attributeOr(type, "Capacity", 0));
		// Quest chests have no Capacity: the server requires exactly one object
		// in them (the reward it copies to the player), so give them one slot.
		if (type.hasFlag("Chest") && fragment.volume == 0) {
			fragment.volume = 1;
		}
		fragment.way_speed = static_cast<uint16_t>(attributeOr(type, "Waypoints", 100));
		fragment.max_text_len = static_cast<uint16_t>(attributeOr(type, "MaxLength", attributeOr(type, "MaxLengthOnce", 0)));
		if (type.hasFlag("Rotate")) {
			fragment.rotate_to = static_cast<uint16_t>(attributeOr(type, "RotateTarget", 0));
		}
		// The server stores weights in hundredths of an ounce.
		fragment.weight = static_cast<float>(attributeOr(type, "Weight", 0)) / 100.0f;
		fragment.attack = attributeOr(type, "WeaponAttackValue", attributeOr(type, "ThrowAttackValue", attributeOr(type, "AmmoAttackValue", 0)));
		fragment.defense = attributeOr(type, "WeaponDefendValue", attributeOr(type, "ShieldDefendValue", attributeOr(type, "ThrowDefendValue", 0)));
		fragment.armor = attributeOr(type, "ArmorValue", 0);
		return fragment;
	}

}

bool SrvItemParser::parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, wxString& error, std::vector<std::string>& /*warnings*/) const {
	const wxString path = input.srv_path.GetFullPath();
	if (path.IsEmpty()) {
		error = "This client version reads its items from an original server's objects.srv, which is found when a .sec map from that server is opened.";
		return false;
	}
	if (!input.srv_path.FileExists()) {
		error = wxstr(std::format("objects.srv was not found at \"{}\".", nstr(path)));
		return false;
	}

	std::ifstream stream(path.ToStdWstring(), std::ios::binary);
	const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (!stream.good() && !stream.eof()) {
		error = wxstr(std::format("Could not read \"{}\".", nstr(path)));
		return false;
	}

	const auto types = rme::srv::readObjectTypes(text);
	if (!types) {
		error = wxstr(std::format("{}:{}: {}", nstr(path), types.error().line, types.error().message));
		return false;
	}

	for (const rme::srv::ObjectType& type : *types) {
		if (type.type_id < kFirstClientTypeId) {
			continue;
		}
		fragments.otb[type.type_id] = toFragment(type);
	}

	if (fragments.otb.empty()) {
		error = wxstr(std::format("\"{}\" defines no object types.", nstr(path)));
		return false;
	}
	return true;
}
