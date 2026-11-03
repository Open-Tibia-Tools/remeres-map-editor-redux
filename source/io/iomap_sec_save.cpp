//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

// Saving maps to the original server's files: the sector files, and
// with them the monster homes, houses, marks and NPC homes kept in the
// server's data folder. See iomap_sec.h.

#include "app/main.h"

#include "io/iomap_sec.h"

#include "app/client_version.h"
#include "app/managers/version_manager.h"
#include "app/settings.h"
#include "game/srv_homes.h"
#include "game/srv_liquids.h"
#include "game/complexitem.h"
#include "game/creature.h"
#include "game/house.h"
#include "game/item.h"
#include "game/spawn.h"
#include "game/waypoints.h"
#include "io/sec/srv_creature_files.h"
#include "io/sec/srv_script_lexer.h"
#include "io/sec/srv_world_files.h"
#include "io/sec/sec_file_io.h"
#include "io/sec/sec_sector_parser.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/map.h"
#include "map/tile.h"
#include "ui/gui.h"
#include "util/file_system.h"

#include <algorithm>
#include <array>
#include <format>
#include <map>
#include <ranges>
#include <spdlog/spdlog.h>
#include <tuple>
#include <unordered_map>

namespace {

	using rme::sec::AbsolutePosition;
	using rme::sec::InstanceAttribute;
	using rme::sec::ObjectAttribute;
	using rme::sec::SectorObject;
	using rme::sec::SectorTile;
	using rme::sec::TileFlag;
	using rme::sec::WriteResult;

	constexpr size_t kMaxListedNames = 20;

	AbsolutePosition toAbsolute(const Position& position) {
		return AbsolutePosition { .x = position.x, .y = position.y, .z = position.z };
	}

	std::string listNames(const std::map<std::string, size_t>& names) {
		std::string out;
		size_t listed = 0;
		for (const auto& [name, count] : names) {
			if (listed++ == kMaxListedNames) {
				out += ", ...";
				break;
			}
			std::format_to(std::back_inserter(out), "{}{} (x{})", out.empty() ? "" : ", ", name, count);
		}
		return out;
	}

	struct SaveStats {
		size_t sectors_written = 0;
		size_t sectors_unchanged = 0;
		std::vector<std::string> failed_files;
		std::vector<std::string> unreadable_sectors;
		size_t invalid_items = 0;
		size_t unknown_liquids = 0;
		size_t ot_only_ids = 0; // Action and unique IDs have no place in sector files
		size_t ot_only_flags = 0; // No-PvP and PvP zones neither
	};

	// Editor items back to sector objects: the inverse of the loader's ItemBuilder.
	class ObjectWriter {
	public:
		explicit ObjectWriter(SaveStats& stats) :
			stats_(stats) {
			for (size_t i = 0; i < names_.size(); ++i) {
				names_[i] = std::string(rme::sec::attributeName(static_cast<InstanceAttribute>(i)));
			}
		}

