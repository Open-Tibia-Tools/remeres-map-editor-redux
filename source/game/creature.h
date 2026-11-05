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

#ifndef RME_CREATURE_H_
#define RME_CREATURE_H_

#include "game/creatures.h"
#include <list>
#include <vector>

enum Direction {
	NORTH = 0,
	EAST = 1,
	SOUTH = 2,
	WEST = 3,

	DIRECTION_FIRST = NORTH,
	DIRECTION_LAST = WEST
};

inline Direction& operator++(Direction& type) {
	return (type = static_cast<Direction>(static_cast<int>(type) + 1));
}
inline Direction operator++(Direction& type, int) {
	Direction tmp = type;
	++type;
	return tmp;
}

// Canary weighted spawns list several monsters on one tile; the server spawns one of them, picked by weight
struct SpawnAlternative {
	std::string name;
	int spawntime = 60;
	Direction direction = SOUTH;
	uint32_t weight = 0;
};

class Creature {
public:
	Creature(CreatureType* ctype);
	Creature(std::string type_name);
	~Creature();

	// Static conversions
	static std::string DirID2Name(uint16_t id);
	static uint16_t DirName2ID(std::string id);

	std::unique_ptr<Creature> deepCopy() const;

	const Outfit& getLookType() const;

	bool isSaved();
	void save();
	void reset();

	bool isSelected() const {
		return selected;
	}
	void deselect() {
		selected = false;
	}
	void select() {
		selected = true;
	}

	bool isNpc() const;

	const std::string& getName() const;
	CreatureBrush* getBrush() const;

	int getSpawnTime() const {
		return spawntime;
	}
	void setSpawnTime(int new_spawntime) {
		spawntime = new_spawntime;
	}

	Direction getDirection() const {
		return direction;
	}
	void setDirection(Direction new_direction) {
		direction = new_direction;
	}

	// Weight and alternatives are preserved across load/save (UI editing to be added)
	uint32_t getWeight() const {
		return weight;
	}
	void setWeight(uint32_t new_weight) {
		weight = new_weight;
	}
	const std::vector<SpawnAlternative>& getAlternatives() const {
		return alternatives;
	}
	std::vector<SpawnAlternative>& getAlternatives() {
		return alternatives;
	}
	void addAlternative(SpawnAlternative alternative) {
		alternatives.push_back(std::move(alternative));
	}
	// In partitioned spawn files (OTBM 5), NPC files can spawn NPCs that share their name with a monster type
	void markAsNpcSpawn() {
		npc_spawn = true;
	}

protected:
	std::string type_name;
	Direction direction;
	int spawntime;
	uint32_t weight = 0; // 0 when the spawn file has no weight attribute
	std::vector<SpawnAlternative> alternatives;
	bool npc_spawn = false;
	bool saved;
	bool selected;
};

inline void Creature::save() {
	saved = true;
}

inline void Creature::reset() {
	saved = false;
}

inline bool Creature::isSaved() {
	return saved;
}

inline bool Creature::isNpc() const {
	if (npc_spawn) {
		return true;
	}
	CreatureType* type = g_creatures[type_name];
	if (type) {
		return type->isNpc;
	}
	return false;
}

inline const std::string& Creature::getName() const {
	CreatureType* type = g_creatures[type_name];
	if (type) {
		return type->name;
	}
	return type_name;
}
inline CreatureBrush* Creature::getBrush() const {
	CreatureType* type = g_creatures[type_name];
	if (type) {
		return type->brush;
	}
	return nullptr;
}

using CreatureVector = std::vector<Creature*>;
using CreatureList = std::list<Creature*>;

#endif
