//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "io/iomap_sec.h"
#include "io/sec/data/creature_placement.h"
#include "io/sec/data/house_files.h"
#include "io/sec/data/map_dat_file.h"
#include "io/sec/data/monster_files.h"
#include "io/sec/data/npc_files.h"
#include "io/sec/data/server_marker_files.h"
#include "io/sec/srv_script_lexer.h"
#include "io/sec/srv_world_files.h"
#include "io/sec/sec_file_io.h"
#include "io/sec/sec_sector_parser.h"

#include "app/client_version.h"
#include "app/managers/version_manager.h"
#include "game/srv_markers.h"
#include "game/srv_liquids.h"
#include "game/complexitem.h"
#include "game/item.h"
#include "io/iomap_otbm.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "ui/gui.h"
#include "util/file_system.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <spdlog/spdlog.h>

namespace {

	using rme::sec::InstanceAttribute;
	using rme::sec::listSectorFiles;
	using rme::sec::readFile;
	using rme::sec::ObjectAttribute;
	using rme::sec::SectorFile;
	using rme::sec::SectorObject;
	using rme::sec::SectorTile;
	using rme::sec::TileFlag;

	constexpr size_t kMaxListedUnknownTypes = 20;

	std::filesystem::path toPath(const FileName& identifier) {
		return std::filesystem::path(identifier.GetFullPath().ToStdWstring());
	}

	bool isSectorFile(const std::filesystem::path& path) {
		return rme::sec::parseSectorFileName(path.filename().string()).has_value();
	}

	struct LoadStats {
		std::map<uint16_t, size_t> unknown_types; // server type ID -> occurrences
		size_t custom_attributes = 0;
		size_t dropped_contents = 0;
		size_t replaced_grounds = 0;
	};

	// Turns sector objects into editor items. Server attributes without a
	// native equivalent on the item (key and keyhole numbers, quest chests,
	// door levels and quests, expiry) are kept as item attributes under their
	// server names; the item panel and tooltips show them.
	class ItemBuilder {
	public:
		explicit ItemBuilder(LoadStats& stats) :
			stats_(stats) { }

		// Item definitions come from the server's objects.srv, so its type
		// IDs are the editor's server IDs.
		std::unique_ptr<Item> create(const SectorObject& object) {
			if (!g_item_definitions.exists(object.type_id)) {
				++stats_.unknown_types[object.type_id];
				return nullptr;
			}

			std::unique_ptr<Item> item = Item::Create(object.type_id);
			if (!item) {
				++stats_.unknown_types[object.type_id];
				return nullptr;
			}

			// The server has no liquid in a pool or vial, and no charges on a
			// rune, until an attribute sets them; the editor's defaults for new
			// items are water and 1 charge.
			if (item->isSplash() || item->isFluidContainer()) {
				item->setSubtype(LIQUID_NONE);
			} else if (item->isCharged() && !item->isStackable()) {
				item->setSubtype(0);
			}
			for (const ObjectAttribute& attribute : object.attributes) {
				apply(*item, attribute);
			}

			if (!object.content.empty()) {
				if (Container* container = item->asContainer()) {
					for (const SectorObject& child : object.content) {
						if (auto child_item = create(child)) {
							container->getVector().push_back(std::move(child_item));
						}
					}
				} else {
					++stats_.dropped_contents;
				}
			}
			return item;
		}

	private:
		void apply(Item& item, const ObjectAttribute& attribute) {
			switch (attribute.key) {
				case InstanceAttribute::Amount:
					if (item.isStackable()) {
						item.setSubtype(clampSubtype(attribute.number));
						return;
					}
					break;
				case InstanceAttribute::Charges:
					if (item.isCharged()) {
						item.setSubtype(clampSubtype(attribute.number));
						return;
					}
					break;
				case InstanceAttribute::ContainerLiquidType:
				case InstanceAttribute::PoolLiquidType:
					if ((item.isFluidContainer() || item.isSplash()) && attribute.number >= 0 && attribute.number < static_cast<int64_t>(rme::srv::kLiquids.size())) {
						item.setSubtype(static_cast<uint16_t>(rme::srv::kLiquids[static_cast<size_t>(attribute.number)]));
						return;
					}
					break;
				case InstanceAttribute::String:
					item.setText(attribute.text);
					return;
				case InstanceAttribute::AbsTeleportDestination:
					if (Teleport* teleport = item.asTeleport()) {
						const auto destination = rme::sec::unpackAbsoluteCoordinate(attribute.number);
						teleport->setDestination(Position(destination.x, destination.y, destination.z));
						return;
					}
					break;
				default:
					break;
			}

			const std::string name(rme::sec::attributeName(attribute.key));
			if (attribute.key == InstanceAttribute::Editor) {
				item.setAttribute(name, attribute.text);
			} else {
				item.setAttribute(name, static_cast<int32_t>(attribute.number));
			}
			++stats_.custom_attributes;
		}

