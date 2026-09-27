#include "item_definitions/core/item_definition_resolver.h"
#include "item_definitions/formats/dat/dat_catalog.h"

#include <algorithm>
#include <format>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
	constexpr uint64_t flagMask(ItemFlag flag) {
		return uint64_t { 1 } << static_cast<uint8_t>(flag);
	}

	constexpr bool isTooltipType(ItemTypes_t type) {
		switch (type) {
			case ITEM_TYPE_DEPOT:
			case ITEM_TYPE_MAILBOX:
			case ITEM_TYPE_TRASHHOLDER:
			case ITEM_TYPE_CONTAINER:
			case ITEM_TYPE_DOOR:
			case ITEM_TYPE_TELEPORT:
				return true;
			default:
				return false;
		}
	}

	constexpr bool isMetaItemOrLiquid(ServerItemId server_id, const XmlItemFragment& xml) {
		return !xml.client_id.has_value() && server_id > 0 && server_id < 100;
	}

	void finalizeDerivedProperties(ResolvedItemDefinitionRow& row) {
		if (row.type == ITEM_TYPE_NONE && row.volume > 0) {
			row.type = ITEM_TYPE_CONTAINER;
		}

		if (row.type == ITEM_TYPE_CONTAINER && row.group == ITEM_GROUP_NONE) {
			row.group = ITEM_GROUP_CONTAINER;
		}

		if (row.type == ITEM_TYPE_MAGICFIELD && row.group == ITEM_GROUP_NONE) {
			row.group = ITEM_GROUP_MAGICFIELD;
		}

		if (isTooltipType(row.type)) {
			row.flags |= flagMask(ItemFlag::Tooltipable);
		}
	}

	// Check if a DAT item is empty/invalid and should be skipped
	bool isDatItemEmptyOrInvalid(const DatItemFragment& dat, const DatCatalog* catalog, ClientItemId client_id) {
		// An item is considered "Empty" (placeholder) if ALL of these conditions are true:
		// 1. Has exactly 1 sprite AND that sprite ID is 0
		// 2. No minimap color
		// 3. No light
		// 4. No group assigned (ITEM_GROUP_NONE)
		// 5. No type assigned (ITEM_TYPE_NONE)
		if (catalog) {
			const auto* entry = catalog->entry(client_id);
			if (entry) {
				bool hasValidSprite = (entry->numsprites > 0 && !entry->sprite_ids.empty() && entry->sprite_ids[0] != 0);
				bool hasMinimapColor = (entry->minimap_color != 0);
				bool hasLight = entry->has_light;
				
				if (!hasValidSprite && !hasMinimapColor && !hasLight &&
					dat.group == ITEM_GROUP_NONE && dat.type == ITEM_TYPE_NONE) {
					return true; // This is a placeholder item
				}
			} else {
				// No catalog entry means no definition
				return true;
			}
		}

		return false;
	}
}

bool ItemDefinitionResolver::resolve(const ItemDefinitionLoadInput& input, const ItemDefinitionFragments& fragments, std::vector<ResolvedItemDefinitionRow>& rows, wxString& error, std::vector<std::string>& warnings, MissingItemReport* missingReport) {
	rows.clear();
	switch (input.mode) {
		// modes with otb
		case ItemDefinitionMode::DatOtb:
		case ItemDefinitionMode::ProtobufOtb:
			return resolveDatOtb(input, fragments, rows, error, warnings, missingReport);

		// modes without otb
		case ItemDefinitionMode::DatOnly:
		case ItemDefinitionMode::ProtobufOnly:
			return resolveDatOnly(input, fragments, rows, error, warnings, missingReport);

		// items.srv
		case ItemDefinitionMode::DatSrv:
			error = "Selected item definition mode is not implemented yet.";
			return false;

		case ItemDefinitionMode::DatRon:
			return resolveDatRon(input, fragments, rows, error, warnings, missingReport);
	}
	error = "Unknown item definition mode.";
	return false;
}

