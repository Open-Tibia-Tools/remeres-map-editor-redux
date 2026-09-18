#ifndef RME_EDITOR_OPERATIONS_MAP_VERSION_CHANGER_H
#define RME_EDITOR_OPERATIONS_MAP_VERSION_CHANGER_H

#include "map/map.h"
#include <string>
#include <vector>

class Editor;

struct MapConversionOptions {
	bool keep_unrecognized_creatures = true;
};

struct MapVersionChangeResult {
	bool success = false;
	std::string error;
	std::vector<std::string> warnings;
};

class MapVersionChanger {
public:
	// Returns result of map version conversion
	static MapVersionChangeResult changeMapVersion(Editor& editor, MapVersion new_ver, const MapConversionOptions& options = {});
};

#endif
