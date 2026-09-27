#include "item_definitions/formats/ron/ron_item_parser.h"

#include <cctype>
#include <fstream>
#include <format>
#include <string>
#include <string_view>

namespace {

constexpr uint64_t flagMask(ItemFlag flag) {
	return uint64_t { 1 } << static_cast<uint8_t>(flag);
}

void setFlag(uint64_t& target, ItemFlag flag) {
	target |= flagMask(flag);
}

struct RonCursor {
	std::string_view text;
	size_t i = 0;
	int line = 1;

	[[nodiscard]] bool eof() const {
		return i >= text.size();
	}

	[[nodiscard]] char peek() const {
		return eof() ? '\0' : text[i];
	}

	char get() {
		if (eof()) {
			return '\0';
		}
		const char ch = text[i++];
		if (ch == '\n') {
			++line;
		}
		return ch;
	}

	void skipWs() {
		while (!eof()) {
			const char ch = peek();
			if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == ',') {
				get();
				continue;
			}
			if (ch == '/' && i + 1 < text.size() && text[i + 1] == '/') {
				while (!eof() && peek() != '\n') {
					get();
				}
				continue;
			}
			if (ch == '/' && i + 1 < text.size() && text[i + 1] == '*') {
				get();
				get();
				while (!eof() && !(peek() == '*' && i + 1 < text.size() && text[i + 1] == '/')) {
					get();
				}
				if (!eof()) {
					get();
					get();
				}
				continue;
			}
			break;
		}
	}

	[[nodiscard]] bool consume(char expected) {
		skipWs();
		if (peek() != expected) {
			return false;
		}
		get();
		return true;
	}

	[[nodiscard]] std::string parseIdent() {
		skipWs();
		std::string ident;
		while (!eof()) {
			const unsigned char ch = static_cast<unsigned char>(peek());
			if (!std::isalnum(ch) && ch != '_') {
				break;
			}
			ident.push_back(get());
		}
		return ident;
	}

	[[nodiscard]] bool parseString(std::string& out, std::string& error) {
		skipWs();
		if (peek() != '"') {
			error = "expected string";
			return false;
		}
		get();
		out.clear();
		while (!eof()) {
			const char ch = get();
			if (ch == '"') {
				return true;
			}
			if (ch == '\\') {
				const char esc = get();
				switch (esc) {
					case '"':
					case '\\':
						out.push_back(esc);
						break;
					case 'n':
						out.push_back('\n');
						break;
					case 't':
						out.push_back('\t');
						break;
					case 'r':
						out.push_back('\r');
						break;
					default:
						out.push_back(esc);
						break;
				}
				continue;
			}
			if (ch == '\0') {
				break;
			}
			out.push_back(ch);
		}
		error = "unterminated string";
		return false;
	}

	[[nodiscard]] bool parseInt(int64_t& out, std::string& error) {
		skipWs();
		const size_t start = i;
		if (peek() == '-' || peek() == '+') {
			get();
		}
		if (!std::isdigit(static_cast<unsigned char>(peek()))) {
			error = "expected integer";
			return false;
		}
		while (std::isdigit(static_cast<unsigned char>(peek()))) {
			get();
		}
		try {
			out = std::stoll(std::string(text.substr(start, i - start)));
		} catch (const std::exception&) {
			error = "integer out of range";
			return false;
		}
		return true;
	}

	[[nodiscard]] bool skipValue(std::string& error) {
		skipWs();
		const char ch = peek();
		if (ch == '"') {
			std::string unused;
			return parseString(unused, error);
		}
		if (ch == '-' || ch == '+' || std::isdigit(static_cast<unsigned char>(ch))) {
			int64_t unused = 0;
			if (!parseInt(unused, error)) {
				return false;
			}
			if (peek() == '.') {
				get();
				while (std::isdigit(static_cast<unsigned char>(peek()))) {
					get();
				}
			}
			return true;
		}
		if (ch == '[') {
			get();
			skipWs();
			if (peek() == ']') {
				get();
				return true;
			}
			while (!eof()) {
				if (!skipValue(error)) {
					return false;
				}
				skipWs();
				if (peek() == ']') {
					get();
					return true;
				}
			}
			error = "unterminated list";
			return false;
		}
		if (ch == '(') {
			get();
			return skipUntil(')', error);
		}
		if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
			(void)parseIdent();
			skipWs();
			if (peek() == '(') {
				get();
				return skipUntil(')', error);
			}
			return true;
		}
		error = std::format("unexpected character '{}'", ch);
		return false;
	}

	[[nodiscard]] bool skipUntil(char end, std::string& error) {
		int depth = 1;
		while (!eof() && depth > 0) {
			skipWs();
			const char ch = peek();
			if (ch == '"') {
				std::string unused;
				if (!parseString(unused, error)) {
					return false;
				}
				continue;
			}
			get();
			if (ch == '(' || ch == '[') {
				++depth;
			} else if (ch == ')' || ch == ']') {
				--depth;
			}
			if (depth == 1 && ch == end) {
				return true;
			}
		}
		if (depth == 0) {
			return true;
		}
		error = "unterminated group";
		return false;
	}
};

