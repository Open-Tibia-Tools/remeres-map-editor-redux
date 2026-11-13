#include "editor/persistence/editor_persistence.h"
#include "editor/persistence/map_backup_service.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"
#include "editor/selection.h"
#include "map/map.h"
#include "io/iomap_otbm.h"
#include "app/settings.h"
#include "app/managers/version_manager.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <unordered_map>
#include <spdlog/spdlog.h>

void EditorPersistence::loadMap(Editor& editor, const std::filesystem::path& fn, const MapLoadOptions& load_options, ProgressCallback on_progress) {
	MapVersion ver;
	if (!IOMapOTBM::getVersionInfo(fn, ver)) {
		throw std::runtime_error("Could not open file \"" + fn.string() + "\".\nThis is not a valid OTBM file or it does not exist.");
	}

	const auto loaded_major = static_cast<uint32_t>(g_version.GetCurrentVersion().getOTBVersion().format_version);
	const bool mismatch = (g_version.GetCurrentVersion().getProtocolID() != ver.client) ||
		(ver.items_major != 0 && loaded_major != 0 && loaded_major != ver.items_major);
	if (mismatch && !load_options.force_client_mismatch) {
		throw std::runtime_error(std::format("Client version mismatch. Expected protocol {} (OTB major {}) but got protocol {} (OTB major {})", ver.client, ver.items_major, g_version.GetCurrentVersion().getProtocolID(), loaded_major));
	}

	editor.map.open(fn.string(), on_progress);
}

