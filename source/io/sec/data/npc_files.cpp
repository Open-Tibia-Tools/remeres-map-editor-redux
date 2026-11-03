//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "io/sec/data/npc_files.h"

#include "app/settings.h"
#include "game/creature.h"
#include "game/spawn.h"
#include "game/srv_homes.h"
#include "io/sec/data/creature_placement.h"
#include "io/sec/srv_creature_files.h"
#include "map/tile.h"
#include "util/file_system.h"

#include <format>
#include <map>
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace rme::sec::npc_files {

	std::vector<CreatureType*> load(const ServerDataFolder& folder, CreaturePlacer& placer, std::vector<std::string>& warnings) {
		const int spawn_time = g_settings.getInteger(Config::DEFAULT_SPAWNTIME);
		std::vector<CreatureType*> types;
		size_t placed = 0;
		forEachServerFile(folder.npc(), ".npc", warnings, [&](const std::filesystem::path& path, std::string_view text) {
			const auto npc = readNpc(text);
			if (!npc) {
				warnings.push_back(std::format("{}:{}: {}; NPC skipped.", FileSystem::PathToUtf8(path.filename()), npc.error().line, npc.error().message));
				return;
			}
			CreatureType* type = serverCreatureType(npc->name, true, npc->outfit);
			types.push_back(type);
			placed += placer.place(type, true, npc->home, npc->radius, 1, spawn_time);
		});
		spdlog::info("IOMapSec: placed {} NPCs", placed);
		return types;
	}

	void save(const std::vector<Tile*>& tiles, const ServerDataFolder& folder, ServerFileWriter& writer, std::vector<std::string>& warnings) {
		struct NpcFile {
			std::filesystem::path path;
			std::string text;
			NpcDefinition definition;
			bool on_map = false;
		};
		std::vector<std::string> ignored;
		std::unordered_map<std::string, NpcFile> files;
		forEachServerFile(folder.npc(), ".npc", ignored, [&](const std::filesystem::path& path, std::string_view text) {
			if (auto npc = readNpc(text)) {
				std::string key = as_lower_str(npc->name);
				files.emplace(std::move(key), NpcFile { .path = path, .text = std::string(text), .definition = std::move(*npc) });
			}
		});

		const int max_radius = g_settings.getInteger(Config::MAX_SPAWN_RADIUS);
		std::map<std::string, size_t> without_file;
		for (Tile* tile : tiles) {
			const Creature& creature = *tile->creature;
			const auto file = files.find(as_lower_str(creature.getName()));
			if (file == files.end()) {
				++without_file[creature.getName()];
				continue;
			}
			file->second.on_map = true;
			// An NPC stands on its home tile, with its spawn. One without a
			// spawn was put next to its home at load (the tile was taken),
			// so its file keeps the home it has.
			if (!tile->spawn) {
				continue;
			}
			const Position& position = tile->getPosition();
			const AbsolutePosition home { .x = position.x, .y = position.y, .z = position.z };
			const int radius = rme::srv::homeRadius(creature, tile->spawn->getSize(), max_radius);
			const NpcDefinition& definition = file->second.definition;
			if (home != definition.home || radius != definition.radius) {
				writer.write(file->second.path, updateNpcHome(file->second.text, home, radius));
			}
		}

		if (!without_file.empty()) {
			warnings.push_back(std::format("NPCs without an npc/*.npc file were not saved (an NPC needs its file with its dialogue): {}", listNameCounts(without_file)));
		}
		std::map<std::string, size_t> missing;
		for (const auto& [key, file] : files) {
			if (!file.on_map) {
				++missing[file.definition.name];
			}
		}
		if (!missing.empty()) {
			warnings.push_back(std::format("NPCs that are no longer on the map keep their npc/*.npc file (delete it to remove them from the server): {}", listNameCounts(missing)));
		}
	}

} // namespace rme::sec::npc_files
