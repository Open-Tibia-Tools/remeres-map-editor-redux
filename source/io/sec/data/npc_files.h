//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_DATA_NPC_FILES_H_
#define RME_IO_SEC_DATA_NPC_FILES_H_

// The server's NPCs (npc/*.npc): each file holds an NPC's outfit, home and
// dialogue. The map places each one on its home tile; saving moves homes.

#include "io/sec/data/server_data_folder.h"

#include <string>
#include <vector>

class Map;
class Tile;
class CreatureType;

namespace rme::sec {

	class CreaturePlacer;

	namespace npc_files {

		// Places the NPCs; returns their creature types.
		[[nodiscard]] std::vector<CreatureType*> load(const ServerDataFolder& folder, CreaturePlacer& placer, std::vector<std::string>& warnings);

		// Writes the home and radius of the NPCs on `tiles` back to their files.
		// An NPC needs its file (its dialogue), so one without a file is not
		// saved; one whose home tile holds another creature keeps its home.
		void save(const Map& map, const std::vector<Tile*>& tiles, const ServerDataFolder& folder, ServerFileWriter& writer, std::vector<std::string>& warnings);

	} // namespace npc_files

} // namespace rme::sec

#endif
