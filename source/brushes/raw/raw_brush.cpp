//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "brushes/raw/raw_brush.h"
#include "app/settings.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/basemap.h"
#include <format>

//=============================================================================
// RAW brush

RAWBrush::RAWBrush(ServerItemId itemid) :
	Brush(),
	item_id(itemid) {
}

RAWBrush::~RAWBrush() {
	////
}

int RAWBrush::getLookID() const {
	if (const auto definition = g_item_definitions.get(item_id)) {
		return definition.clientId();
	}
	return 0;
}

ServerItemId RAWBrush::getItemID() const {
	return item_id;
}

std::string RAWBrush::getName() const {
	const auto definition = g_item_definitions.get(item_id);
	if (!definition) {
		return "RAWBrush";
	}

	if (definition.hasFlag(ItemFlag::HookSouth)) {
		return std::format("{} - {} (Hook South)", item_id, definition.name());
	} else if (definition.hasFlag(ItemFlag::HookEast)) {
		return std::format("{} - {} (Hook East)", item_id, definition.name());
	}

	return std::format("{} - {}{}", item_id, definition.name(), definition.editorSuffix());
}

void RAWBrush::undraw(BaseMap* map, Tile* tile) {
	if (tile->ground && tile->ground->getID() == item_id) {
		tile->ground = nullptr;
	}
	std::erase_if(tile->items, [brush_item_id = item_id](const auto& item) {
		return item->getID() == brush_item_id;
	});
}

void RAWBrush::draw(BaseMap* map, Tile* tile, void* parameter) {
	const auto definition = g_item_definitions.get(item_id);
	if (!definition) {
		return;
	}

	bool b = parameter ? *reinterpret_cast<bool*>(parameter) : false;
	if ((g_settings.getInteger(Config::RAW_LIKE_SIMONE) && !b) && definition.hasFlag(ItemFlag::AlwaysOnBottom) && definition.attribute(ItemAttributeKey::AlwaysOnTopOrder) == 2) {
		std::erase_if(tile->items, [topOrder = static_cast<int>(definition.attribute(ItemAttributeKey::AlwaysOnTopOrder))](const auto& item) {
			return item->getTopOrder() == topOrder;
		});
	}
	tile->addItem(Item::Create(item_id));
}
