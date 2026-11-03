//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "io/sec/data/monster_files.h"

#include "app/settings.h"
#include "game/creature.h"
#include "game/srv_homes.h"
#include "io/sec/data/creature_placement.h"
#include "io/sec/sec_file_io.h"
#include "io/sec/srv_creature_files.h"
#include "map/map.h"
#include "map/tile.h"
#include "util/file_system.h"

#include <algorithm>
#include <format>
#include <spdlog/spdlog.h>
#include <tuple>
#include <unordered_map>

namespace rme::sec::monster_files {

	namespace {


		// mon/*.mon by file name, with the names the editor gives the races.
		std::vector<NamedMonsterRace> readRaces(const ServerDataFolder& folder, std::vector<std::string>& warnings, bool report_errors) {
			std::vector<std::pair<std::string, MonsterRace>> races;
			forEachServerFile(folder.mon(), ".mon", warnings, [&](const std::filesystem::path& path, std::string_view text) {
				auto race = readMonsterRace(text);
				if (!race) {
					if (report_errors) {
						warnings.push_back(std::format("{}:{}: {}; monster race skipped.", FileSystem::PathToUtf8(path.filename()), race.error().line, race.error().message));
					}
					return;
				}
				races.emplace_back(FileSystem::PathToUtf8(path.stem()), std::move(*race));
			});
			return nameMonsterRaces(std::move(races));
		}



	} // namespace

	Races loadRaces(const ServerDataFolder& folder, std::vector<std::string>& warnings) {
		Races races;
		for (const NamedMonsterRace& named : readRaces(folder, warnings, true)) {
			CreatureType* type = serverCreatureType(named.editor_name, false, named.race.outfit);
			races.by_race[named.race.race] = type;
			races.types.push_back(type);
		}
		return races;
	}

	void loadHomes(const ServerDataFolder& folder, const Races& races, CreaturePlacer& placer, std::vector<std::string>& warnings) {
		const std::filesystem::path path = folder.dat() / "monster.db";
		std::string text;
		if (!readFile(path, text)) {
			spdlog::info("IOMapSec: no dat/monster.db, so the map has no monsters yet");
			return;
		}
		const auto homes = readMonsterHomes(text);
		if (!homes) {
			warnings.push_back(std::format("monster.db:{}: {}; no monsters were placed.", homes.error().line, homes.error().message));
			return;
		}

		size_t placed = 0;
		std::map<int, size_t> unknown_races;
		for (const MonsterHome& home : *homes) {
			const auto race = races.by_race.find(home.race);
			if (race == races.by_race.end() || !race->second) {
				unknown_races[home.race] += static_cast<size_t>(std::max(0, home.amount));
				continue;
			}
			placed += placer.place(race->second, false, home.position, home.radius, home.amount, home.regen);
		}
		for (const auto& [race, count] : unknown_races) {
			warnings.push_back(std::format("monster.db: race {} has no mon/*.mon file; {} monsters were not placed.", race, count));
		}
		spdlog::info("IOMapSec: placed {} monsters", placed);
	}

	void save(Map& map, const std::vector<Tile*>& tiles, const ServerDataFolder& folder, ServerFileWriter& writer, std::vector<std::string>& warnings) {
		// Named as loadRaces named them, so each race maps back to itself.
		std::vector<std::string> ignored;
		std::unordered_map<std::string, int> races;
		for (const NamedMonsterRace& named : readRaces(folder, ignored, false)) {
			races.emplace(as_lower_str(named.editor_name), named.race.race);
		}
		const std::filesystem::path path = folder.dat() / "monster.db";
		std::error_code ec;
		if (tiles.empty() && !std::filesystem::exists(path, ec)) {
			return;
		}
		if (races.empty()) {
			if (!tiles.empty()) {
				warnings.push_back("No monster races (mon/*.mon) were found next to the map; monster.db was not written.");
			}
			return;
		}

		const int max_radius = g_settings.getInteger(Config::MAX_SPAWN_RADIUS);
		std::vector<MonsterHome> homes;
		homes.reserve(tiles.size());
		std::map<std::string, size_t> unknown;
		for (Tile* tile : tiles) {
			const Creature& creature = *tile->creature;
			const auto race = races.find(as_lower_str(creature.getName()));
			if (race == races.end()) {
				++unknown[creature.getName()];
				continue;
			}
			homes.push_back(MonsterHome {
				.race = race->second,
				.position = AbsolutePosition { .x = tile->getPosition().x, .y = tile->getPosition().y, .z = tile->getPosition().z },
				.radius = rme::srv::homeRadius(creature, rme::srv::homeSpawnSize(map, tile->getPosition()), max_radius),
				.amount = creature.getAmount(),
				.regen = creature.getSpawnTime(),
			});
		}
		if (!unknown.empty()) {
			warnings.push_back(std::format("Monsters without a mon/*.mon race were not written to monster.db: {}", listNameCounts(unknown)));
		}
		// Within a sector, homes are listed by race and position.
		std::ranges::sort(homes, {}, [](const MonsterHome& home) {
			return std::tuple(home.race, home.position.x, home.position.y);
		});
		// Written only when the homes changed; their order in the file does not count.
		const auto key = [](const MonsterHome& h) { return std::tuple(h.position.x, h.position.y, h.position.z, h.race, h.radius, h.amount, h.regen); };
		if (std::string text; readFile(path, text)) {
			if (auto previous = readMonsterHomes(text)) {
				std::vector<MonsterHome> sorted = homes;
				std::ranges::sort(sorted, {}, key);
				std::ranges::sort(*previous, {}, key);
				if (std::ranges::equal(sorted, *previous, {}, key, key)) {
					return;
				}
			}
		}
		writer.write(path, writeMonsterHomes(std::move(homes)));
	}

} // namespace rme::sec::monster_files