std::string lowerCopy(std::string value) {
	for (char& ch : value) {
		ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	}
	return value;
}

void applyFlagName(OtbItemFragment& fragment, std::string_view name) {
	if (name == "alwaysontop") {
		setFlag(fragment.flags, ItemFlag::AlwaysOnBottom);
	} else if (name == "animation") {
		setFlag(fragment.flags, ItemFlag::AnimateAlways);
	} else if (name == "blockpathfind") {
		setFlag(fragment.flags, ItemFlag::BlockPathfinder);
	} else if (name == "blockprojectile") {
		setFlag(fragment.flags, ItemFlag::BlockMissiles);
	} else if (name == "blocksolid") {
		setFlag(fragment.flags, ItemFlag::Unpassable);
	} else if (name == "hangable") {
		setFlag(fragment.flags, ItemFlag::IsHangable);
	} else if (name == "hasheight") {
		setFlag(fragment.flags, ItemFlag::HasElevation);
	} else if (name == "horizontal") {
		setFlag(fragment.flags, ItemFlag::HookSouth);
	} else if (name == "vertical") {
		setFlag(fragment.flags, ItemFlag::HookEast);
	} else if (name == "moveable" || name == "movable") {
		setFlag(fragment.flags, ItemFlag::Moveable);
	} else if (name == "pickupable") {
		setFlag(fragment.flags, ItemFlag::Pickupable);
	} else if (name == "readable") {
		setFlag(fragment.flags, ItemFlag::CanReadText);
	} else if (name == "rotatable") {
		setFlag(fragment.flags, ItemFlag::Rotatable);
	} else if (name == "stackable") {
		setFlag(fragment.flags, ItemFlag::Stackable);
	} else if (name == "useable" || name == "usable") {
		setFlag(fragment.flags, ItemFlag::ForceUse);
	}
}

void noteFloorChange(OtbItemFragment& fragment) {
	if ((fragment.flags & flagMask(ItemFlag::FloorChangeDown)) || (fragment.flags & flagMask(ItemFlag::FloorChangeNorth)) ||
		(fragment.flags & flagMask(ItemFlag::FloorChangeEast)) || (fragment.flags & flagMask(ItemFlag::FloorChangeSouth)) ||
		(fragment.flags & flagMask(ItemFlag::FloorChangeWest))) {
		setFlag(fragment.flags, ItemFlag::FloorChange);
	}
}

void applyFloorChange(OtbItemFragment& fragment, std::string_view name) {
	setFlag(fragment.flags, ItemFlag::FloorChange);
	if (name == "down") {
		setFlag(fragment.flags, ItemFlag::FloorChangeDown);
	} else if (name == "north") {
		setFlag(fragment.flags, ItemFlag::FloorChangeNorth);
	} else if (name == "south") {
		setFlag(fragment.flags, ItemFlag::FloorChangeSouth);
	} else if (name == "east") {
		setFlag(fragment.flags, ItemFlag::FloorChangeEast);
	} else if (name == "west") {
		setFlag(fragment.flags, ItemFlag::FloorChangeWest);
	}
}

void applyGroup(OtbItemFragment& fragment, std::string_view name) {
	if (name == "ground") {
		fragment.group = ITEM_GROUP_GROUND;
	} else if (name == "container") {
		fragment.group = ITEM_GROUP_CONTAINER;
		if (fragment.type == ITEM_TYPE_NONE) {
			fragment.type = ITEM_TYPE_CONTAINER;
		}
	} else if (name == "fluid") {
		fragment.group = ITEM_GROUP_FLUID;
	} else if (name == "splash") {
		fragment.group = ITEM_GROUP_SPLASH;
	}
}