		std::optional<SectorObject> write(const Item& item) {
			const ServerItemId id = item.getID();
			if (id > 0xFFFF || !g_item_definitions.exists(id)) {
				++stats_.invalid_items;
				return std::nullopt;
			}

			SectorObject object { .type_id = static_cast<uint16_t>(id) };
			const auto set = [&object](InstanceAttribute key, int64_t number) {
				object.attributes.push_back(ObjectAttribute { .key = key, .number = number });
			};

			if (item.isStackable()) {
				set(InstanceAttribute::Amount, item.getSubtype());
			} else if (item.isCharged()) {
				set(InstanceAttribute::Charges, item.getSubtype());
			}
			if ((item.isFluidContainer() || item.isSplash()) && item.getSubtype() != LIQUID_NONE) {
				const auto liquid = std::ranges::find(rme::srv::kLiquids, static_cast<SplashType>(item.getSubtype()));
				if (liquid == rme::srv::kLiquids.end()) {
					++stats_.unknown_liquids;
				} else {
					set(item.isSplash() ? InstanceAttribute::PoolLiquidType : InstanceAttribute::ContainerLiquidType, std::distance(rme::srv::kLiquids.begin(), liquid));
				}
			}
			if (const auto text = item.getText(); !text.empty()) {
				object.attributes.push_back(ObjectAttribute { .key = InstanceAttribute::String, .text = std::string(text) });
			}
			if (const Teleport* teleport = item.asTeleport(); teleport && teleport->hasDestination()) {
				set(InstanceAttribute::AbsTeleportDestination, rme::sec::packAbsoluteCoordinate(toAbsolute(teleport->getDestination())));
			}

			// Everything else the loader kept as item attributes under its
			// server name (keyholes, quests, levels, expiry, ...).
			if (item.hasAttributes()) {
				for (size_t i = 0; i < names_.size(); ++i) {
					const auto key = static_cast<InstanceAttribute>(i);
					if (key == InstanceAttribute::Content || key == InstanceAttribute::String || std::ranges::any_of(object.attributes, [key](const ObjectAttribute& a) { return a.key == key; })) {
						continue;
					}
					if (key == InstanceAttribute::Editor) {
						if (const std::string* editor = item.getStringAttribute(names_[i])) {
							object.attributes.push_back(ObjectAttribute { .key = key, .text = *editor });
						}
					} else if (const int32_t* value = item.getIntegerAttribute(names_[i])) {
						set(key, *value);
					}
				}
				if (item.getActionID() != 0 || item.getUniqueID() != 0) {
					++stats_.ot_only_ids;
				}
			}

			if (const Container* container = item.asContainer()) {
				for (const auto& child : container->getVector()) {
					if (child) {
						if (auto child_object = write(*child)) {
							object.content.push_back(std::move(*child_object));
						}
					}
				}
			}
			return object;
		}

	private:
		SaveStats& stats_;
		std::array<std::string, 18> names_;
	};

	// The server lists ground and always-on-top items first, then the others
	// with the topmost first; the editor keeps those bottom to top.
	SectorTile writeTile(const Tile& tile, int sector_x, int sector_y, ObjectWriter& writer, SaveStats& stats) {
		const Position& position = tile.getPosition();
		SectorTile out {
			.offset_x = static_cast<uint8_t>(position.x - sector_x * rme::sec::kSectorSize),
			.offset_y = static_cast<uint8_t>(position.y - sector_y * rme::sec::kSectorSize),
		};

		const uint32_t flags = tile.getMapFlags();
		if (flags & TILESTATE_REFRESH) {
			out.flags |= static_cast<uint8_t>(TileFlag::Refresh);
		}
		if (flags & TILESTATE_NOLOGOUT) {
			out.flags |= static_cast<uint8_t>(TileFlag::NoLogout);
		}
		if (flags & TILESTATE_PROTECTIONZONE) {
			out.flags |= static_cast<uint8_t>(TileFlag::ProtectionZone);
		}
		if (flags & (TILESTATE_NOPVP | TILESTATE_PVPZONE)) {
			++stats.ot_only_flags;
		}

		const auto add = [&](const Item& item) {
			if (auto object = writer.write(item)) {
				out.objects.push_back(std::move(*object));
			}
		};
		if (tile.ground) {
			add(*tile.ground);
		}
		for (const auto& item : tile.items) {
			if (item && item->isAlwaysOnBottom()) {
				add(*item);
			}
		}
		for (const auto& item : tile.items | std::views::reverse) {
			if (item && !item->isAlwaysOnBottom()) {
				add(*item);
			}
		}
		return out;
	}

	// The spawn size a creature's home radius is read from: the spawn on its
	// tile, else the first spawn covering it.
	int spawnSizeAt(Map& map, Tile& tile) {
		if (tile.spawn) {
			return tile.spawn->getSize();
		}
		const SpawnList spawns = map.getSpawnList(&tile);
		return spawns.empty() ? 1 : spawns.front()->getSize();
	}

	class ServerDataWriter {
	public:
		ServerDataWriter(Map& map, const std::filesystem::path& data_folder, std::vector<std::string>& warnings, SaveStats& stats) :
			map_(map), data_folder_(data_folder), warnings_(warnings), stats_(stats), max_radius_(g_settings.getInteger(Config::MAX_SPAWN_RADIUS)) { }

		void addCreature(Tile& tile) {
			if (tile.creature->isNpc()) {
				npcs_.push_back(&tile);
			} else {
				monsters_.push_back(&tile);
			}
		}

