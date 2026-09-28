//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "brushes/ground/auto_border.h"
#include "brushes/brush_enums.h"
#include "brushes/ground/ground_brush.h"
#include "item_definitions/core/item_definition_store.h"
#include "game/material_server_ids.h"
#include "ext/pugixml.hpp"
#include <wx/string.h>
#include <utility>

AutoBorder::AutoBorder(int _id) :
	id(_id), group(0), ground(false) {
	tiles.fill(0);
}

int AutoBorder::edgeNameToID(std::string_view edgename) {
	static constexpr std::pair<std::string_view, int> edges[] = {
		{ "n", NORTH_HORIZONTAL },
		{ "w", WEST_HORIZONTAL },
		{ "s", SOUTH_HORIZONTAL },
		{ "e", EAST_HORIZONTAL },
		{ "cnw", NORTHWEST_CORNER },
		{ "cne", NORTHEAST_CORNER },
		{ "csw", SOUTHWEST_CORNER },
		{ "cse", SOUTHEAST_CORNER },
		{ "dnw", NORTHWEST_DIAGONAL },
		{ "dne", NORTHEAST_DIAGONAL },
		{ "dsw", SOUTHWEST_DIAGONAL },
		{ "dse", SOUTHEAST_DIAGONAL }
	};

	for (const auto& [name, id] : edges) {
		if (name == edgename) {
			return id;
		}
	}
	return BORDER_NONE;
}

bool AutoBorder::load(pugi::xml_node node, std::vector<std::string>& warnings, GroundBrush* owner, uint16_t border_base_ground_id) {
	ASSERT(ground ? border_base_ground_id != 0 : true);

	pugi::xml_attribute attribute;

	bool optionalBorder = false;
	if ((attribute = node.attribute("type"))) {
		if (std::string_view(attribute.as_string()) == "optional") {
			optionalBorder = true;
		}
	}

	if ((attribute = node.attribute("group"))) {
		group = attribute.as_ushort();
	}

	for (pugi::xml_node childNode = node.first_child(); childNode; childNode = childNode.next_sibling()) {
		if (!(attribute = childNode.attribute("item"))) {
			continue;
		}

		uint16_t itemid = materialClientId(attribute.as_ushort());
		if (!(attribute = childNode.attribute("edge"))) {
			continue;
		}

		const std::string_view orientation = attribute.as_string();

		if (!g_item_definitions.exists(itemid)) {
			warnings.push_back("Invalid item ID " + std::to_string(itemid) + " for border " + std::to_string(id));
			continue;
		}

		if (ground) { // We are a ground border
			g_item_definitions.setGroup(itemid, ITEM_GROUP_NONE);
			g_item_definitions.setAttribute(itemid, ItemAttributeKey::BorderBaseGroundId, border_base_ground_id);
			g_item_definitions.mutableEditorData(itemid).brush = owner;
		}

		g_item_definitions.setFlag(itemid, ItemFlag::AlwaysOnBottom, true);
		g_item_definitions.setFlag(itemid, ItemFlag::IsBorder, true);
		if (optionalBorder) {
			g_item_definitions.setFlag(itemid, ItemFlag::IsOptionalBorder, true);
		}
		if (group && g_item_definitions.get(itemid).attribute(ItemAttributeKey::BorderGroup) == 0) {
			g_item_definitions.setAttribute(itemid, ItemAttributeKey::BorderGroup, group);
		}

		int32_t edge_id = edgeNameToID(orientation);
		if (edge_id != BORDER_NONE) {
			tiles[edge_id] = itemid;
			if (g_item_definitions.get(itemid).attribute(ItemAttributeKey::BorderAlignment) == BORDER_NONE) {
				g_item_definitions.setAttribute(itemid, ItemAttributeKey::BorderAlignment, edge_id);
			}
		}
	}
	return true;
}
