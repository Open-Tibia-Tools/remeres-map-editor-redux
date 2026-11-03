//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_DATA_CREATURE_PLACEMENT_H_
#define RME_IO_SEC_DATA_CREATURE_PLACEMENT_H_

// Puts the server's monster homes (dat/monster.db) and NPCs (npc/*.npc) on
// the map, for monster_files.h and npc_files.h: each one is a creature on
// its home tile carrying the home's radius and amount (see game/srv_homes.h).

#include "io/sec/sec_sector_parser.h"
#include "io/sec/srv_creature_files.h"

#include <string>
#include <vector>

class Map;
class Tile;
class CreatureType;

namespace rme::sec {

	// The creature type named `name`, added with the server's outfit when the
	// editor does not know it yet.
	[[nodiscard]] CreatureType* serverCreatureType(const std::string& name, bool is_npc, const CreatureOutfit& look);

	// The server's monsters and NPCs as the Creature palette, so they can be
	// placed and picked from the map ("Select Creature" needs their brushes).
	void addServerCreaturePalette(std::vector<CreatureType*> monsters, std::vector<CreatureType*> npcs);

	class CreaturePlacer {
	public:
		explicit CreaturePlacer(Map& map);

		// A monster home or NPC as one creature carrying the home's radius and
		// amount: on the home tile (trap creatures sit inside walls there), or
		// on the nearest free walkable tile within its radius when another
		// creature already stands on it. Returns `amount` when placed, 0 otherwise.
		size_t place(CreatureType* type, bool is_npc, const AbsolutePosition& home, int radius, int amount, int spawn_time);

		// What could not be placed.
		void report(std::vector<std::string>& warnings) const;

	private:
		Tile* findFreeTile(const AbsolutePosition& home, int radius) const;
		void noteUnplaced(const CreatureType* type, const AbsolutePosition& home, int radius, size_t count);

		Map& map_;
		size_t unplaced_ = 0;
		size_t unplaced_homes_ = 0;
		std::vector<std::string> unplaced_lines_;
	};

} // namespace rme::sec

#endif
