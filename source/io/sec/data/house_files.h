//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_DATA_HOUSE_FILES_H_
#define RME_IO_SEC_DATA_HOUSE_FILES_H_

// The server's houses: dat/houseareas.dat (the areas, each with its price
// per square and its town's depot) and dat/houses.dat (the houses).

#include "io/sec/data/server_data_folder.h"

#include <string>
#include <vector>

class Map;

namespace rme::sec::house_files {

	// Reads the houses into the map; their towns come from map.dat (map_dat::load).
	void load(Map& map, const ServerDataFolder& folder, std::vector<std::string>& warnings);

	// Writes the map's houses back to houses.dat, when they changed. A town
	// without a house area gets one in houseareas.dat; files that do not
	// exist yet are created once the map has houses.
	void save(Map& map, const ServerDataFolder& folder, ServerFileWriter& writer, std::vector<std::string>& warnings);

} // namespace rme::sec::house_files

#endif
