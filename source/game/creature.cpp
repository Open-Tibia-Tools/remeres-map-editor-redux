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

#include "game/creature.h"
#include <array>
#include <string_view>
#include <unordered_map>

Creature::Creature(CreatureType* ctype) :
	direction(SOUTH), spawntime(0), saved(false), selected(false) {
	if (ctype) {
		type_name = ctype->name;
	}
}

Creature::Creature(std::string ctype_name) :
	type_name(std::move(ctype_name)), direction(SOUTH), spawntime(0), saved(false), selected(false) {
	////
}

Creature::~Creature() {
	////
}

std::string Creature::DirID2Name(uint16_t id) {
	static constexpr std::array<std::string_view, 4> dir_names = {
		"North", "East", "South", "West"
	};

	if (id < dir_names.size()) {
		return std::string(dir_names[id]);
	}
	return "Unknown";
}

uint16_t Creature::DirName2ID(std::string dir) {
	to_lower_str(dir);

	static const std::unordered_map<std::string, uint16_t> dir_map = {
		{ "north", NORTH },
		{ "east", EAST },
		{ "south", SOUTH },
		{ "west", WEST }
	};

	auto it = dir_map.find(dir);
	if (it != dir_map.end()) {
		return it->second;
	}
	return SOUTH;
}

std::unique_ptr<Creature> Creature::deepCopy() const {
	std::unique_ptr<Creature> copy = std::make_unique<Creature>(type_name);
	copy->spawntime = spawntime;
	copy->direction = direction;
	copy->selected = selected;
	copy->saved = saved;
	return copy;
}

const Outfit& Creature::getLookType() const {
	CreatureType* type = g_creatures[type_name];
	if (type) {
		return type->outfit;
	}
	return DEFAULT_UNKNOWN_CREATURE_OUTFIT;
}
