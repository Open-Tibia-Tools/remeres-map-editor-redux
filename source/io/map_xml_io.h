//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_XML_IO_H_
#define RME_MAP_XML_IO_H_

#include <string>
#include <functional>
#include <utility>
#include "ext/pugixml.hpp"

class Map;
class wxFileName;

/**
 * @brief Helper class to handle XML auxiliary map files (Houses, Spawns, Waypoints)
 */
class MapXMLIO {
public:
	// Helper class to handle XML auxiliary map files (Houses, Spawns, Waypoints)
	using CancelCheck = std::function<bool()>;

	// Spawns
	static bool loadSpawns(Map& map, const wxFileName& dir);
	static bool loadMonsterSpawns(Map& map, const wxFileName& dir);
	static bool loadNpcSpawns(Map& map, const wxFileName& dir);
	static bool loadSpawns(Map& map, pugi::xml_document& doc);
	// OTBM 5 maps get separate monster and NPC files.
	// shouldCancel is polled while the document is built; returning true aborts.
	static bool saveSpawns(const Map& map, const wxFileName& dir, const CancelCheck& shouldCancel = {});

	// Houses
	static bool loadHouses(Map& map, const wxFileName& dir);
	static bool loadHouses(Map& map, pugi::xml_document& doc);
	static bool saveHouses(const Map& map, const wxFileName& dir, const CancelCheck& shouldCancel = {});
	static bool saveHouses(const Map& map, pugi::xml_document& doc, const CancelCheck& shouldCancel = {});

	// Zones (Crystal Server)
	static bool loadZones(Map& map, const wxFileName& dir);
	static bool saveZones(const Map& map, const wxFileName& dir);

	// Waypoints
	static bool loadWaypoints(Map& map, const wxFileName& dir, bool replace = true);
	static bool loadWaypoints(Map& map, pugi::xml_document& doc, bool replace = true);
	static bool saveWaypoints(const Map& map, const wxFileName& dir, const CancelCheck& shouldCancel = {});
	static bool saveWaypoints(const Map& map, pugi::xml_document& doc, const CancelCheck& shouldCancel = {});
	static bool loadWaypoints(Map& map, pugi::xml_node node, bool replace = true);

	static std::pair<std::string, std::string> normalizeMapFilePaths(const wxFileName& dir, const std::string& filename);
};

#endif // RME_MAP_XML_IO_H_