void applySlot(OtbItemFragment& fragment, std::string_view name) {
	uint16_t slot = fragment.slot_position == SLOTP_HAND ? 0 : fragment.slot_position;
	if (name == "head") {
		slot |= SLOTP_HEAD;
	} else if (name == "body") {
		slot |= SLOTP_ARMOR;
	} else if (name == "legs") {
		slot |= SLOTP_LEGS;
	} else if (name == "feet") {
		slot |= SLOTP_FEET;
	} else if (name == "backpack") {
		slot |= SLOTP_BACKPACK;
	} else if (name == "necklace") {
		slot |= SLOTP_NECKLACE;
	} else if (name == "ring") {
		slot |= SLOTP_RING;
	} else if (name == "ammo") {
		slot |= SLOTP_AMMO;
	} else if (name == "two-handed" || name == "twohanded") {
		slot |= SLOTP_HAND | SLOTP_TWO_HAND;
	} else if (name == "hand") {
		slot |= SLOTP_HAND;
	}
	if (slot != 0) {
		fragment.slot_position = slot;
	}
}

void applyWeapon(OtbItemFragment& fragment, std::string_view name) {
	if (name == "sword") {
		fragment.weapon_type = WEAPON_SWORD;
	} else if (name == "club") {
		fragment.weapon_type = WEAPON_CLUB;
	} else if (name == "axe") {
		fragment.weapon_type = WEAPON_AXE;
	} else if (name == "shield") {
		fragment.weapon_type = WEAPON_SHIELD;
	} else if (name == "distance") {
		fragment.weapon_type = WEAPON_DISTANCE;
	} else if (name == "wand") {
		fragment.weapon_type = WEAPON_WAND;
	} else if (name == "ammunition" || name == "ammo") {
		fragment.weapon_type = WEAPON_AMMO;
	}
	if (fragment.weapon_type != WEAPON_NONE && fragment.group == ITEM_GROUP_NONE) {
		fragment.group = fragment.weapon_type == WEAPON_AMMO ? ITEM_GROUP_AMMUNITION : ITEM_GROUP_WEAPON;
	}
}

void applyType(OtbItemFragment& fragment, std::string_view name) {
	if (name == "depot") {
		fragment.type = ITEM_TYPE_DEPOT;
	} else if (name == "mailbox") {
		fragment.type = ITEM_TYPE_MAILBOX;
	} else if (name == "trashholder") {
		fragment.type = ITEM_TYPE_TRASHHOLDER;
	} else if (name == "container" || name == "backpack") {
		fragment.type = ITEM_TYPE_CONTAINER;
		if (fragment.group == ITEM_GROUP_NONE) {
			fragment.group = ITEM_GROUP_CONTAINER;
		}
	} else if (name == "door") {
		fragment.type = ITEM_TYPE_DOOR;
		if (fragment.group == ITEM_GROUP_NONE) {
			fragment.group = ITEM_GROUP_DOOR;
		}
	} else if (name == "magicfield") {
		fragment.type = ITEM_TYPE_MAGICFIELD;
		fragment.group = ITEM_GROUP_MAGICFIELD;
	} else if (name == "teleport") {
		fragment.type = ITEM_TYPE_TELEPORT;
		if (fragment.group == ITEM_GROUP_NONE) {
			fragment.group = ITEM_GROUP_TELEPORT;
		}
	} else if (name == "bed") {
		fragment.type = ITEM_TYPE_BED;
	} else if (name == "key") {
		fragment.type = ITEM_TYPE_KEY;
		if (fragment.group == ITEM_GROUP_NONE) {
			fragment.group = ITEM_GROUP_KEY;
		}
	} else {
		applyWeapon(fragment, name);
	}
}

bool parseBool(RonCursor& cursor, bool& out, std::string& error) {
	const std::string ident = lowerCopy(cursor.parseIdent());
	if (ident == "true") {
		out = true;
		return true;
	}
	if (ident == "false") {
		out = false;
		return true;
	}
	error = "expected bool";
	return false;
}

bool parseName(RonCursor& cursor, std::string& out, std::string& error) {
	cursor.skipWs();
	if (cursor.peek() == '"') {
		return cursor.parseString(out, error);
	}
	out = cursor.parseIdent();
	if (out.empty()) {
		error = "expected name";
		return false;
	}
	return true;
}

