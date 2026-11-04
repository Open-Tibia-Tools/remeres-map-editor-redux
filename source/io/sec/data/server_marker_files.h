//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_DATA_SERVER_MARKER_FILES_H_
#define RME_IO_SEC_DATA_SERVER_MARKER_FILES_H_

// The read-only server markers (see game/srv_markers.h): the start positions
// of map.dat, the tiles that dat/moveuse.dat rules name and the raid waves of
// mon/*.evt. They are never saved.

#include "io/sec/data/server_data_folder.h"
#include "io/sec/srv_world_files.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class CreatureType;

namespace rme::srv {
	class ServerMarkers;
}

namespace rme::sec::server_marker_files {

	// `info` is map.dat (nullptr when it was not read); `races` names the raids' monsters.
	[[nodiscard]] std::unique_ptr<rme::srv::ServerMarkers> load(const ServerDataFolder& folder, const MapInfo* info, const std::map<int, CreatureType*>& races, std::vector<std::string>& warnings);

} // namespace rme::sec::server_marker_files

#endif