SaveResult EditorPersistence::saveMap(Editor& editor, const std::filesystem::path& filename, ProgressCallback on_progress) {
	SaveResult result;
	std::filesystem::path save_path = filename;
	bool save_as = false;

	if (save_path.empty()) {
		save_path = editor.map.getFilename();
	} else {
		save_as = (save_path != std::filesystem::path(editor.map.getFilename()));
	}

	const std::string prev_filename = editor.map.getFilename();
	const std::string prev_name = editor.map.getName();
	const bool prev_unnamed = editor.map.unnamed;
	const std::string prev_spawn = editor.map.getSpawnFilename();
	const std::string prev_monster = editor.map.getMonsterFilename();
	const std::string prev_npc = editor.map.getNpcFilename();
	const std::string prev_house = editor.map.getHouseFilename();
	const std::string prev_zone = editor.map.getZoneFilename();
	const std::string prev_waypoint = editor.map.getWaypointFilename();

	// If not named yet, propagate the file name to the auxilliary files
	if (editor.map.unnamed) {
		std::string stem = save_path.stem().string();
		editor.map.setSpawnFilename(stem + "-spawn.xml");
		editor.map.setMonsterFilename(stem + "-monster.xml");
		editor.map.setNpcFilename(stem + "-npc.xml");
		editor.map.setHouseFilename(stem + "-house.xml");
		editor.map.setZoneFilename(stem + "-zones.xml");
		editor.map.setWaypointFilename(stem + "-waypoint.xml");
		editor.map.unnamed = false;
	}

	std::filesystem::path parent_dir = save_path.parent_path();
	std::string base_stem = save_path.stem().string();

	std::filesystem::path backup_otbm;
	std::filesystem::path backup_house;
	std::filesystem::path backup_spawn;
	std::filesystem::path backup_monster;
	std::filesystem::path backup_npc;
	std::filesystem::path backup_zone;
	std::filesystem::path backup_waypoint;

	auto createBackup = [](const std::filesystem::path& sourcePath, const std::filesystem::path& backupPath) {
		std::error_code ec;
		std::filesystem::remove(backupPath, ec);
		std::filesystem::rename(sourcePath, backupPath, ec);
		if (ec) {
			spdlog::warn("EditorPersistence: Failed to create backup '{}' -> '{}': {}", sourcePath.string(), backupPath.string(), ec.message());
		}
	};

	std::error_code ec;
	if (std::filesystem::exists(save_path, ec)) {
		backup_otbm = parent_dir / (base_stem + ".otbm~");
		createBackup(save_path, backup_otbm);
	}

	std::filesystem::path house_path = parent_dir / editor.map.getHouseFilename();
	if (std::filesystem::exists(house_path, ec)) {
		backup_house = parent_dir / (house_path.stem().string() + ".xml~");
		createBackup(house_path, backup_house);
	}

	if (editor.map.getVersion().otbm == MAP_OTBM_5) {
		std::filesystem::path monster_path = parent_dir / editor.map.getMonsterFilename();
		if (std::filesystem::exists(monster_path, ec)) {
			backup_monster = parent_dir / (monster_path.stem().string() + ".xml~");
			createBackup(monster_path, backup_monster);
		}

		std::filesystem::path npc_path = parent_dir / editor.map.getNpcFilename();
		if (std::filesystem::exists(npc_path, ec)) {
			backup_npc = parent_dir / (npc_path.stem().string() + ".xml~");
			createBackup(npc_path, backup_npc);
		}

		std::filesystem::path zone_path = parent_dir / editor.map.getZoneFilename();
		if (std::filesystem::exists(zone_path, ec)) {
			backup_zone = parent_dir / (zone_path.stem().string() + ".xml~");
			createBackup(zone_path, backup_zone);
		}
	} else {
		std::filesystem::path spawn_path = parent_dir / editor.map.getSpawnFilename();
		if (std::filesystem::exists(spawn_path, ec)) {
			backup_spawn = parent_dir / (spawn_path.stem().string() + ".xml~");
			createBackup(spawn_path, backup_spawn);
		}
	}

	std::filesystem::path waypoint_path = parent_dir / editor.map.getWaypointFilename();
	if (std::filesystem::exists(waypoint_path, ec)) {
		backup_waypoint = parent_dir / (waypoint_path.stem().string() + ".xml~");
		createBackup(waypoint_path, backup_waypoint);
	}

	// Save the map
	MapBackupService::WriteCrashMarker({backup_otbm.string(), backup_house.string(), backup_spawn.string(), backup_monster.string(), backup_npc.string(), backup_zone.string(), backup_waypoint.string()});

	{
		editor.map.filename = save_path.string();
		editor.map.name = save_path.filename().string();

		IOMapOTBM mapsaver(editor.map.getVersion());
		if (on_progress) {
			mapsaver.setProgressCallback(on_progress);
		}
		bool success = mapsaver.saveMap(editor.map, save_path);

		if (!success) {
			std::vector<std::string> failed_restores;
			auto restoreBackup = [&failed_restores](const std::filesystem::path& backupPath, const std::filesystem::path& targetPath) {
				if (backupPath.empty()) {
					if (!targetPath.empty()) {
						std::error_code ec;
						std::filesystem::remove(targetPath, ec);
					}
					return;
				}
				std::error_code ec;
				std::filesystem::remove(targetPath, ec);
				std::filesystem::rename(backupPath, targetPath, ec);
				if (ec) {
					spdlog::error("EditorPersistence: Failed to restore backup '{}' -> '{}': {}", backupPath.string(), targetPath.string(), ec.message());
					failed_restores.push_back(backupPath.string());
				}
			};

			restoreBackup(backup_otbm, save_path);
			if (!editor.map.getHouseFilename().empty()) {
				restoreBackup(backup_house, parent_dir / editor.map.getHouseFilename());
			}
			if (editor.map.getVersion().otbm == MAP_OTBM_5) {
				if (!editor.map.getMonsterFilename().empty()) {
					restoreBackup(backup_monster, parent_dir / editor.map.getMonsterFilename());
				}
				if (!editor.map.getNpcFilename().empty()) {
					restoreBackup(backup_npc, parent_dir / editor.map.getNpcFilename());
				}
				if (!editor.map.getZoneFilename().empty()) {
					restoreBackup(backup_zone, parent_dir / editor.map.getZoneFilename());
				}
			} else {
				if (!editor.map.getSpawnFilename().empty()) {
					restoreBackup(backup_spawn, parent_dir / editor.map.getSpawnFilename());
				}
			}
			if (!editor.map.getWaypointFilename().empty()) {
				restoreBackup(backup_waypoint, parent_dir / editor.map.getWaypointFilename());
			}

			bool keep_recovery_info = false;
			if (!failed_restores.empty()) {
				keep_recovery_info = true;
				result.error_message = "The save could not be rolled back completely. Previous data is kept in backup files (~).";
			} else {
				result.error_message = "Could not save, unable to open target for writing.";
			}

			if (!keep_recovery_info) {
				MapBackupService::ClearCrashMarker();
			}

			editor.map.filename = prev_filename;
			editor.map.name = prev_name;
			editor.map.unnamed = prev_unnamed;
			editor.map.setSpawnFilename(prev_spawn);
			editor.map.setMonsterFilename(prev_monster);
			editor.map.setNpcFilename(prev_npc);
			editor.map.setHouseFilename(prev_house);
			editor.map.setZoneFilename(prev_zone);
			editor.map.setWaypointFilename(prev_waypoint);
			return result;
		}

		MapBackupService::ClearCrashMarker();
	}

	// Move to permanent backup
	if (!save_as && g_settings.getInteger(Config::ALWAYS_MAKE_BACKUP)) {
		const std::string date_str = MapBackupService::GenerateTimestampString(false);

		if (!backup_otbm.empty()) {
			std::filesystem::rename(backup_otbm, parent_dir / (base_stem + "." + date_str + ".otbm"), ec);
		}
		if (!backup_house.empty()) {
			std::filesystem::rename(backup_house, parent_dir / (std::filesystem::path(editor.map.getHouseFilename()).stem().string() + "." + date_str + ".xml"), ec);
		}
		if (!backup_spawn.empty()) {
			std::filesystem::rename(backup_spawn, parent_dir / (std::filesystem::path(editor.map.getSpawnFilename()).stem().string() + "." + date_str + ".xml"), ec);
		}
		if (!backup_monster.empty()) {
			std::filesystem::rename(backup_monster, parent_dir / (std::filesystem::path(editor.map.getMonsterFilename()).stem().string() + "." + date_str + ".xml"), ec);
		}
		if (!backup_npc.empty()) {
			std::filesystem::rename(backup_npc, parent_dir / (std::filesystem::path(editor.map.getNpcFilename()).stem().string() + "." + date_str + ".xml"), ec);
		}
		if (!backup_zone.empty()) {
			std::filesystem::rename(backup_zone, parent_dir / (std::filesystem::path(editor.map.getZoneFilename()).stem().string() + "." + date_str + ".xml"), ec);
		}
		if (!backup_waypoint.empty()) {
			std::filesystem::rename(backup_waypoint, parent_dir / (std::filesystem::path(editor.map.getWaypointFilename()).stem().string() + "." + date_str + ".xml"), ec);
		}
	} else {
		std::filesystem::remove(backup_otbm, ec);
		std::filesystem::remove(backup_house, ec);
		std::filesystem::remove(backup_spawn, ec);
		std::filesystem::remove(backup_monster, ec);
		std::filesystem::remove(backup_npc, ec);
		std::filesystem::remove(backup_zone, ec);
		std::filesystem::remove(backup_waypoint, ec);
	}

	editor.map.clearChanges();
	result.success = true;
	return result;
}

