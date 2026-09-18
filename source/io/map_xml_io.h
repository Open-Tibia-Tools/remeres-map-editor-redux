//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_XML_IO_H_
#define RME_MAP_XML_IO_H_

#include <filesystem>
#include <string>
#include <utility>
#include "app/main.h"
#include "ext/pugixml.hpp"

class Map;

/**
 * @brief Helper class to handle XML auxiliary map files (Houses, Spawns, Waypoints)
 */
class MapXMLIO {
public:
	// Spawns
	static bool loadSpawns(Map& map, const std::filesystem::path& dir);
	static bool loadSpawns(Map& map, const FileName& dir) {
		return loadSpawns(map, std::filesystem::path(dir.GetFullPath().ToStdWstring()));
	}
	static bool loadSpawns(Map& map, pugi::xml_document& doc);
	static bool saveSpawns(const Map& map, const std::filesystem::path& dir);
	static bool saveSpawns(const Map& map, const FileName& dir) {
		return saveSpawns(map, std::filesystem::path(dir.GetFullPath().ToStdWstring()));
	}
	static bool saveSpawns(const Map& map, pugi::xml_document& doc);

	// Houses
	static bool loadHouses(Map& map, const std::filesystem::path& dir);
	static bool loadHouses(Map& map, const FileName& dir) {
		return loadHouses(map, std::filesystem::path(dir.GetFullPath().ToStdWstring()));
	}
	static bool loadHouses(Map& map, pugi::xml_document& doc);
	static bool saveHouses(const Map& map, const std::filesystem::path& dir);
	static bool saveHouses(const Map& map, const FileName& dir) {
		return saveHouses(map, std::filesystem::path(dir.GetFullPath().ToStdWstring()));
	}
	static bool saveHouses(const Map& map, pugi::xml_document& doc);

	// Waypoints
	static bool loadWaypoints(Map& map, const std::filesystem::path& dir, bool replace = true);
	static bool loadWaypoints(Map& map, const FileName& dir, bool replace = true) {
		return loadWaypoints(map, std::filesystem::path(dir.GetFullPath().ToStdWstring()), replace);
	}
	static bool loadWaypoints(Map& map, pugi::xml_document& doc, bool replace = true);
	static bool saveWaypoints(const Map& map, const std::filesystem::path& dir);
	static bool saveWaypoints(const Map& map, const FileName& dir) {
		return saveWaypoints(map, std::filesystem::path(dir.GetFullPath().ToStdWstring()));
	}
	static bool saveWaypoints(const Map& map, pugi::xml_document& doc);
	static bool loadWaypoints(Map& map, pugi::xml_node node, bool replace = true);

	static std::pair<std::string, std::string> normalizeMapFilePaths(const std::filesystem::path& dir, const std::string& filename);
	static std::pair<std::string, std::string> normalizeMapFilePaths(const FileName& dir, const std::string& filename) {
		return normalizeMapFilePaths(std::filesystem::path(dir.GetFullPath().ToStdWstring()), filename);
	}
};

#endif // RME_MAP_XML_IO_H_
