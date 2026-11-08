//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_DATA_MAP_DAT_FILE_H_
#define RME_IO_SEC_DATA_MAP_DAT_FILE_H_

// dat/map.dat: the towns (one per Depot line), the named marks and,
// read-only, the sectors the server loads and the start positions. It is
// the only source of the map's towns.
//
// The server has no temples: a town's temple is the mark named like the
// town ("PortHope" for Port Hope), or the veteran start when it has none.
// That mark belongs to its town; the other marks are the map's waypoints.

#include "io/sec/data/server_data_folder.h"
#include "io/sec/srv_world_files.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Map;

namespace rme::sec {

	// Editor town IDs start at 1 (0 means "no town"), depot numbers at 0.
	[[nodiscard]] constexpr uint32_t townIdOfDepot(int depot) {
		return static_cast<uint32_t>(depot) + 1;
	}
	[[nodiscard]] constexpr int depotOfTownId(uint32_t town_id) {
		return static_cast<int>(town_id) - 1;
	}

	namespace map_dat {

		// Reads map.dat into the map's towns and waypoints. Returns what it
		// holds for the other handlers; nullopt when there is none or it
		// could not be read.
		[[nodiscard]] std::optional<MapInfo> load(Map& map, const ServerDataFolder& folder, std::vector<std::string>& warnings);

		// Writes the towns back as depots, and their temples and the waypoints
		// as marks, when they changed. Without a map.dat, a new one also gets
		// `sectors`, the range of the saved sector files.
		void save(const Map& map, const ServerDataFolder& folder, const std::optional<SectorRange>& sectors, ServerFileWriter& writer, std::vector<std::string>& warnings);

	} // namespace map_dat

} // namespace rme::sec

#endif