void EditorPersistence::importTowns(Editor& editor, Map& imported_map, const Position& offset, ImportType house_import_type, std::unordered_map<uint32_t, uint32_t>& town_id_map) {
	if (house_import_type == IMPORT_DONT) {
		return;
	}

	for (auto tit = imported_map.towns.begin(); tit != imported_map.towns.end();) {
		Town* imported_town = tit->second.get();
		Town* current_town = editor.map.towns.getTown(imported_town->getID());

		Position oldexit = imported_town->getTemplePosition();
		Position newexit = oldexit + offset;

		bool skip = false;
		switch (house_import_type) {
			case IMPORT_MERGE: {
				town_id_map[imported_town->getID()] = imported_town->getID();
				if (current_town) {
					skip = true;
				}
				break;
			}
			case IMPORT_SMART_MERGE: {
				if (current_town) {
					// Compare and insert/merge depending on parameters
					if (current_town->getName() == imported_town->getName() && current_town->getID() == imported_town->getID()) {
						// Just add to map
						town_id_map[imported_town->getID()] = current_town->getID();
						skip = true;
					} else {
						// Conflict! Find a new id and replace old
						uint32_t old_id = imported_town->getID();
						uint32_t new_id = editor.map.towns.getEmptyID();
						imported_town->setID(new_id);
						town_id_map[old_id] = new_id;
					}
				} else {
					town_id_map[imported_town->getID()] = imported_town->getID();
				}
				break;
			}
			case IMPORT_INSERT: {
				// Find a new id and replace old
				uint32_t old_id = imported_town->getID();
				uint32_t new_id = editor.map.towns.getEmptyID();
				imported_town->setID(new_id);
				town_id_map[old_id] = new_id;
				break;
			}
			case IMPORT_DONT: {
				skip = true;
				break;
			}
		}

		if (skip) {
			++tit;
			continue;
		}

		if (newexit.isValid()) {
			imported_town->setTemplePosition(newexit);
			editor.map.getOrCreateTile(newexit)->getLocation()->increaseTownCount();
		}

		uint32_t town_id = imported_town->getID();
		if (!editor.map.towns.addTown(std::move(tit->second))) {
			spdlog::warn("Failed to add town {} during import (duplicate ID)", town_id);
		}

		tit = imported_map.towns.erase(tit);
	}
}

