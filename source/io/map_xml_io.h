//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_XML_IO_H_
#define RME_MAP_XML_IO_H_

#include "app/definitions.h"
#include "ext/pugixml.hpp"

#include <filesystem>
#include <string>
#include <utility>

class Map;

/**
 * @brief Helper class to handle XML auxiliary map files (Houses, Spawns, Waypoints)
 */
class MapXMLIO {
public:
	// Spawns
	static bool loadSpawns(Map& map, const std::filesystem::path& dir);
	static bool loadSpawns(Map& map, pugi::xml_document& doc);
	static bool saveSpawns(const Map& map, const std::filesystem::path& dir);
	static bool saveSpawns(const Map& map, pugi::xml_document& doc);

	// Houses
	static bool loadHouses(Map& map, const std::filesystem::path& dir);
	static bool loadHouses(Map& map, pugi::xml_document& doc);
	static bool saveHouses(const Map& map, const std::filesystem::path& dir);
	static bool saveHouses(const Map& map, pugi::xml_document& doc);

	// Waypoints
	static bool loadWaypoints(Map& map, const std::filesystem::path& dir, bool replace = true);
	static bool loadWaypoints(Map& map, pugi::xml_document& doc, bool replace = true);
	static bool saveWaypoints(const Map& map, const std::filesystem::path& dir);
	static bool saveWaypoints(const Map& map, pugi::xml_document& doc);
	static bool loadWaypoints(Map& map, pugi::xml_node node, bool replace = true);

	static std::pair<std::string, std::string> normalizeMapFilePaths(const std::filesystem::path& dir, const std::string& filename);
};

#endif // RME_MAP_XML_IO_H_