		static uint16_t clampSubtype(int64_t value) {
			return static_cast<uint16_t>(std::clamp<int64_t>(value, 0, 0xFFFF));
		}

		LoadStats& stats_;
	};

	// Sector files list ground and always-on-top items first, then the other
	// items with the topmost first. The editor keeps those bottom to top.
	void addObjects(Tile& tile, std::span<const SectorObject> objects, ItemBuilder& builder, LoadStats& stats) {
		std::vector<std::unique_ptr<Item>> stacked;
		for (const SectorObject& object : objects) {
			std::unique_ptr<Item> item = builder.create(object);
			if (!item) {
				continue;
			}
			if (item->isGroundTile()) {
				if (tile.hasGround()) {
					++stats.replaced_grounds;
				}
				tile.addItemFast(std::move(item));
			} else if (item->isAlwaysOnBottom()) {
				tile.addItemFast(std::move(item));
			} else {
				stacked.push_back(std::move(item));
			}
		}
		for (auto& item : stacked | std::views::reverse) {
			tile.addItemFast(std::move(item));
		}
	}

	uint32_t toMapFlags(const SectorTile& sector_tile) {
		uint32_t flags = TILESTATE_NONE;
		if (sector_tile.hasFlag(TileFlag::ProtectionZone)) {
			flags |= TILESTATE_PROTECTIONZONE;
		}
		if (sector_tile.hasFlag(TileFlag::NoLogout)) {
			flags |= TILESTATE_NOLOGOUT;
		}
		if (sector_tile.hasFlag(TileFlag::Refresh)) {
			flags |= TILESTATE_REFRESH;
		}
		return flags;
	}

} // namespace

bool IOMapSec::isSectorMap(const FileName& identifier) {
	const std::filesystem::path path = toPath(identifier);
	std::error_code ec;
	if (std::filesystem::is_directory(path, ec)) {
		for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
			if (isSectorFile(entry.path())) {
				return true;
			}
		}
		return false;
	}
	return isSectorFile(path);
}

std::filesystem::path IOMapSec::sectorFolder(const FileName& identifier) {
	const std::filesystem::path path = toPath(identifier);
	std::error_code ec;
	return std::filesystem::is_directory(path, ec) ? path : path.parent_path();
}

bool IOMapSec::isSectorTarget(const FileName& identifier) {
	const std::filesystem::path path = toPath(identifier);
	std::error_code ec;
	return std::filesystem::is_directory(path, ec) || rme::srv::equalsIgnoreCase(path.extension().string(), rme::sec::kSectorFileExtension);
}

wxFileName IOMapSec::objectsFile(const FileName& identifier) {
	// Server data layout: <data>/<map folder>/*.sec and <data>/dat/objects.srv
	const std::filesystem::path path = sectorFolder(identifier).parent_path() / "dat" / "objects.srv";
	return wxFileName(wxString(path.wstring()));
}

ClientVersion* IOMapSec::findClientVersion() {
	const auto matches = [](const ClientVersion* version) {
		return version && version->getItemDefinitionMode() == ItemDefinitionMode::DatSrv;
	};

	if (ClientVersion* loaded = g_version.getLoadedVersion(); matches(loaded)) {
		return loaded;
	}

	ClientVersion* best = nullptr;
	for (ClientVersion* version : ClientVersion::getAllVisible()) {
		if (!matches(version)) {
			continue;
		}
		if (!best || (version->hasConfiguredClientPath() && !best->hasConfiguredClientPath())) {
			best = version;
		}
	}
	return best;
}