bool ItemDefinitionResolver::resolveDatOtb(const ItemDefinitionLoadInput& input, const ItemDefinitionFragments& fragments, std::vector<ResolvedItemDefinitionRow>& rows, wxString& error, std::vector<std::string>& warnings, MissingItemReport* missingReport) {
	rows.reserve(fragments.otb.size());

	// Track which DAT client IDs are referenced by OTB
	std::unordered_set<ClientItemId> dat_ids_referenced_by_otb;

	for (const auto& [server_id, otb] : fragments.otb) {
		ResolvedItemDefinitionRow row;
		row.server_id = server_id;
		row.group = otb.group;
		row.type = otb.type;
		row.flags = otb.flags;
		row.volume = otb.volume;
		row.max_text_len = otb.max_text_len;
		row.slot_position = otb.slot_position;
		row.weapon_type = otb.weapon_type;
		row.classification = otb.classification;
		row.border_base_ground_id = otb.border_base_ground_id;
		row.border_group = otb.border_group;
		row.weight = otb.weight;
		row.attack = otb.attack;
		row.defense = otb.defense;
		row.armor = otb.armor;
		row.charges = otb.charges;
		row.rotate_to = otb.rotate_to;
		row.way_speed = otb.way_speed;
		row.always_on_top_order = otb.always_on_top_order;
		row.border_alignment = otb.border_alignment;
		row.name = otb.name;
		row.description = otb.description;

		const auto xml_it = fragments.xml.find(server_id);
		const ClientItemId effective_client_id = (xml_it != fragments.xml.end() && xml_it->second.client_id.has_value()) ? *xml_it->second.client_id : otb.client_id;
		if (effective_client_id == 0) {
			warnings.push_back(std::format("Skipping items.otb entry {} because it does not define a DAT client id.", server_id));
			continue;
		}

		row.client_id = effective_client_id;

		const auto dat_it = fragments.dat.find(effective_client_id);
		if (dat_it == fragments.dat.end()) {
			// DAT item truly doesn't exist - collect as missing
			if (missingReport) {
				missingReport->missing_in_dat.push_back({
					.server_id = server_id,
					.client_id = effective_client_id,
					.name = otb.name,
					.description = otb.description
				});
			} else {
				error = wxString::FromUTF8(std::format("Missing DAT definition for client id {} (server id {}).", effective_client_id, server_id));
				return false;
			}
			// Continue processing other items
			continue;
		}

		// OTB has a mapping to DAT. This is a valid item regardless of whether it's empty or not.
		// We add it to the rows so the editor recognizes the ID.
		dat_ids_referenced_by_otb.insert(effective_client_id);
		row.flags |= dat_it->second.flags & ~flagMask(ItemFlag::Moveable);
		row.passive_metadata_json = dat_it->second.passive_metadata_json;

		if (xml_it != fragments.xml.end()) {
			applyXmlOverrides(xml_it->second, row);
		}
		finalizeDerivedProperties(row);

		rows.push_back(std::move(row));
	}

	// Collect DAT items not referenced by OTB
	if (missingReport) {
		for (const auto& [client_id, dat] : fragments.dat) {
			// Skip empty/invalid DAT items
			if (isDatItemEmptyOrInvalid(dat, input.dat_catalog, client_id)) {
				continue;
			}
			if (!dat_ids_referenced_by_otb.contains(client_id)) {
				missingReport->missing_in_otb.push_back({
					.server_id = 0,
					.client_id = client_id,
					.name = "",
					.description = ""
				});
			}
		}

		// Collect XML entries that reference non-existent OTB server IDs
		for (const auto& [server_id, xml] : fragments.xml) {
			// Skip server-side fluid types and special items (IDs < 100)
			if (server_id < 100) {
				continue;
			}
			// If XML server_id already exists in OTB, it's valid
			if (fragments.otb.contains(server_id)) {
				continue;
			}
			// If XML entry has a client_id override that maps to an existing OTB entry, it's valid
			if (xml.client_id.has_value()) {
				bool foundInOtb = std::ranges::any_of(fragments.otb, [&](const auto& p) {
					return p.second.client_id == xml.client_id.value();
				});
				if (foundInOtb) {
					continue;
				}
			}
			missingReport->xml_no_otb.push_back({
				.server_id = server_id,
				.client_id = xml.client_id.value_or(0),
				.name = xml.name,
				.description = xml.description
			});
		}

		// Collect OTB entries that don't have XML entries (informational)
		for (const auto& [server_id, otb] : fragments.otb) {
			// Skip OTB entries with no client ID
			if (otb.client_id == 0) {
				continue;
			}
			// Skip entries where DAT definition doesn't exist (already reported)
			if (!fragments.dat.contains(otb.client_id)) {
				continue;
			}
			// Skip entries that have XML overrides
			if (fragments.xml.find(server_id) != fragments.xml.end()) {
				continue;
			}
			missingReport->otb_no_xml.push_back({
				.server_id = server_id,
				.client_id = otb.client_id,
				.name = otb.name,
				.description = otb.description
			});
		}
	}

	if (rows.empty()) {
		error = "No item definitions were resolved from DAT/OTB/XML.";
		return false;
	}
	return true;
}

