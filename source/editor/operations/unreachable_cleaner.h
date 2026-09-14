//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_OPERATIONS_UNREACHABLE_CLEANER_H_
#define RME_EDITOR_OPERATIONS_UNREACHABLE_CLEANER_H_

#include "app/main.h"
#include <cstdint>
#include <string>
#include <vector>
#include "map/position.h"

class Editor;
class Map;
class Tile;

namespace EditorOperations {

enum class CleanerFloorScope : uint8_t {
	All = 0,        // Floors 0 - 15
	SurfaceOnly,   // Floors 0 - 7 (Ground and above)
	UndergroundOnly // Floors 8 - 15 (Subterranean caves)
};

struct UnreachableCleanerSettings {
	int viewport_width = 26;
	int viewport_height = 11;
	int safety_margin = 2;
	CleanerFloorScope floor_scope = CleanerFloorScope::All;
	bool multi_floor = true;
	bool create_backup = true;
};

struct UnreachableCleanerResult {
	uint64_t total_tiles_checked = 0;
	uint64_t unreachable_tiles_found = 0;
	uint64_t tiles_removed = 0;
	bool backup_created = false;
	std::string backup_path;
	std::string error_message;
};

class UnreachableCleaner {
public:
	static bool IsWalkable(const Tile* tile);
	static std::vector<Position> FindUnreachableTiles(Map& map, const UnreachableCleanerSettings& settings, uint64_t* out_total_tiles = nullptr);
	static bool CreateBackup(Editor& editor, std::string& out_backup_path, std::string& out_error);
	static UnreachableCleanerResult Clean(Editor& editor, const UnreachableCleanerSettings& settings);
};

} // namespace EditorOperations

#endif // RME_EDITOR_OPERATIONS_UNREACHABLE_CLEANER_H_