void EditorPersistence::importHouses(Editor& editor, Map& imported_map, const Position& offset, ImportType house_import_type, const std::unordered_map<uint32_t, uint32_t>& town_id_map, std::unordered_map<uint32_t, uint32_t>& house_id_map) {
	if (house_import_type == IMPORT_DONT) {
		return;
	}

	for (auto hit = imported_map.houses.begin(); hit != imported_map.houses.end();) {
		House* imported_house = hit->second.get();
		House* current_house = editor.map.houses.getHouse(imported_house->getID());

		if (auto it = town_id_map.find(imported_house->townid); it != town_id_map.end()) {
			imported_house->townid = it->second;
		} else {
			imported_house->townid = 0;
		}

		Position oldexit = imported_house->getExit();
		imported_house->setExit(nullptr, Position()); // Reset it

		bool skip = false;
		switch (house_import_type) {
			case IMPORT_MERGE: {
				house_id_map[imported_house->getID()] = imported_house->getID();
				if (current_house) {
					skip = true;
					Position newexit = oldexit + offset;
					if (newexit.isValid()) {
						current_house->setExit(&editor.map, newexit);
					}
				}
				break;
			}
			case IMPORT_SMART_MERGE: {
				if (current_house) {
					// Compare and insert/merge depending on parameters
					if (current_house->name == imported_house->name && current_house->townid == imported_house->townid) {
						// Just add to map
						house_id_map[imported_house->getID()] = current_house->getID();
						skip = true;
						Position newexit = oldexit + offset;
						if (newexit.isValid()) {
							current_house->setExit(&editor.map, newexit);
						}
					} else {
						// Conflict! Find a new id and replace old
						uint32_t new_id = editor.map.houses.getEmptyID();
						house_id_map[imported_house->getID()] = new_id;
						imported_house->setID(new_id);
					}
				} else {
				}
				break;
			}
			case IMPORT_INSERT: {
				// Find a new id and replace old
				uint32_t new_id = editor.map.houses.getEmptyID();
				house_id_map[imported_house->getID()] = new_id;
				imported_house->setID(new_id);
				break;
			}
			default:
				break;
		}

		if (skip) {
			++hit;
			continue;
		}

		Position newexit = oldexit + offset;
		if (newexit.isValid()) {
			imported_house->setExit(&editor.map, newexit);
		}
		uint32_t house_id = imported_house->getID();
		if (!editor.map.houses.addHouse(std::move(hit->second))) {
			spdlog::warn("Failed to add house {} during import (duplicate ID)", house_id);
		}

		hit = imported_map.houses.erase(hit);
	}
}