		void write() {
			writeMonsterHomes();
			writeNpcHomes();
			writeHouses();
			writeMarks();
		}

	private:
		void save(const std::filesystem::path& path, std::string_view content) {
			if (rme::sec::writeFileIfChanged(path, content, scratch_) == WriteResult::Failed) {
				stats_.failed_files.push_back(FileSystem::PathToUtf8(path));
			}
		}

		template <typename OnFile>
		void forEachFile(const std::filesystem::path& folder, std::string_view extension, OnFile on_file) {
			std::error_code ec;
			std::string text;
			for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
				if (entry.is_regular_file(ec) && rme::srv::equalsIgnoreCase(entry.path().extension().string(), extension) && rme::sec::readFile(entry.path(), text)) {
					on_file(entry.path(), text);
				}
			}
		}

		void writeMonsterHomes() {
			// Named as the loader named them, so each race maps back to itself.
			std::vector<std::pair<std::string, rme::sec::MonsterRace>> by_file;
			forEachFile(data_folder_ / "mon", ".mon", [&](const std::filesystem::path& path, std::string_view text) {
				if (auto race = rme::sec::readMonsterRace(text)) {
					by_file.emplace_back(FileSystem::PathToUtf8(path.stem()), std::move(*race));
				}
			});
			std::unordered_map<std::string, int> races;
			for (const rme::sec::NamedMonsterRace& named : rme::sec::nameMonsterRaces(std::move(by_file))) {
				races.emplace(as_lower_str(named.editor_name), named.race.race);
			}
			if (races.empty()) {
				warnings_.push_back("No monster races (mon/*.mon) were found next to the map; monster.db was not written.");
				return;
			}

			std::vector<rme::sec::MonsterHome> homes;
			homes.reserve(monsters_.size());
			std::map<std::string, size_t> unknown;
			for (Tile* tile : monsters_) {
				const Creature& creature = *tile->creature;
				const auto race = races.find(as_lower_str(creature.getName()));
				if (race == races.end()) {
					++unknown[creature.getName()];
					continue;
				}
				homes.push_back(rme::sec::MonsterHome {
					.race = race->second,
					.position = toAbsolute(tile->getPosition()),
					.radius = rme::srv::homeRadius(creature, spawnSizeAt(map_, *tile), max_radius_),
					.amount = creature.getAmount(),
					.regen = creature.getSpawnTime(),
				});
			}
			if (!unknown.empty()) {
				warnings_.push_back(std::format("Monsters without a mon/*.mon race were not written to monster.db: {}", listNames(unknown)));
			}
			// Within a sector, homes are listed by race and position.
			std::ranges::sort(homes, {}, [](const rme::sec::MonsterHome& home) {
				return std::tuple(home.race, home.position.x, home.position.y);
			});
			// Written only when the homes changed; their order in the file does not count.
			const std::filesystem::path path = data_folder_ / "dat" / "monster.db";
			const auto key = [](const rme::sec::MonsterHome& h) { return std::tuple(h.position.x, h.position.y, h.position.z, h.race, h.radius, h.amount, h.regen); };
			if (std::string text; rme::sec::readFile(path, text)) {
				if (auto previous = rme::sec::readMonsterHomes(text)) {
					std::vector<rme::sec::MonsterHome> sorted = homes;
					std::ranges::sort(sorted, {}, key);
					std::ranges::sort(*previous, {}, key);
					if (std::ranges::equal(sorted, *previous, {}, key, key)) {
						return;
					}
				}
			}
			save(path, rme::sec::writeMonsterHomes(std::move(homes)));
		}

