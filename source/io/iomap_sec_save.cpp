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
#include "game/srv_liquids.h"
#include "game/complexitem.h"
#include "game/creature.h"
#include "game/item.h"
#include "io/sec/data/house_files.h"
#include "io/sec/data/map_dat_file.h"
#include "io/sec/data/monster_files.h"
#include "io/sec/data/npc_files.h"
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
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <spdlog/spdlog.h>
#include <tuple>

namespace {

	using rme::sec::AbsolutePosition;
	using rme::sec::InstanceAttribute;
	using rme::sec::ObjectAttribute;
	using rme::sec::SectorObject;
	using rme::sec::SectorTile;
	using rme::sec::TileFlag;

	AbsolutePosition toAbsolute(const Position& position) {
		return AbsolutePosition { .x = position.x, .y = position.y, .z = position.z };
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

	// Tiles by sector (x, y, z).
	using SectorTiles = std::map<std::tuple<int, int, int>, std::vector<const Tile*>>;

	// The range of the saved sectors, for a new map.dat.
	std::optional<rme::sec::SectorRange> sectorRange(const SectorTiles& sectors) {
		if (sectors.empty()) {
			return std::nullopt;
		}
		const auto [first_x, first_y, first_z] = sectors.begin()->first;
		rme::sec::SectorRange range {
			.min = { .x = first_x, .y = first_y, .z = first_z },
			.max = { .x = first_x, .y = first_y, .z = first_z },
		};
		for (const auto& [key, tiles] : sectors) {
			const auto [x, y, z] = key;
			range.min = { .x = std::min(range.min.x, x), .y = std::min(range.min.y, y), .z = std::min(range.min.z, z) };
			range.max = { .x = std::max(range.max.x, x), .y = std::max(range.max.y, y), .z = std::max(range.max.z, z) };
		}
		return range;
	}

	// A sector map is opened with the dat/objects.srv next to it, so a folder
	// without one gets a copy of the file its items came from.
	void copyObjectsFile(const rme::sec::ServerDataFolder& data, std::vector<std::filesystem::path>& created, std::vector<std::string>& warnings) {
		const std::filesystem::path target = data.dat() / "objects.srv";
		std::error_code ec;
		if (std::filesystem::exists(target, ec)) {
			return;
		}
		const std::filesystem::path source(g_version.GetCurrentVersion().getObjectsPath().GetFullPath().ToStdWstring());
		std::filesystem::create_directories(data.dat(), ec);
		if (source.empty() || !std::filesystem::copy_file(source, target, ec)) {
			warnings.push_back(std::format("objects.srv could not be copied to \"{}\"; the map opens again only once that folder has it.", FileSystem::PathToUtf8(data.dat())));
			return;
		}
		created.push_back(target);
	}

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

	SaveStats stats;
	std::vector<Tile*> monsters;
	std::vector<Tile*> npcs;

	g_gui.SetLoadDone(0, "Collecting sectors...");
	SectorTiles sectors;
	for (auto& location : map.tiles()) {
		Tile* tile = location.get();
		if (!tile) {
			continue;
		}
		const Position& position = tile->getPosition();
		sectors[std::tuple(position.x / rme::sec::kSectorSize, position.y / rme::sec::kSectorSize, position.z)].push_back(tile);
		if (tile->creature) {
			(tile->creature->isNpc() ? npcs : monsters).push_back(tile);
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

	const rme::sec::ServerDataFolder data { .root = data_folder };

	// The server loads only the sectors map.dat names; others are saved but ignored.
	std::string map_dat;
	if (rme::sec::readFile(data.dat() / "map.dat", map_dat)) {
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

	// The server files around the map folder, each written by its own handler
	// (io/sec/data/). A folder without them becomes a server data folder.
	g_gui.SetLoadDone(95, "Saving monster homes, NPCs, houses, towns and marks...");
	std::vector<std::filesystem::path> created;
	copyObjectsFile(data, created, warnings);
	rme::sec::ServerFileWriter files;
	rme::sec::monster_files::save(monsters, data, files, warnings);
	rme::sec::npc_files::save(map, npcs, data, files, warnings);
	rme::sec::house_files::save(map, data, files, warnings);
	rme::sec::map_dat::save(map, data, sectorRange(sectors), files, warnings);
	std::ranges::copy(files.failedFiles(), std::back_inserter(stats.failed_files));
	std::ranges::copy(files.createdFiles(), std::back_inserter(created));
	if (!created.empty()) {
		std::string names;
		for (const std::filesystem::path& path : created) {
			names += (names.empty() ? "" : ", ") + FileSystem::PathToUtf8(path.lexically_relative(data_folder));
		}
		warnings.push_back(std::format("New server files in \"{}\": {}.", FileSystem::PathToUtf8(data_folder), names));
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
