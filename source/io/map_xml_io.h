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
	// Reports progress while a document is built: which companion file, how many
	// of its items are done, and the total for the whole stage (OTBM 5 splits its
	// stage between the monster and the NPC file). Used by long writers (spawns)
	// to keep the progress dialog moving instead of sitting on one percentage.
	using ProgressReport = std::function<void(const std::string& file, size_t done, size_t total)>;

	// Spawns
	static bool loadSpawns(Map& map, const wxFileName& dir);
	static bool loadMonsterSpawns(Map& map, const wxFileName& dir);
	static bool loadNpcSpawns(Map& map, const wxFileName& dir);
	static bool loadSpawns(Map& map, pugi::xml_document& doc);
	static bool saveSpawns(const Map& map, pugi::xml_document& doc);
	// OTBM 5 maps get separate monster and NPC files.
	// shouldCancel is polled while the document is built; returning true aborts.
	// report follows the whole stage, so the monster pass covers its first half
	// and the NPC pass the second one.
	static bool saveSpawns(const Map& map, const wxFileName& dir, const CancelCheck& shouldCancel = {}, const ProgressReport& report = {});

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
