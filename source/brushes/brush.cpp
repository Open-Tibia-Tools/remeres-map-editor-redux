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

#include "brushes/brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/ground/auto_border.h"
#include "brushes/house/house_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/managers/brush_manager.h"

#include "brushes/flag/flag_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/border/optional_border_brush.h"

#include "app/settings.h"

#include "game/sprites.h"

#include "game/item.h"
#include "game/complexitem.h"
#include "game/creatures.h"
#include "game/creature.h"
#include "map/map.h"

#include "ui/gui.h"

#include <ranges>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <format>
#include <memory>

Brushes g_brushes;

Brushes::Brushes() {
	////
}

Brushes::~Brushes() {
	////
}

void Brushes::clear() {
	for (auto& entry : brushes) {
		entry.second.reset();
	}
	brushes.clear();

	for (auto& entry : borders) {
		entry.second.reset();
	}
	borders.clear();
}

void Brushes::init() {
	addManagedBrush(g_brush_manager.optional_brush);
	addManagedBrush(g_brush_manager.eraser);
	addManagedBrush(g_brush_manager.spawn_brush);

	addManagedBrush(g_brush_manager.normal_door_brush, WALL_DOOR_NORMAL);
	addManagedBrush(g_brush_manager.locked_door_brush, WALL_DOOR_LOCKED);
	addManagedBrush(g_brush_manager.magic_door_brush, WALL_DOOR_MAGIC);
	addManagedBrush(g_brush_manager.quest_door_brush, WALL_DOOR_QUEST);
	addManagedBrush(g_brush_manager.hatch_door_brush, WALL_HATCH_WINDOW);
	addManagedBrush(g_brush_manager.archway_door_brush, WALL_ARCHWAY);
	addManagedBrush(g_brush_manager.normal_door_alt_brush, WALL_DOOR_NORMAL_ALT);
	addManagedBrush(g_brush_manager.window_door_brush, WALL_WINDOW);

	addManagedBrush(g_brush_manager.house_brush);
	addManagedBrush(g_brush_manager.house_exit_brush);
	addManagedBrush(g_brush_manager.waypoint_brush);

	addManagedBrush(g_brush_manager.pz_brush, TILESTATE_PROTECTIONZONE);
	addManagedBrush(g_brush_manager.rook_brush, TILESTATE_NOPVP);
	addManagedBrush(g_brush_manager.nolog_brush, TILESTATE_NOLOGOUT);
	addManagedBrush(g_brush_manager.pvp_brush, TILESTATE_PVPZONE);
	addManagedBrush(g_brush_manager.refresh_brush, TILESTATE_REFRESH);

	GroundBrush::init();
	WallBrush::init();
	TableBrush::init();
	CarpetBrush::init();
}

bool Brushes::unserializeBrush(pugi::xml_node node, std::vector<std::string>& warnings) {
	pugi::xml_attribute attribute;
	if (!(attribute = node.attribute("name"))) {
		warnings.push_back("Brush node without name.");
		return false;
	}

	const std::string& brushName = attribute.as_string();
	if (brushName == "all" || brushName == "none") {
		warnings.push_back(std::format("Using reserved brushname \"{}\".", brushName));
		return false;
	}

	Brush* brush = getBrush(brushName);
	std::unique_ptr<Brush> newBrush;

	if (!brush) {
		attribute = node.attribute("type");
		if (!attribute) {
			warnings.push_back("Couldn't read brush type");
			return false;
		}

		const std::string_view brushType = attribute.as_string();

		static const std::unordered_map<std::string_view, std::function<std::unique_ptr<Brush>()>> typeMap = {
			{ "border", [] { return std::make_unique<GroundBrush>(); } },
			{ "ground", [] { return std::make_unique<GroundBrush>(); } },
			{ "wall", [] { return std::make_unique<WallBrush>(); } },
			{ "wall decoration", [] { return std::make_unique<WallDecorationBrush>(); } },
			{ "carpet", [] { return std::make_unique<CarpetBrush>(); } },
			{ "table", [] { return std::make_unique<TableBrush>(); } },
			{ "doodad", [] { return std::make_unique<DoodadBrush>(); } }
		};

		if (auto it = typeMap.find(brushType); it != typeMap.end()) {
			newBrush = it->second();
			brush = newBrush.get();
		} else {
			warnings.push_back(std::format("Unknown brush type {}", brushType));
			return false;
		}

		ASSERT(brush);
		brush->setName(brushName);
	}

	if (!node.first_child()) {
		if (newBrush) {
			brushes.emplace(brush->getName(), std::move(newBrush));
		}
		return true;
	}

	std::vector<std::string> subWarnings;
	brush->load(node, subWarnings);

	if (!subWarnings.empty()) {
		warnings.push_back((wxString("Errors while loading brush \"") << wxstr(brush->getName()) << "\"").ToStdString());
		warnings.insert(warnings.end(), subWarnings.begin(), subWarnings.end());
	}

	if (brush->getName() == "all" || brush->getName() == "none") {
		warnings.push_back(std::format("Using reserved brushname '{}'.", brush->getName()));
		// newBrush will delete automatically if set
		return false;
	}

	Brush* otherBrush = getBrush(brush->getName());
	if (otherBrush) {
		if (otherBrush != brush) {
			warnings.push_back(std::string(wxstr(std::format("Duplicate brush name {}. Undefined behaviour may ensue.", brush->getName())).mb_str()));
		} else {
			// Don't insert
			return true;
		}
	}

	if (newBrush) {
		brushes.emplace(brush->getName(), std::move(newBrush));
	}
	return true;
}

bool Brushes::unserializeBorder(pugi::xml_node node, std::vector<std::string>& warnings) {
	pugi::xml_attribute attribute = node.attribute("id");
	if (!attribute) {
		warnings.push_back("Couldn't read border id node");
		return false;
	}

	uint32_t id = attribute.as_uint();
	if (borders.contains(id)) {
		warnings.push_back(std::string(wxstr(std::format("Border ID {} already exists", id)).mb_str()));
		return false;
	}

	auto border = std::make_unique<AutoBorder>(id);
	border->load(node, warnings);
	borders[id] = std::move(border);
	return true;
}

void Brushes::addBrush(std::unique_ptr<Brush> brush) {
	const std::string name = brush->getName();
	brushes.emplace(name, std::move(brush));
}

Brush* Brushes::getBrush(std::string_view name) const {
	if (auto it = brushes.find(name); it != brushes.end()) {
		return it->second.get();
	}
	return nullptr;
}

// Brush
uint32_t Brush::id_counter = 0;
Brush::Brush() :
	id(++id_counter), visible(false), usesCollection(false) {
	////
}

Brush::~Brush() {
	////
}

// TerrainBrush
TerrainBrush::TerrainBrush() :
	look_id(0), hate_friends(false) {
	////
}

TerrainBrush::~TerrainBrush() {
	////
}

bool TerrainBrush::friendOf(TerrainBrush* other) {
	const uint32_t borderID = other->getID();
	const bool found = std::ranges::any_of(friends, [borderID](uint32_t friendId) {
		return friendId == borderID || friendId == 0xFFFFFFFF;
	});
	return found ? !hate_friends : hate_friends;
}