		void writeNpcHomes() {
			struct NpcFile {
				std::filesystem::path path;
				std::string text;
				rme::sec::NpcDefinition definition;
				bool on_map = false;
			};
			std::unordered_map<std::string, NpcFile> files;
			forEachFile(data_folder_ / "npc", ".npc", [&](const std::filesystem::path& path, std::string_view text) {
				if (auto npc = rme::sec::readNpc(text)) {
					std::string key = as_lower_str(npc->name);
					files.emplace(std::move(key), NpcFile { .path = path, .text = std::string(text), .definition = std::move(*npc) });
				}
			});

			std::map<std::string, size_t> without_file;
			for (Tile* tile : npcs_) {
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
				const AbsolutePosition home = toAbsolute(tile->getPosition());
				const int radius = rme::srv::homeRadius(creature, tile->spawn->getSize(), max_radius_);
				const rme::sec::NpcDefinition& definition = file->second.definition;
				if (home != definition.home || radius != definition.radius) {
					save(file->second.path, rme::sec::updateNpcHome(file->second.text, home, radius));
				}
			}

			if (!without_file.empty()) {
				warnings_.push_back(std::format("NPCs without an npc/*.npc file were not saved (an NPC needs its file with its dialogue): {}", listNames(without_file)));
			}
			std::map<std::string, size_t> missing;
			for (const auto& [key, file] : files) {
				if (!file.on_map) {
					++missing[file.definition.name];
				}
			}
			if (!missing.empty()) {
				warnings_.push_back(std::format("NPCs that are no longer on the map keep their npc/*.npc file (delete it to remove them from the server): {}", listNames(missing)));
			}
		}

		void writeHouses() {
			const std::filesystem::path houses_path = data_folder_ / "dat" / "houses.dat";
			std::string houses_text;
			std::string areas_text;
			if (!rme::sec::readFile(houses_path, houses_text) || !rme::sec::readFile(data_folder_ / "dat" / "houseareas.dat", areas_text)) {
				warnings_.push_back("dat/houses.dat or dat/houseareas.dat was not found; houses were not saved.");
				return;
			}
			const auto original = rme::sec::readHouses(houses_text);
			const auto areas = rme::sec::readHouseAreas(areas_text);
			if (!original || !areas) {
				warnings_.push_back("dat/houses.dat or dat/houseareas.dat could not be read; houses were not saved.");
				return;
			}

			std::vector<const House*> houses;
			for (const auto& [id, house] : map_.houses) {
				houses.push_back(house.get());
			}
			std::ranges::sort(houses, {}, &House::getID);

			std::vector<rme::sec::HouseDefinition> definitions;
			std::vector<uint32_t> without_area;
			for (const House* house : houses) {
				const auto previous = std::ranges::find(*original, static_cast<int>(house->getID()), &rme::sec::HouseDefinition::id);
				rme::sec::HouseDefinition definition = previous != original->end() ? *previous : rme::sec::HouseDefinition { .id = static_cast<int>(house->getID()) };

				// Towns are depots (town ID = depot + 1); keep the house's area
				// while its depot still matches, else take the town's first area.
				const int depot = static_cast<int>(house->townid) - 1;
				auto area = std::ranges::find(*areas, definition.area, &rme::sec::HouseArea::id);
				if (area == areas->end() || area->depot != depot) {
					area = std::ranges::find(*areas, depot, &rme::sec::HouseArea::depot);
				}
				if (area == areas->end()) {
					without_area.push_back(house->getID());
					continue;
				}

				std::vector<AbsolutePosition> fields;
				for (const Position& position : house->getTiles()) {
					fields.push_back(toAbsolute(position));
				}
				const auto field_order = [](const AbsolutePosition& p) { return std::tuple(p.z, p.y, p.x); };
				std::ranges::sort(fields, {}, field_order);
				// Unchanged fields keep the order the file has them in.
				std::vector<AbsolutePosition> previous_fields = definition.fields;
				std::ranges::sort(previous_fields, {}, field_order);
				if (previous_fields == fields) {
					fields = definition.fields;
				}

				// The server's rent is the area's price per square times the
				// size plus the offset: keep the offset unless the rent was edited.
				const int previous_size = static_cast<int>(definition.fields.size());
				const auto previous_area = std::ranges::find(*areas, definition.area, &rme::sec::HouseArea::id);
				const int previous_rent = previous_area != areas->end() ? previous_area->square_price * previous_size + definition.rent_offset : -1;
				if (house->rent != previous_rent) {
					definition.rent_offset = house->rent - area->square_price * static_cast<int>(fields.size());
				}

				definition.name = house->name;
				definition.area = area->id;
				definition.guild_house = house->guildhall;
				definition.exit = toAbsolute(house->getExit());
				if (previous == original->end() && !fields.empty()) {
					const auto [min_x, max_x] = std::ranges::minmax(fields | std::views::transform(&AbsolutePosition::x));
					const auto [min_y, max_y] = std::ranges::minmax(fields | std::views::transform(&AbsolutePosition::y));
					definition.center = AbsolutePosition { .x = (min_x + max_x) / 2, .y = (min_y + max_y) / 2, .z = definition.exit.z };
				}
				definition.fields = std::move(fields);
				definitions.push_back(std::move(definition));
			}
			if (!without_area.empty()) {
				warnings_.push_back(std::format("{} houses belong to a town without a house area in houseareas.dat and were not saved.", without_area.size()));
			}
			// Written only when a house changed; how the file formats them does not count.
			std::vector<rme::sec::HouseDefinition> previous = *original;
			std::ranges::sort(previous, {}, &rme::sec::HouseDefinition::id);
			if (previous == definitions) {
				return;
			}
			save(houses_path, rme::sec::writeHouses(definitions, rme::sec::newlineOf(houses_text)));
		}