bool applyField(RonCursor& cursor, const std::string& key, OtbItemFragment& fragment, std::string& error) {
	const std::string name = lowerCopy(key);
	if (name == "id") {
		int64_t value = 0;
		if (!cursor.parseInt(value, error) || value < 0 || value > 65535) {
			error = "invalid item id";
			return false;
		}
		fragment.server_id = static_cast<ServerItemId>(value);
		fragment.client_id = static_cast<ClientItemId>(value);
		return true;
	}
	if (name == "name" || name == "article" || name == "plural" || name == "description" || name == "editorsuffix" || name == "runespellname") {
		std::string text;
		if (!cursor.parseString(text, error)) {
			return false;
		}
		if (name == "name") {
			fragment.name = std::move(text);
		} else if (name == "description") {
			fragment.description = std::move(text);
		} else if (name == "editorsuffix" || name == "editor_suffix") {
			fragment.editor_suffix = std::move(text);
		}
		return true;
	}
	if (name == "editor_suffix") {
		return cursor.parseString(fragment.editor_suffix, error);
	}
	if (name == "group") {
		std::string ident;
		if (!parseName(cursor, ident, error)) {
			return false;
		}
		applyGroup(fragment, lowerCopy(ident));
		return true;
	}
	if (name == "flags") {
		if (!cursor.consume('[')) {
			error = "expected flag list";
			return false;
		}
		cursor.skipWs();
		if (cursor.peek() == ']') {
			cursor.get();
			return true;
		}
		while (!cursor.eof()) {
			std::string flag;
			if (!parseName(cursor, flag, error)) {
				return false;
			}
			applyFlagName(fragment, lowerCopy(flag));
			cursor.skipWs();
			if (cursor.peek() == ']') {
				cursor.get();
				return true;
			}
		}
		error = "unterminated flag list";
		return false;
	}
	if (name == "type") {
		std::string ident;
		if (!parseName(cursor, ident, error)) {
			return false;
		}
		applyType(fragment, lowerCopy(ident));
		return true;
	}
	if (name == "slottype" || name == "slot_type") {
		std::string ident;
		if (!parseName(cursor, ident, error)) {
			return false;
		}
		applySlot(fragment, lowerCopy(ident));
		return true;
	}
	if (name == "weapontype" || name == "weapon_type") {
		std::string ident;
		if (!parseName(cursor, ident, error)) {
			return false;
		}
		applyWeapon(fragment, lowerCopy(ident));
		return true;
	}
	if (name == "floorchange" || name == "floor_change") {
		std::string ident;
		if (!parseName(cursor, ident, error)) {
			return false;
		}
		applyFloorChange(fragment, lowerCopy(ident));
		noteFloorChange(fragment);
		return true;
	}
	if (name == "speed" || name == "containersize" || name == "container_size" || name == "alwaystoptoporder" || name == "always_on_top_order" || name == "rotateto" || name == "rotate_to" || name == "maxtextlen" || name == "max_text_len" || name == "weight" || name == "attack" || name == "defense" || name == "armor" || name == "charges" || name == "duration" || name == "decayto" || name == "decay_to") {
		int64_t value = 0;
		if (!cursor.parseInt(value, error)) {
			return false;
		}
		if (name == "speed" && value >= 0 && value <= 65535) {
			fragment.way_speed = static_cast<uint16_t>(value);
		} else if ((name == "containersize" || name == "container_size") && value >= 0 && value <= 65535) {
			fragment.volume = static_cast<uint16_t>(value);
		} else if (name == "always_on_top_order" || name == "alwaystoptoporder") {
			fragment.always_on_top_order = static_cast<int>(value);
		} else if ((name == "rotateto" || name == "rotate_to") && value >= 0 && value <= 65535) {
			fragment.rotate_to = static_cast<uint16_t>(value);
			setFlag(fragment.flags, ItemFlag::Rotatable);
		} else if ((name == "maxtextlen" || name == "max_text_len") && value >= 0 && value <= 65535) {
			fragment.max_text_len = static_cast<uint16_t>(value);
		} else if (name == "weight") {
			fragment.weight = static_cast<float>(value) / 100.f;
		} else if (name == "attack") {
			fragment.attack = static_cast<int>(value);
		} else if (name == "defense") {
			fragment.defense = static_cast<int>(value);
		} else if (name == "armor") {
			fragment.armor = static_cast<int>(value);
		} else if (name == "charges" && value > 0) {
			fragment.charges = static_cast<uint32_t>(value);
			setFlag(fragment.flags, ItemFlag::ExtraChargeable);
		} else if ((name == "duration" || name == "decayto" || name == "decay_to") && value > 0) {
			setFlag(fragment.flags, ItemFlag::Decays);
		}
		return true;
	}
	if (name == "blockprojectile" || name == "block_projectile" || name == "blockpathfind" || name == "block_path_find" || name == "moveable" || name == "readable" || name == "writeable" || name == "forceuse" || name == "force_use" || name == "allowdistread" || name == "allow_dist_read" || name == "replaceable") {
		bool value = false;
		if (!parseBool(cursor, value, error)) {
			return false;
		}
		if (!value) {
			return true;
		}
		if (name == "blockprojectile" || name == "block_projectile") {
			setFlag(fragment.flags, ItemFlag::BlockMissiles);
		} else if (name == "blockpathfind" || name == "block_path_find") {
			setFlag(fragment.flags, ItemFlag::BlockPathfinder);
		} else if (name == "moveable") {
			setFlag(fragment.flags, ItemFlag::Moveable);
		} else if (name == "readable") {
			setFlag(fragment.flags, ItemFlag::CanReadText);
		} else if (name == "writeable") {
			setFlag(fragment.flags, ItemFlag::CanReadText);
			setFlag(fragment.flags, ItemFlag::CanWriteText);
		} else if (name == "forceuse" || name == "force_use") {
			setFlag(fragment.flags, ItemFlag::ForceUse);
		} else if (name == "allowdistread" || name == "allow_dist_read") {
			setFlag(fragment.flags, ItemFlag::AllowDistRead);
		} else if (name == "replaceable") {
			setFlag(fragment.flags, ItemFlag::Replaceable);
		}
		return true;
	}
	if (name == "field") {
		fragment.type = ITEM_TYPE_MAGICFIELD;
		fragment.group = ITEM_GROUP_MAGICFIELD;
	}
	return cursor.skipValue(error);
}