bool IOMapSec::peekStartupInfo(const FileName& identifier, OTBMStartupPeekResult& out_info) {
	out_info = {};
	out_info.is_sector_map = true;

	const std::filesystem::path folder = sectorFolder(identifier);
	out_info.map_name = wxstr(FileSystem::PathToUtf8(folder.filename()));
	wxDateTime modified_time;
	if (identifier.GetTimes(nullptr, &modified_time, nullptr)) {
		out_info.modified_time = modified_time;
	}

	const std::vector<SectorFile> files = listSectorFiles(folder);
	if (files.empty()) {
		out_info.has_error = true;
		out_info.error_message = wxstr(std::format("No sector files (XXXX-YYYY-ZZ.sec) found in \"{}\".", FileSystem::PathToUtf8(folder)));
		return false;
	}

	const wxFileName objects = objectsFile(identifier);
	if (!objects.FileExists()) {
		out_info.has_error = true;
		out_info.error_message = wxstr(std::format("objects.srv was not found at \"{}\".", nstr(objects.GetFullPath())));
		return false;
	}

	int max_sector_x = 0;
	int max_sector_y = 0;
	for (const SectorFile& file : files) {
		max_sector_x = std::max(max_sector_x, file.coordinates.x);
		max_sector_y = std::max(max_sector_y, file.coordinates.y);
	}
	const auto extent = [](int max_sector) {
		return static_cast<uint16_t>(std::clamp<int64_t>((int64_t { max_sector } + 1) * rme::sec::kSectorSize, 0, 0xFFFF));
	};
	out_info.width = extent(max_sector_x);
	out_info.height = extent(max_sector_y);
	out_info.description = wxstr(std::format("Sector map: {} sector files. Items from {}", files.size(), nstr(objects.GetFullPath())));
	return true;
}

