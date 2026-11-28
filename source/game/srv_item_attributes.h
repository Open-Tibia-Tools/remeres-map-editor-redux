//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_GAME_SRV_ITEM_ATTRIBUTES_H_
#define RME_GAME_SRV_ITEM_ATTRIBUTES_H_

// Item attributes of the original game server that have no OTBM
// field. Maps read from sector files (IOMapSec) keep them as item attributes
// under the server's own names; the item panel edits them and the map
// tooltips show them.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace rme::srv {

	enum class ItemAttribute : uint8_t {
		KeyNumber,
		KeyholeNumber,
		ChestQuestNumber,
		DoorLevel,
		DoorQuestNumber,
		DoorQuestValue,
		RemainingExpireTime,
		SavedExpireTime,
		RemainingUses,
		Responsible,
	};

	struct ItemAttributeInfo {
		std::string_view key; // Attribute name in sector files and on the item
		std::string_view label;
	};

	inline constexpr std::array<ItemAttributeInfo, 10> kItemAttributes = { {
		{ "KeyNumber", "Key Number" },
		{ "KeyholeNumber", "Keyhole Number" },
		{ "ChestQuestNumber", "Quest Chest" },
		{ "Level", "Door Level" },
		{ "DoorQuestNumber", "Door Quest" },
		{ "DoorQuestValue", "Door Quest Value" },
		{ "RemainingExpireTime", "Expires In" },
		{ "SavedExpireTime", "Saved Expire Time" },
		{ "RemainingUses", "Remaining Uses" },
		{ "Responsible", "Responsible" },
	} };

	[[nodiscard]] constexpr const ItemAttributeInfo& info(ItemAttribute attribute) {
		return kItemAttributes[static_cast<size_t>(attribute)];
	}

	// The keys as std::string, built once: item attributes live in a
	// std::map<std::string, ...>, and these are looked up while drawing.
	[[nodiscard]] inline const std::string& key(ItemAttribute attribute) {
		static const std::array<std::string, kItemAttributes.size()> keys = [] {
			std::array<std::string, kItemAttributes.size()> result;
			for (size_t i = 0; i < kItemAttributes.size(); ++i) {
				result[i] = std::string(kItemAttributes[i].key);
			}
			return result;
		}();
		return keys[static_cast<size_t>(attribute)];
	}

} // namespace rme::srv

#endif