		// Waypoints are the server's marks; known ones keep their place in map.dat.
		void writeMarks() {
			const std::filesystem::path map_dat = data_folder_ / "dat" / "map.dat";
			std::string text;
			if (!rme::sec::readFile(map_dat, text)) {
				warnings_.push_back("dat/map.dat was not found; waypoints were not saved.");
				return;
			}
			const auto info = rme::sec::readMapInfo(text);
			if (!info) {
				warnings_.push_back("dat/map.dat could not be read; waypoints were not saved.");
				return;
			}

			std::vector<rme::sec::MapMark> marks;
			for (const auto& [key, waypoint] : map_.waypoints) {
				marks.push_back(rme::sec::MapMark { .name = waypoint->name, .position = toAbsolute(waypoint->pos) });
			}
			const auto order = [&info](const rme::sec::MapMark& mark) {
				const auto it = std::ranges::find(info->marks, mark.name, &rme::sec::MapMark::name);
				return std::pair(std::distance(info->marks.begin(), it), mark.name);
			};
			std::ranges::sort(marks, {}, order);
			save(map_dat, rme::sec::updateMapMarks(text, marks));
		}

		Map& map_;
		std::filesystem::path data_folder_;
		std::vector<std::string>& warnings_;
		SaveStats& stats_;
		const int max_radius_;
		std::vector<Tile*> monsters_;
		std::vector<Tile*> npcs_;
		std::string scratch_;
	};

} // namespace