void EditorPersistence::importSpawns(Editor& editor, Map& imported_map, const Position& offset, ImportType spawn_import_type, std::map<Position, std::unique_ptr<Spawn>>& spawn_map) {
	if (spawn_import_type == IMPORT_DONT) {
		return;
	}

	for (auto siter = imported_map.spawns.begin(); siter != imported_map.spawns.end();) {
		Position old_spawn_pos = *siter;
		Position new_spawn_pos = *siter + offset;
		bool skip = false;

		switch (spawn_import_type) {
			case IMPORT_SMART_MERGE:
			case IMPORT_INSERT:
			case IMPORT_MERGE: {
				Tile* imported_tile = imported_map.getTile(old_spawn_pos);
				if (imported_tile) {
					ASSERT(imported_tile->spawn);
					spawn_map[new_spawn_pos] = std::move(imported_tile->spawn);
				}
				break;
			}
			case IMPORT_DONT: {
				skip = true;
				break;
			}
		}

		if (skip) {
			++siter;
		} else {
			Tile* imported_tile = imported_map.getTile(old_spawn_pos);
			if (imported_tile) {
				siter = imported_map.spawns.erase(siter);
			} else {
				++siter;
			}
		}
	}
}

MapImportResult EditorPersistence::importMap(
	Editor& editor,
	const std::filesystem::path& filename,
	const MapImportOptions& options,
	ProgressCallback on_progress
) {
	MapImportResult result;
	editor.selection.clear();
	editor.actionQueue->clear();

	Map imported_map;
	bool loaded = imported_map.open(filename.string(), on_progress);

	if (!loaded) {
		result.error = "Error loading map!\n" + imported_map.getError();
		return result;
	}
	result.warnings = imported_map.getWarnings();

	Position offset(options.import_x_offset, options.import_y_offset, 0);

	std::unordered_map<uint32_t, uint32_t> town_id_map;
	town_id_map.reserve(imported_map.towns.count());
	std::unordered_map<uint32_t, uint32_t> house_id_map;
	house_id_map.reserve(imported_map.houses.count());

	importTowns(editor, imported_map, offset, options.house_import_type, town_id_map);
	importHouses(editor, imported_map, offset, options.house_import_type, town_id_map, house_id_map);

	std::unordered_map<uint16_t, uint16_t> zone_id_map;
	if (editor.map.getVersion().otbm == MAP_OTBM_5 && imported_map.getVersion().otbm == MAP_OTBM_5) {
		zone_id_map.reserve(imported_map.zones.size());
		for (const auto& imported_zone : imported_map.zones) {
			if (const auto* existing = editor.map.findZone(imported_zone.name)) {
				zone_id_map[imported_zone.id] = existing->id;
			} else {
				const uint16_t new_id = editor.map.addZone(imported_zone.name);
				if (new_id != 0) {
					zone_id_map[imported_zone.id] = new_id;
				}
			}
		}
	}

	std::map<Position, std::unique_ptr<Spawn>> spawn_map;
	importSpawns(editor, imported_map, offset, options.spawn_import_type, spawn_map);

	// Plain merge of waypoints, very simple! :)
	for (auto& [name, waypoint] : imported_map.waypoints) {
		waypoint->pos += offset;
		editor.map.waypoints.addWaypoint(std::move(waypoint));
	}
	imported_map.waypoints.waypoints.clear();

	uint64_t tiles_to_import = imported_map.getTileCount();
	int newsize_x = editor.map.getWidth();
	int newsize_y = editor.map.getHeight();

	for (MapIterator mit = imported_map.begin(); mit != imported_map.end(); ++mit) {
		if (result.tiles_merged % 8092 == 0 && on_progress && tiles_to_import > 0) {
			on_progress(static_cast<int>(100.0 * static_cast<double>(result.tiles_merged) / static_cast<double>(tiles_to_import)), "Merging maps...");
		}
		++result.tiles_merged;

		Tile* import_tile = mit->get();
		Position new_pos = import_tile->getPosition() + offset;
		if (!new_pos.isValid()) {
			++result.discarded_tiles;
			continue;
		}

		if (!options.auto_resize && (new_pos.x > editor.map.getWidth() || new_pos.y > editor.map.getHeight())) {
			++result.discarded_tiles;
			continue;
		}

		newsize_x = std::max(newsize_x, int(new_pos.x));
		newsize_y = std::max(newsize_y, int(new_pos.y));

		std::unique_ptr<Tile> moved_tile = imported_map.setTile(import_tile->getPosition(), nullptr);
		TileLocation* location = editor.map.createTileL(new_pos);

		// Check if we should update any houses
		uint32_t new_houseid = 0;
		if (auto it = house_id_map.find(import_tile->getHouseID()); it != house_id_map.end()) {
			new_houseid = it->second;
		}

		House* house = editor.map.houses.getHouse(new_houseid);
		if (import_tile->isHouseTile() && options.house_import_type != IMPORT_DONT && house) {
			house->removeTile(import_tile);
			import_tile->setLocation(location);
			house->addTile(import_tile);
		} else {
			import_tile->setLocation(location);
		}

		if (offset != Position(0, 0, 0)) {
			for (auto& item : import_tile->items) {
				if (Teleport* teleport = dynamic_cast<Teleport*>(item.get())) {
					teleport->setDestination(teleport->getDestination() + offset);
				}
			}
		}

		Tile* old_tile = editor.map.getTile(new_pos);
		if (old_tile) {
			editor.map.removeSpawn(old_tile);
		}

		import_tile->spawn.reset();

		if (editor.map.getVersion().otbm == MAP_OTBM_5) {
			if (import_tile->hasZones()) {
				std::vector<uint16_t> remapped_zones;
				for (const uint16_t zid : import_tile->getZoneIds()) {
					if (auto it = zone_id_map.find(zid); it != zone_id_map.end()) {
						remapped_zones.push_back(it->second);
					}
				}
				std::sort(remapped_zones.begin(), remapped_zones.end());
				const auto last = std::unique(remapped_zones.begin(), remapped_zones.end());
				remapped_zones.erase(last, remapped_zones.end());
				import_tile->setZoneIds(remapped_zones);
			}
		} else if (import_tile->hasZones()) {
			import_tile->setZoneIds({});
		}

		(void)editor.map.setTile(new_pos, std::move(moved_tile));
	}

	for (auto& spawn_entry : spawn_map) {
		Position pos = spawn_entry.first;
		TileLocation* location = editor.map.createTileL(pos);
		Tile* tile = location->get();
		if (!tile) {
			tile = editor.map.createTile(pos.x, pos.y, pos.z);
		} else if (tile->spawn) {
			editor.map.removeSpawn(tile);
			tile->spawn.reset();
		}
		tile->spawn = std::move(spawn_entry.second);

		editor.map.addSpawn(tile);
	}

	if (editor.map.getVersion().otbm == MAP_OTBM_5) {
		editor.map.rebuildZoneIndex();
	}

	editor.map.setWidth(newsize_x);
	editor.map.setHeight(newsize_y);

	result.success = true;
	return result;
}

bool EditorPersistence::importMiniMap(Editor& editor, const std::filesystem::path& filename, int import, int import_x_offset, int import_y_offset, int import_z_offset) {
	return false;
}