bool parseItem(RonCursor& cursor, OtbItemFragment& fragment, std::string& error) {
	const std::string keyword = cursor.parseIdent();
	if (keyword != "Item") {
		error = "expected Item";
		return false;
	}
	if (!cursor.consume('(')) {
		error = "expected '(' after Item";
		return false;
	}
	cursor.skipWs();
	if (cursor.peek() == ')') {
		cursor.get();
		error = "item has no id";
		return false;
	}
	while (!cursor.eof()) {
		const std::string key = cursor.parseIdent();
		if (key.empty() || !cursor.consume(':')) {
			error = std::format("expected field name at line {}", cursor.line);
			return false;
		}
		if (!applyField(cursor, key, fragment, error)) {
			error = std::format("{} at line {}", error, cursor.line);
			return false;
		}
		cursor.skipWs();
		if (cursor.peek() == ')') {
			cursor.get();
			noteFloorChange(fragment);
			if (fragment.server_id == 0) {
				error = std::format("item is missing id at line {}", cursor.line);
				return false;
			}
			return true;
		}
	}
	error = "unterminated Item";
	return false;
}

} // namespace

bool RonItemParser::parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, wxString& error, std::vector<std::string>& warnings) const {
	const std::string path = input.ron_path.GetFullPath().ToStdString();
	if (path.empty()) {
		error = "items.ron path is missing.";
		return false;
	}

	std::ifstream file(path, std::ios::binary);
	if (!file) {
		error = wxString::FromUTF8(std::format("Couldn't open items.ron: {}", path));
		return false;
	}
	std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	RonCursor cursor { .text = text };

	const std::string root = cursor.parseIdent();
	if (root != "ItemCatalog" || !cursor.consume('(')) {
		error = "items.ron must start with ItemCatalog(";
		return false;
	}

	bool saw_items = false;
	while (!cursor.eof()) {
		const std::string key = cursor.parseIdent();
		if (key.empty()) {
			cursor.skipWs();
			if (cursor.peek() == ')') {
				break;
			}
			error = wxString::FromUTF8(std::format("Expected field in ItemCatalog at line {}.", cursor.line));
			return false;
		}
		if (!cursor.consume(':')) {
			error = wxString::FromUTF8(std::format("Expected ':' after {} at line {}.", key, cursor.line));
			return false;
		}
		if (key != "items") {
			std::string skip_error;
			if (!cursor.skipValue(skip_error)) {
				error = wxString::FromUTF8(std::format("{} at line {}.", skip_error, cursor.line));
				return false;
			}
			continue;
		}
		saw_items = true;
		if (!cursor.consume('[')) {
			error = "items.ron items field must be a list.";
			return false;
		}
		cursor.skipWs();
		while (!cursor.eof() && cursor.peek() != ']') {
			OtbItemFragment fragment;
			std::string item_error;
			if (!parseItem(cursor, fragment, item_error)) {
				error = wxString::FromUTF8(item_error.empty() ? std::format("Invalid item at line {}.", cursor.line) : item_error);
				return false;
			}
			if (fragments.otb.contains(fragment.server_id)) {
				warnings.push_back(std::format("Duplicate item id {} in items.ron; keeping the first.", fragment.server_id));
			} else {
				fragments.otb.emplace(fragment.server_id, std::move(fragment));
			}
			cursor.skipWs();
		}
		if (!cursor.consume(']')) {
			error = "Unterminated items list in items.ron.";
			return false;
		}
	}

	if (!saw_items || fragments.otb.empty()) {
		error = "items.ron did not contain any item definitions.";
		return false;
	}
	return true;
}
