//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_XML_IO_H_
#define RME_MAP_XML_IO_H_

#include <string>
#include <utility>
#include "ext/pugixml.hpp"

class Map;
class wxFileName;

/**
 * @brief Helper class to handle XML auxiliary map files (Houses, Spawns, Waypoints)
 */
class MapXMLIO {
public:
	// Spawns
	static bool loadSpawns(Map& map, const wxFileName& dir);
	static bool loadNpcSpawns(Map& map, const wxFileName& dir);
	static bool loadSpawns(Map& map, pugi::xml_document& doc);
	// Crystal Server maps (MAP_OTBM_5) get separate monster and NPC files
	static bool saveSpawns(const Map& map, const wxFileName& dir);

	// Houses
	static bool loadHouses(Map& map, const wxFileName& dir);
	static bool loadHouses(Map& map, pugi::xml_document& doc);
	static bool saveHouses(const Map& map, const wxFileName& dir);
	static bool saveHouses(const Map& map, pugi::xml_document& doc);

	// Zones (Crystal Server)
	static bool loadZones(Map& map, const wxFileName& dir);
	static bool saveZones(const Map& map, const wxFileName& dir);

	// Waypoints
	static bool loadWaypoints(Map& map, const wxFileName& dir, bool replace = true);
	static bool loadWaypoints(Map& map, pugi::xml_document& doc, bool replace = true);
	static bool saveWaypoints(const Map& map, const wxFileName& dir);
	static bool saveWaypoints(const Map& map, pugi::xml_document& doc);
	static bool loadWaypoints(Map& map, pugi::xml_node node, bool replace = true);

	static std::pair<std::string, std::string> normalizeMapFilePaths(const wxFileName& dir, const std::string& filename);
};

#endif // RME_MAP_XML_IO_H_
