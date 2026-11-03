//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_DATA_MONSTER_FILES_H_
#define RME_IO_SEC_DATA_MONSTER_FILES_H_

// The server's monsters: their races (mon/*.mon, read-only) and their homes
// (dat/monster.db), which are the map's monster spawns.

#include "io/sec/data/server_data_folder.h"

#include <map>
#include <string>
#include <vector>

class Map;
class Tile;
class CreatureType;

namespace rme::sec {

	class CreaturePlacer;

	namespace monster_files {

		struct Races {
			std::map<int, CreatureType*> by_race; // race number -> its creature type
			std::vector<CreatureType*> types;
		};

		// mon/*.mon as creature types, named as monster_files::save names them back.
		[[nodiscard]] Races loadRaces(const ServerDataFolder& folder, std::vector<std::string>& warnings);

		// Places the homes of dat/monster.db.
		void loadHomes(const ServerDataFolder& folder, const Races& races, CreaturePlacer& placer, std::vector<std::string>& warnings);

		// Writes the monsters on `tiles` to dat/monster.db, when they changed;
		// creates it once the map has monsters.
		void save(Map& map, const std::vector<Tile*>& tiles, const ServerDataFolder& folder, ServerFileWriter& writer, std::vector<std::string>& warnings);

	} // namespace monster_files

} // namespace rme::sec

#endif
