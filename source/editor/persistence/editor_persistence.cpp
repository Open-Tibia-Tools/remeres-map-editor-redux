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

	if (g_version.GetCurrentVersion().getProtocolID() != ver.client && !load_options.force_client_mismatch) {
		throw std::runtime_error(std::format("Client version mismatch. Expected protocol {} but got protocol {}", ver.client, g_version.GetCurrentVersion().getProtocolID()));
	}

	editor.map.open(fn.string(), on_progress);
}

SaveResult EditorPersistence::saveMap(Editor& editor, const std::filesystem::path& filename, ProgressCallback on_progress) {
	SaveResult result;
	std::filesystem::path save_path = filename;
	bool save_as = false;

	if (save_path.empty()) {
		save_path = editor.map.getFilename();
		save_as = (save_path != std::filesystem::path(editor.map.getFilename()));
	}

	// If not named yet, propagate the file name to the auxilliary files
	if (editor.map.unnamed) {
		std::string stem = save_path.stem().string();
		editor.map.setSpawnFilename(stem + "-spawn.xml");
		editor.map.setHouseFilename(stem + "-house.xml");
		editor.map.setWaypointFilename(stem + "-waypoint.xml");
		editor.map.unnamed = false;
	}

	std::filesystem::path parent_dir = save_path.parent_path();
	std::string base_stem = save_path.stem().string();

	std::filesystem::path backup_otbm;
	std::filesystem::path backup_house;
	std::filesystem::path backup_spawn;
	std::filesystem::path backup_waypoint;

	std::error_code ec;
	if (std::filesystem::exists(save_path, ec)) {
		backup_otbm = parent_dir / (base_stem + ".otbm~");
		std::filesystem::remove(backup_otbm, ec);
		std::filesystem::rename(save_path, backup_otbm, ec);
	}

	std::filesystem::path house_path = parent_dir / editor.map.getHouseFilename();
	if (std::filesystem::exists(house_path, ec)) {
		backup_house = parent_dir / (std::filesystem::path(editor.map.getHouseFilename()).stem().string() + ".xml~");
		std::filesystem::remove(backup_house, ec);
		std::filesystem::rename(house_path, backup_house, ec);
	}

	std::filesystem::path spawn_path = parent_dir / editor.map.getSpawnFilename();
	if (std::filesystem::exists(spawn_path, ec)) {
		backup_spawn = parent_dir / (std::filesystem::path(editor.map.getSpawnFilename()).stem().string() + ".xml~");
		std::filesystem::remove(backup_spawn, ec);
		std::filesystem::rename(spawn_path, backup_spawn, ec);
	}

	std::filesystem::path waypoint_path = parent_dir / editor.map.getWaypointFilename();
	if (std::filesystem::exists(waypoint_path, ec)) {
		backup_waypoint = parent_dir / (std::filesystem::path(editor.map.getWaypointFilename()).stem().string() + ".xml~");
		std::filesystem::remove(backup_waypoint, ec);
		std::filesystem::rename(waypoint_path, backup_waypoint, ec);
	}

	// Save the map
	MapBackupService::WriteCrashMarker({backup_otbm.string(), backup_house.string(), backup_spawn.string(), backup_waypoint.string()});

	{
		editor.map.filename = save_path.string();
		editor.map.name = save_path.filename().string();

		IOMapOTBM mapsaver(editor.map.getVersion());
		if (on_progress) {
			mapsaver.setProgressCallback(on_progress);
		}
		bool success = mapsaver.saveMap(editor.map, save_path);

		if (!success) {
			// Restore temporary backup files
			if (!backup_otbm.empty()) {
				std::filesystem::rename(backup_otbm, save_path, ec);
			}
			if (!backup_house.empty()) {
				std::filesystem::rename(backup_house, house_path, ec);
			}
			if (!backup_spawn.empty()) {
				std::filesystem::rename(backup_spawn, spawn_path, ec);
			}
			if (!backup_waypoint.empty()) {
				std::filesystem::rename(backup_waypoint, waypoint_path, ec);
			}

			MapBackupService::ClearCrashMarker();
			result.error_message = "Could not save, unable to open target for writing.";
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
		if (!backup_waypoint.empty()) {
			std::filesystem::rename(backup_waypoint, parent_dir / (std::filesystem::path(editor.map.getWaypointFilename()).stem().string() + "." + date_str + ".xml"), ec);
		}
	} else {
		std::filesystem::remove(backup_otbm, ec);
		std::filesystem::remove(backup_house, ec);
		std::filesystem::remove(backup_spawn, ec);
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
						// Conflict! Find a newd id and replace old
						uint32_t new_id = editor.map.houses.getEmptyID();
						house_id_map[imported_house->getID()] = new_id;
						imported_house->setID(new_id);
					}
				} else {
				}
				break;
			}
			case IMPORT_INSERT: {
				// Find a newd id and replace old
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

	editor.map.setWidth(newsize_x);
	editor.map.setHeight(newsize_y);

	result.success = true;
	return result;
}

bool EditorPersistence::importMiniMap(Editor& editor, const std::filesystem::path& filename, int import, int import_x_offset, int import_y_offset, int import_z_offset) {
	return false;
}