bool ItemDefinitionResolver::resolveDatRon(const ItemDefinitionLoadInput& input, const ItemDefinitionFragments& fragments, std::vector<ResolvedItemDefinitionRow>& rows, wxString& error, std::vector<std::string>& warnings, MissingItemReport* missingReport) {
	(void)warnings;
	rows.reserve(fragments.otb.size());
	std::unordered_set<ClientItemId> referenced_client_ids;

	for (const auto& [server_id, ron] : fragments.otb) {
		if (ron.client_id == 0) {
			continue;
		}

		const auto dat_it = fragments.dat.find(ron.client_id);
		if (dat_it == fragments.dat.end()) {
			if (missingReport) {
				missingReport->missing_in_dat.push_back({
					.server_id = server_id,
					.client_id = ron.client_id,
					.name = ron.name,
					.description = ron.description
				});
				continue;
			}
			error = wxString::FromUTF8(std::format("Missing DAT definition for client id {}.", ron.client_id));
			return false;
		}

		ResolvedItemDefinitionRow row;
		row.server_id = server_id;
		row.client_id = ron.client_id;
		row.group = ron.group;
		row.type = ron.type;
		row.flags = ron.flags;
		row.volume = ron.volume;
		row.max_text_len = ron.max_text_len;
		row.slot_position = ron.slot_position;
		row.weapon_type = ron.weapon_type;
		row.weight = ron.weight;
		row.attack = ron.attack;
		row.defense = ron.defense;
		row.armor = ron.armor;
		row.charges = ron.charges;
		row.rotate_to = ron.rotate_to;
		row.way_speed = ron.way_speed;
		row.always_on_top_order = ron.always_on_top_order;
		row.name = ron.name;
		row.editor_suffix = ron.editor_suffix;
		row.description = ron.description;
		row.flags |= dat_it->second.flags & ~flagMask(ItemFlag::Moveable);
		row.passive_metadata_json = dat_it->second.passive_metadata_json;
		finalizeDerivedProperties(row);
		referenced_client_ids.insert(ron.client_id);
		rows.push_back(std::move(row));
	}

	if (missingReport) {
		for (const auto& [client_id, dat] : fragments.dat) {
			if (isDatItemEmptyOrInvalid(dat, input.dat_catalog, client_id)) {
				continue;
			}
			if (!referenced_client_ids.contains(client_id)) {
				missingReport->missing_in_otb.push_back({
					.server_id = 0,
					.client_id = client_id,
					.name = "",
					.description = ""
				});
			}
		}
	}

	if (rows.empty()) {
		error = "No item definitions were resolved from DAT/items.ron.";
		return false;
	}
	return true;
}