bool IOMapSec::saveMap(Map& map, const FileName& identifier) {
	if (!g_version.IsVersionLoaded() || g_version.GetCurrentVersion().getItemDefinitionMode() != ItemDefinitionMode::DatSrv) {
		errorstr = "Sector maps are saved with a client version that reads its items from objects.srv (configType dat_srv).";
		return false;
	}

	const std::filesystem::path folder = sectorFolder(identifier);
	std::error_code ec;
	std::filesystem::create_directories(folder, ec);
	if (!std::filesystem::is_directory(folder, ec)) {
		errorstr = wxstr(std::format("Could not create the folder \"{}\".", FileSystem::PathToUtf8(folder)));
		return false;
	}

	// The server's data folder holds the map folder, dat/, mon/ and npc/.
	const std::filesystem::path data_folder = folder.parent_path();
	const bool server_layout = std::filesystem::is_directory(data_folder / "dat", ec);

	SaveStats stats;
	ServerDataWriter server_data(map, data_folder, warnings, stats);

	g_gui.SetLoadDone(0, "Collecting sectors...");
	std::map<std::tuple<int, int, int>, std::vector<const Tile*>> sectors;
	for (auto& location : map.tiles()) {
		Tile* tile = location.get();
		if (!tile) {
			continue;
		}
		const Position& position = tile->getPosition();
		sectors[std::tuple(position.x / rme::sec::kSectorSize, position.y / rme::sec::kSectorSize, position.z)].push_back(tile);
		if (tile->creature) {
			server_data.addCreature(*tile);
		}
	}

	ObjectWriter writer(stats);
	std::string scratch;
	size_t index = 0;
	for (const auto& [key, tiles] : sectors) {
		if (index++ % 64 == 0) {
			g_gui.SetLoadDone(static_cast<int>(index * 90 / sectors.size()), wxstr(std::format("Saving sectors ({}/{})...", index, sectors.size())));
		}
		const auto [sector_x, sector_y, sector_z] = key;
		rme::sec::Sector sector;
		sector.tiles.reserve(tiles.size());
		for (const Tile* tile : tiles) {
			sector.tiles.push_back(writeTile(*tile, sector_x, sector_y, writer, stats));
		}
		const rme::sec::SectorCoordinates coordinates { .x = sector_x, .y = sector_y, .z = sector_z };
		const std::filesystem::path path = folder / rme::sec::sectorFileName(coordinates);
		const std::string content = rme::sec::writeSector(coordinates, sector);

		// Only sectors whose tiles changed are written. A file in another
		// layout that holds the same tiles (origmap's older style) is left
		// as it is, and one the editor cannot read is never overwritten.
		if (rme::sec::readFile(path, scratch)) {
			const auto previous = rme::sec::parseSector(scratch);
			if (!previous) {
				stats.unreadable_sectors.push_back(FileSystem::PathToUtf8(path.filename()));
				continue;
			}
			if (rme::sec::writeSector(coordinates, *previous) == content) {
				++stats.sectors_unchanged;
				continue;
			}
		}
		if (rme::sec::writeFileAtomically(path, content)) {
			++stats.sectors_written;
		} else {
			stats.failed_files.push_back(FileSystem::PathToUtf8(path));
		}
	}

	// The server loads only the sectors map.dat names; others are saved but ignored.
	std::string map_dat;
	if (server_layout && rme::sec::readFile(data_folder / "dat" / "map.dat", map_dat)) {
		if (const auto info = rme::sec::readMapInfo(map_dat); info && info->sectors) {
			const auto outside = std::ranges::count_if(sectors, [&](const auto& entry) {
				const auto [sector_x, sector_y, sector_z] = entry.first;
				return !info->sectors->contains(rme::sec::SectorCoordinates { .x = sector_x, .y = sector_y, .z = sector_z });
			});
			if (outside > 0) {
				warnings.push_back(std::format("{} sectors lie outside the sectors map.dat lets the server load ({}-{}, {}-{}, floors {}-{}). They were saved, but the server ignores them.", outside, info->sectors->min.x, info->sectors->max.x, info->sectors->min.y, info->sectors->max.y, info->sectors->min.z, info->sectors->max.z));
			}
		}
	}

	g_gui.SetLoadDone(95, "Saving monster homes, NPCs, houses and marks...");
	if (server_layout) {
		server_data.write();
	} else {
		warnings.push_back("The folder is not in a server data folder (no dat/ next to it), so monster homes, NPCs, houses and marks were not saved.");
	}

	if (!stats.unreadable_sectors.empty()) {
		warnings.push_back(std::format("{} sector files could not be read and were not overwritten, starting with {}.", stats.unreadable_sectors.size(), stats.unreadable_sectors.front()));
	}
	if (stats.invalid_items > 0) {
		warnings.push_back(std::format("{} items with no type in objects.srv were left out.", stats.invalid_items));
	}
	if (stats.unknown_liquids > 0) {
		warnings.push_back(std::format("{} vials or pools hold a liquid the original server does not have; their liquid was left out.", stats.unknown_liquids));
	}
	if (stats.ot_only_ids > 0) {
		warnings.push_back(std::format("{} items have an action or unique ID, which sector files cannot hold; the IDs were left out.", stats.ot_only_ids));
	}
	if (stats.ot_only_flags > 0) {
		warnings.push_back(std::format("{} tiles are marked No-PvP or PvP zone, which the original server does not have; those marks were left out.", stats.ot_only_flags));
	}
	spdlog::info("IOMapSec: saved {} sectors to {} ({} written, {} unchanged)", sectors.size(), FileSystem::PathToUtf8(folder), stats.sectors_written, stats.sectors_unchanged);

	if (!stats.failed_files.empty()) {
		errorstr = wxstr(std::format("{} files could not be written, starting with \"{}\".", stats.failed_files.size(), stats.failed_files.front()));
		return false;
	}
	return true;
}