bool IOMapSec::loadMap(Map& map, const FileName& identifier) {
	if (!g_version.IsVersionLoaded() || g_version.GetCurrentVersion().getItemDefinitionMode() != ItemDefinitionMode::DatSrv) {
		errorstr = "Sector maps need a client version that reads its items from objects.srv (configType dat_srv).";
		return false;
	}

	const ClientVersion& client = g_version.GetCurrentVersion();
	version.otbm = client.getPrefferedMapVersionID();
	version.client = client.getProtocolID();
	version.items_major = client.getOTBVersion().format_version;

	const std::filesystem::path folder = sectorFolder(identifier);
	std::vector<SectorFile> files = listSectorFiles(folder);
	if (files.empty()) {
		errorstr = wxstr(std::format("No sector files (XXXX-YYYY-ZZ.sec) found in \"{}\".", FileSystem::PathToUtf8(folder)));
		return false;
	}
	std::ranges::sort(files, {}, [](const SectorFile& file) {
		return std::tuple(file.coordinates.z, file.coordinates.y, file.coordinates.x);
	});

	const auto started = std::chrono::steady_clock::now();
	LoadStats stats;
	ItemBuilder builder(stats);
	std::string text;
	int max_x = 0;
	int max_y = 0;
	size_t loaded_sectors = 0;

	for (size_t index = 0; index < files.size(); ++index) {
		const SectorFile& file = files[index];
		if (index % 64 == 0) {
			const int percent = static_cast<int>(index * 100 / files.size());
			g_gui.SetLoadDone(percent, wxstr(std::format("Loading sectors ({}/{})...", index, files.size())));
		}

		const std::string file_name = FileSystem::PathToUtf8(file.path.filename());
		const auto [sector_x, sector_y, sector_z] = file.coordinates;
		// The sector numbers come from the file name, so they are checked before multiplying.
		const auto fits = [](int sector, int map_size) { return sector >= 0 && sector < map_size / rme::sec::kSectorSize; };
		if (sector_z < 0 || sector_z >= MAP_LAYERS || !fits(sector_x, MAP_MAX_WIDTH) || !fits(sector_y, MAP_MAX_HEIGHT)) {
			warnings.push_back(std::format("{}: sector is outside the map area, skipped.", file_name));
			continue;
		}
		const int base_x = sector_x * rme::sec::kSectorSize;
		const int base_y = sector_y * rme::sec::kSectorSize;

		if (!readFile(file.path, text)) {
			warnings.push_back(std::format("{}: could not be read, skipped.", file_name));
			continue;
		}

		const auto sector = rme::sec::parseSector(text);
		if (!sector) {
			warnings.push_back(std::format("{}:{}: {}; sector skipped.", file_name, sector.error().line, sector.error().message));
			continue;
		}

		for (const SectorTile& sector_tile : sector->tiles) {
			const int x = base_x + sector_tile.offset_x;
			const int y = base_y + sector_tile.offset_y;
			Tile* tile = map.createTile(x, y, sector_z);
			if (!tile) {
				continue;
			}
			tile->addMapFlags(toMapFlags(sector_tile));
			addObjects(*tile, sector_tile.objects, builder, stats);
			TileOperations::update(tile);
			max_x = std::max(max_x, x);
			max_y = std::max(max_y, y);
		}
		++loaded_sectors;
	}

	map.setWidth(max_x + 1);
	map.setHeight(max_y + 1);
	map.setMapDescription(std::format("Imported from sector files in {}", FileSystem::PathToUtf8(folder)));

	// The server files around the map folder, each read by its own handler (io/sec/data/).
	g_gui.SetLoadDone(100, "Loading towns, houses, monsters and NPCs...");
	const rme::sec::ServerDataFolder data { .root = folder.parent_path() };
	const std::optional<rme::sec::MapInfo> info = rme::sec::map_dat::load(map, data, warnings);
	rme::sec::house_files::load(map, data, warnings);
	const rme::sec::monster_files::Races races = rme::sec::monster_files::loadRaces(data, warnings);
	rme::sec::CreaturePlacer placer(map);
	rme::sec::monster_files::loadHomes(data, races, placer, warnings);
	std::vector<CreatureType*> npcs = rme::sec::npc_files::load(data, placer, warnings);
	rme::sec::addServerCreaturePalette(races.types, std::move(npcs));
	placer.report(warnings);
	map.server_markers = rme::sec::server_marker_files::load(data, info ? &*info : nullptr, races.by_race, warnings);

	if (info && info->sectors) {
		const auto outside = [&](const SectorFile& file) { return !info->sectors->contains(file.coordinates); };
		if (const auto first = std::ranges::find_if(files, outside); first != files.end()) {
			warnings.push_back(std::format("{} sector files lie outside the sectors map.dat lets the server load ({}-{}, {}-{}, floors {}-{}), starting with {}; the server ignores them.", std::ranges::count_if(files, outside), info->sectors->min.x, info->sectors->max.x, info->sectors->min.y, info->sectors->max.y, info->sectors->min.z, info->sectors->max.z, FileSystem::PathToUtf8(first->path.filename())));
		}
	}

	if (!stats.unknown_types.empty()) {
		size_t total = 0;
		size_t listed_count = 0;
		std::string listed;
		for (const auto& [type_id, count] : stats.unknown_types) {
			total += count;
			if (listed_count++ < kMaxListedUnknownTypes) {
				listed += std::format("{}{} (x{})", listed.empty() ? "" : ", ", type_id, count);
			}
		}
		warnings.push_back(std::format("{} objects of {} type IDs are not defined in objects.srv and were skipped: {}{}", total, stats.unknown_types.size(), listed, stats.unknown_types.size() > kMaxListedUnknownTypes ? ", ..." : ""));
	}
	if (stats.dropped_contents > 0) {
		warnings.push_back(std::format("{} objects had contents but are not containers in objects.srv; the contents were skipped.", stats.dropped_contents));
	}
	if (stats.replaced_grounds > 0) {
		warnings.push_back(std::format("{} tiles listed more than one ground item; only the last one was kept.", stats.replaced_grounds));
	}
	// OTBM 4 stores them in the item attribute map; older versions drop them on save.
	if (stats.custom_attributes > 0 && version.otbm != MAP_OTBM_4) {
		warnings.push_back(std::format("{} server attributes (key and keyhole numbers, quest chests, door levels and quests, expiry times) are kept on the items, but this client saves OTBM version {}, which drops them. OTBM version 4 keeps them.", stats.custom_attributes, static_cast<int>(version.otbm) + 1));
	}

	spdlog::info("IOMapSec: loaded {} of {} sectors ({} tiles) from {} in {:.2f} s", loaded_sectors, files.size(), map.getTileCount(), FileSystem::PathToUtf8(folder), std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());

	if (loaded_sectors == 0) {
		errorstr = "None of the sector files could be loaded.";
		return false;
	}
	return true;
}
