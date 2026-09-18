#ifndef RME_EDITOR_PERSISTENCE_H
#define RME_EDITOR_PERSISTENCE_H

#include "app/definitions.h"
#include "editor/persistence/map_load_options.h"
#include "io/iomap.h"
#include "map/position.h"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Editor;
class Map;
class Spawn;

struct SaveResult {
	bool success = false;
	std::string error_message;
};

struct MapImportOptions {
	int import_x_offset = 0;
	int import_y_offset = 0;
	ImportType house_import_type = IMPORT_MERGE;
	ImportType spawn_import_type = IMPORT_MERGE;
	bool auto_resize = true;
};

struct MapImportResult {
	bool success = false;
	uint64_t tiles_merged = 0;
	uint64_t discarded_tiles = 0;
	std::string error;
	std::vector<std::string> warnings;
};

class EditorPersistence {
public:
	using ProgressCallback = std::function<void(int percent, std::string_view msg)>;

	static SaveResult saveMap(Editor& editor, const std::filesystem::path& filename = {}, ProgressCallback on_progress = nullptr);
	static void loadMap(Editor& editor, const std::filesystem::path& filename, const MapLoadOptions& load_options = {});
	static MapImportResult importMap(Editor& editor, const std::filesystem::path& filename, const MapImportOptions& options = {}, ProgressCallback on_progress = nullptr);
	static bool importMiniMap(Editor& editor, const std::filesystem::path& filename, int import, int import_x_offset, int import_y_offset, int import_z_offset);

private:
	static void importTowns(Editor& editor, Map& imported_map, const Position& offset, ImportType house_import_type, std::unordered_map<uint32_t, uint32_t>& town_id_map);
	static void importHouses(Editor& editor, Map& imported_map, const Position& offset, ImportType house_import_type, const std::unordered_map<uint32_t, uint32_t>& town_id_map, std::unordered_map<uint32_t, uint32_t>& house_id_map);
	static void importSpawns(Editor& editor, Map& imported_map, const Position& offset, ImportType spawn_import_type, std::map<Position, std::unique_ptr<Spawn>>& spawn_map);
};

#endif