bool ItemDefinitionResolver::resolveDatOnly(const ItemDefinitionLoadInput& input, const ItemDefinitionFragments& fragments, std::vector<ResolvedItemDefinitionRow>& rows, wxString& error, std::vector<std::string>& warnings, MissingItemReport* missingReport) {
	rows.reserve(fragments.dat.size());

	std::unordered_map<ClientItemId, size_t> client_to_row;
	client_to_row.reserve(fragments.dat.size());
	std::unordered_map<ClientItemId, ServerItemId> xml_server_by_client;
	xml_server_by_client.reserve(fragments.xml.size());

	// Track which DAT client IDs are used by XML
	std::unordered_set<ClientItemId> dat_ids_used_by_xml;

	// 1. Load all items from tibia.dat (Source of Truth)
	for (const auto& [client_id, dat] : fragments.dat) {
		ResolvedItemDefinitionRow row;
		row.server_id = client_id;
		row.client_id = client_id;
		row.group = dat.group;
		row.type = dat.type;
		row.flags = dat.flags;
		row.way_speed = dat.way_speed;
		row.always_on_top_order = dat.always_on_top_order;
		if (dat.max_text_len.has_value()) {
			row.max_text_len = *dat.max_text_len;
		}
		if (dat.slot_position.has_value()) {
			row.slot_position = *dat.slot_position;
		}
		row.passive_metadata_json = dat.passive_metadata_json;
		rows.push_back(std::move(row));
		client_to_row[client_id] = rows.size() - 1;
	}

	// 2. Apply items.xml overrides
	for (const auto& [server_id, xml] : fragments.xml) {
		// Skip server-side fluid types and special items (IDs < 100)
		if (isMetaItemOrLiquid(server_id, xml)) {
			continue;
		}

		const ClientItemId client_id = xml.client_id.value_or(server_id);
		const auto row_it = client_to_row.find(client_id);
		
		if (row_it == client_to_row.end()) {
			// XML item is missing from tibia.dat.
			// Rule: Report only when id >= 100.
			if (missingReport) {
				missingReport->missing_in_dat.push_back({
					.server_id = server_id,
					.client_id = client_id,
					.name = xml.name,
					.description = xml.description
				});
			} else {
				warnings.push_back(std::format("Skipping items.xml entry {} in dat/protobuf only mode because DAT client id {} is missing.", server_id, client_id));
			}
			continue;
		}

		// DAT item exists. Handle duplicate XML mappings.
		const auto owner_it = xml_server_by_client.find(client_id);
		if (owner_it != xml_server_by_client.end() && owner_it->second != server_id) {
			warnings.push_back(std::format("Duplicate items.xml client id {} in dat/protobuf only mode for server ids {} and {}. Keeping the first mapping.", client_id, owner_it->second, server_id));
			continue;
		}
		if (owner_it == xml_server_by_client.end()) {
			xml_server_by_client.emplace(client_id, server_id);
			dat_ids_used_by_xml.insert(client_id);
		}

		ResolvedItemDefinitionRow& row = rows[row_it->second];
		row.server_id = server_id;
		applyXmlOverrides(xml, row);
	}

	for (auto& row : rows) {
		finalizeDerivedProperties(row);
	}

	// 3. Report items in tibia.dat that are missing from items.xml
	// Rule: Report if item is NOT empty. Skip if item is empty.
	if (missingReport) {
		for (const auto& [client_id, dat] : fragments.dat) {
			if (!dat_ids_used_by_xml.contains(client_id)) {
				if (!isDatItemEmptyOrInvalid(dat, input.dat_catalog, client_id)) {
					missingReport->missing_in_otb.push_back({
						.server_id = 0,
						.client_id = client_id,
						.name = "",
						.description = ""
					});
				}
			}
		}
	}

	if (rows.empty()) {
		error = "No item definitions were resolved from DAT/protobuf/XML.";
		return false;
	}

	return true;
}

void ItemDefinitionResolver::applyXmlOverrides(const XmlItemFragment& xml, ResolvedItemDefinitionRow& row) {
	if (xml.client_id.has_value()) {
		row.client_id = *xml.client_id;
	}
	if (!xml.name.empty()) {
		row.name = xml.name;
	}
	if (!xml.editor_suffix.empty()) {
		row.editor_suffix = xml.editor_suffix;
	}
	if (!xml.description.empty()) {
		row.description = xml.description;
	}
	if (xml.group.has_value()) {
		row.group = *xml.group;
	}
	if (xml.type.has_value()) {
		row.type = *xml.type;
	}
	row.flags |= xml.flags;
	if (xml.volume.has_value()) {
		row.volume = *xml.volume;
	}
	if (xml.max_text_len.has_value()) {
		row.max_text_len = *xml.max_text_len;
	}
	if (xml.slot_position.has_value()) {
		row.slot_position = *xml.slot_position;
	}
	if (xml.weapon_type.has_value()) {
		row.weapon_type = *xml.weapon_type;
	}
	if (xml.weight.has_value()) {
		row.weight = *xml.weight;
	}
	if (xml.attack.has_value()) {
		row.attack = *xml.attack;
	}
	if (xml.defense.has_value()) {
		row.defense = *xml.defense;
	}
	if (xml.armor.has_value()) {
		row.armor = *xml.armor;
	}
	if (xml.charges.has_value()) {
		row.charges = *xml.charges;
	}
	if (xml.rotate_to.has_value()) {
		row.rotate_to = *xml.rotate_to;
	}
	if (xml.way_speed.has_value()) {
		row.way_speed = *xml.way_speed;
	}
}
