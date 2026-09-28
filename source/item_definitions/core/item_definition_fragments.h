#ifndef RME_ITEM_DEFINITION_FRAGMENTS_H_
#define RME_ITEM_DEFINITION_FRAGMENTS_H_

#include "item_definitions/core/item_definition_types.h"
#include "app/client_version.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class GraphicManager;
struct DatCatalog;

struct DatItemFragment {
	ClientItemId client_id = 0;
	ItemGroup_t group = ITEM_GROUP_NONE;
	ItemTypes_t type = ITEM_TYPE_NONE;
	uint64_t flags = 0;
	uint16_t way_speed = 100;
	int always_on_top_order = 0;
	std::optional<uint16_t> max_text_len;
	std::optional<uint16_t> slot_position;
	std::string passive_metadata_json;
};

struct OtbItemFragment {
	ServerItemId server_id = 0;
	ClientItemId client_id = 0;
	ItemGroup_t group = ITEM_GROUP_NONE;
	ItemTypes_t type = ITEM_TYPE_NONE;
	uint64_t flags = 0;
	uint16_t volume = 0;
	uint16_t max_text_len = 0;
	uint16_t slot_position = SLOTP_HAND;
	uint8_t weapon_type = WEAPON_NONE;
	uint8_t classification = 0;
	uint16_t border_base_ground_id = 0;
	uint32_t border_group = 0;
	float weight = 0.0f;
	int attack = 0;
	int defense = 0;
	int armor = 0;
	uint32_t charges = 0;
	uint16_t rotate_to = 0;
	uint16_t way_speed = 100;
	int always_on_top_order = 0;
	BorderType border_alignment = BORDER_NONE;
	std::string name;
	std::string description;
	std::string editor_suffix;
};

struct XmlItemFragment {
	ServerItemId server_id = 0;
	std::optional<ClientItemId> client_id;
	std::string name;
	std::string editor_suffix;
	std::string description;
	std::optional<ItemGroup_t> group;
	std::optional<ItemTypes_t> type;
	uint64_t flags = 0;
	std::optional<uint16_t> volume;
	std::optional<uint16_t> max_text_len;
	std::optional<uint16_t> slot_position;
	std::optional<uint8_t> weapon_type;
	std::optional<float> weight;
	std::optional<int> attack;
	std::optional<int> defense;
	std::optional<int> armor;
	std::optional<uint32_t> charges;
	std::optional<uint16_t> rotate_to;
	std::optional<uint16_t> way_speed;
};

struct ResolvedItemDefinitionRow {
	ServerItemId server_id = 0;
	ClientItemId client_id = 0;
	ItemGroup_t group = ITEM_GROUP_NONE;
	ItemTypes_t type = ITEM_TYPE_NONE;
	uint64_t flags = 0;
	uint16_t volume = 0;
	uint16_t max_text_len = 0;
	uint16_t slot_position = SLOTP_HAND;
	uint8_t weapon_type = WEAPON_NONE;
	uint8_t classification = 0;
	uint16_t border_base_ground_id = 0;
	uint32_t border_group = 0;
	float weight = 0.0f;
	int attack = 0;
	int defense = 0;
	int armor = 0;
	uint32_t charges = 0;
	uint16_t rotate_to = 0;
	uint16_t way_speed = 100;
	int always_on_top_order = 0;
	BorderType border_alignment = BORDER_NONE;
	std::string name;
	std::string editor_suffix;
	std::string description;
	std::string passive_metadata_json;
};

struct ItemDefinitionVersionInfo {
	uint32_t major_version = 0;
	uint32_t minor_version = 0;
	uint32_t build_number = 0;
};

struct ItemDefinitionFragments {
	std::unordered_map<ClientItemId, DatItemFragment> dat;
	std::unordered_map<ServerItemId, OtbItemFragment> otb;
	std::unordered_map<ServerItemId, XmlItemFragment> xml;
	ItemDefinitionVersionInfo version;
};

struct ItemDefinitionLoadInput {
	ItemDefinitionMode mode = ItemDefinitionMode::DatOtb;
	wxFileName dat_path;
	wxFileName otb_path;
	wxFileName xml_path;
	std::vector<wxFileName> xml_paths;
	ClientVersion* client_version = nullptr;
	GraphicManager* graphics = nullptr;
	const DatCatalog* dat_catalog = nullptr;
	wxFileName ron_path;
};

#endif
