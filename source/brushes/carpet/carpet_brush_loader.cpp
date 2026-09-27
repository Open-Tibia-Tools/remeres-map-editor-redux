//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "brushes/carpet/carpet_brush_loader.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/ground/auto_border.h"
#include "item_definitions/core/item_definition_store.h"
#include "game/material_server_ids.h"
#include <string_view>
#include <charconv>
#include <algorithm>
#include <cctype>

// Helper for C++20 case-insensitive comparison (zero allocation)
static const auto iequal = [](char a, char b) {
	return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
};

bool CarpetBrushLoader::load(CarpetBrush& brush, pugi::xml_node node, std::vector<std::string>& warnings) {
	pugi::xml_attribute attribute;
	if ((attribute = node.attribute("lookid"))) {
		brush.look_id = attribute.as_ushort();
	}

	if ((attribute = node.attribute("server_lookid"))) {
		const auto definition = g_item_definitions.get(materialClientId(attribute.as_ushort()));
		if (!definition) {
			warnings.push_back("Invalid server_lookid " + std::to_string(attribute.as_ushort()) + " for carpet brush");
		} else {
			brush.look_id = definition.clientId();
		}
	}

	for (pugi::xml_node childNode : node.children()) {
		if (!std::ranges::equal(std::string_view(childNode.name()), std::string_view("carpet"), iequal)) {
			continue;
		}

		uint32_t alignment;
		if ((attribute = childNode.attribute("align"))) {
			std::string alignString = attribute.as_string();
			alignment = AutoBorder::edgeNameToID(alignString);
			if (alignment == BORDER_NONE) {
				if (alignString == "center") {
					alignment = CARPET_CENTER;
				} else {
					warnings.push_back("Invalid alignment of carpet node\n");
					continue;
				}
			}
		} else {
			warnings.push_back("Could not read alignment tag of carpet node\n");
			continue;
		}

		bool use_local_id = true;
		for (pugi::xml_node subChildNode : childNode.children()) {
			if (!std::ranges::equal(std::string_view(subChildNode.name()), std::string_view("item"), iequal)) {
				continue;
			}

			use_local_id = false;
			if (!(attribute = subChildNode.attribute("id"))) {
				warnings.push_back("Could not read id tag of item node\n");
				continue;
			}

			int32_t id = materialClientId(static_cast<uint16_t>(attribute.as_uint()));
			if (id <= 0) {
				warnings.push_back("Invalid id for item node: " + std::to_string(id));
				continue;
			}
			if (!(attribute = subChildNode.attribute("chance"))) {
				warnings.push_back("Could not read chance tag of item node\n");
				continue;
			}

			int32_t chance = attribute.as_int();

			const auto definition = g_item_definitions.get(id);
			if (!definition) {
				warnings.push_back("There is no itemtype with id " + std::to_string(id));
				continue;
			} else if (definition.editorData().brush && definition.editorData().brush != &brush) {
				warnings.push_back("Itemtype id " + std::to_string(id) + " already has a brush");
				continue;
			}

			g_item_definitions.setFlag(id, ItemFlag::IsCarpet, true);
			g_item_definitions.mutableEditorData(id).brush = &brush;

			brush.m_items.addItem(static_cast<BorderType>(alignment), id, chance);
		}

		if (use_local_id) {
			if (!(attribute = childNode.attribute("id"))) {
				warnings.push_back("Could not read id tag of carpet node\n");
				continue;
			}

			uint16_t id = materialClientId(attribute.as_ushort());

			const auto definition = g_item_definitions.get(id);
			if (!definition) {
				warnings.push_back("There is no itemtype with id " + std::to_string(id));
				return false;
			} else if (definition.editorData().brush && definition.editorData().brush != &brush) {
				warnings.push_back("Itemtype id " + std::to_string(id) + " already has a brush");
				return false;
			}

			g_item_definitions.setFlag(id, ItemFlag::IsCarpet, true);
			g_item_definitions.mutableEditorData(id).brush = &brush;

			brush.m_items.addItem(static_cast<BorderType>(alignment), id, 1);
		}
	}
	return true;
}
