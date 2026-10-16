//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "map_xml_io.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/creatures.h"
#include "game/creature.h"
#include "game/spawn.h"
#include "game/town.h"
#include "game/house.h"
#include "app/settings.h"
#include "ui/gui.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <format>
#include <filesystem>
#include <ranges>

std::pair<std::string, std::string> MapXMLIO::normalizeMapFilePaths(const wxFileName& dir, const std::string& filename) {
	wxFileName resolved(dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME), wxstr(filename));
	resolved.Normalize(wxPATH_NORM_ALL, dir.GetPath());

	// Security: Prevent path traversal attacks by ensuring the resolved sidecar path
	// remains strictly contained within the map directory.
	const wxString base_dir = dir.GetPath();
	wxString base_dir_with_sep = dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME);
	if (!base_dir_with_sep.EndsWith(wxFileName::GetPathSeparator())) {
		base_dir_with_sep += wxFileName::GetPathSeparator();
	}
	if (filename.find("..") != std::string::npos || (resolved.IsAbsolute() && !resolved.GetFullPath().StartsWith(base_dir_with_sep))) {
		spdlog::warn("MapXMLIO: Sidecar file '{}' attempts path traversal outside map directory. Constraining to base name.", filename);
		resolved = wxFileName(base_dir, resolved.GetFullName());
	}

	std::string utf8_path = (const char*)(resolved.GetFullPath().mb_str(wxConvUTF8));
	std::string encoded_path = (const char*)(resolved.GetFullPath().mb_str(wxConvLocal));

	return { utf8_path, encoded_path };
}

namespace {
	enum class CreatureFilter {
		All,
		Monsters,
		Npcs,
	};

	struct SpawnFileFormat {
		const char* root;
		const char* spawnTag;
		CreatureFilter filter;
	};

	bool loadXMLFile(pugi::xml_document& doc, const std::string& path_utf8, const std::string& path_local) {
#if defined(_WIN32)
		return doc.load_file(wxstr(path_utf8).wc_str());
#else
		return doc.load_file(path_utf8.c_str());
#endif
	}

	bool saveXMLFile(const pugi::xml_document& doc, const std::string& path_utf8, const std::string& path_local) {
		std::error_code ec;
#if defined(_WIN32)
		std::filesystem::create_directories(std::filesystem::path(wxstr(path_utf8).wc_str()).parent_path(), ec);
		return doc.save_file(wxstr(path_utf8).wc_str(), "\t", pugi::format_default, pugi::encoding_utf8);
#else
		std::filesystem::create_directories(std::filesystem::path(path_utf8).parent_path(), ec);
		return doc.save_file(path_utf8.c_str(), "\t", pugi::format_default, pugi::encoding_utf8);
#endif
	}

	// OTBM 5 splits spawns into a monster file and an NPC file, each with its own tag names
	constexpr SpawnFileFormat STANDARD_SPAWN_FILE { "spawns", "spawn", CreatureFilter::All };
	constexpr SpawnFileFormat MONSTERS_SPAWN_FILE { "monsters", "monster", CreatureFilter::Monsters };
	constexpr SpawnFileFormat NPCS_SPAWN_FILE { "npcs", "npc", CreatureFilter::Npcs };

	bool loadSpawnFile(Map& map, const wxFileName& dir, const std::string& filename) {
		auto paths = MapXMLIO::normalizeMapFilePaths(dir, filename);
		if (!FileName(wxstr(paths.first)).FileExists()) {
			return false;
		}

		pugi::xml_document doc;
		if (!loadXMLFile(doc, paths.first, paths.second)) {
			return false;
		}
		return MapXMLIO::loadSpawns(map, doc);
	}

	void appendCreatureNode(pugi::xml_node spawnNode, const char* tag, int x, int y, int z, const SpawnAlternative& creature, bool isOtbm5) {
		pugi::xml_node creatureNode = spawnNode.append_child(tag);
		creatureNode.append_attribute("name") = creature.name.c_str();
		creatureNode.append_attribute("x") = x;
		creatureNode.append_attribute("y") = y;
		if (isOtbm5) {
			creatureNode.append_attribute("z") = z;
		}
		creatureNode.append_attribute("spawntime") = creature.spawntime;
		if (creature.direction != NORTH) {
			creatureNode.append_attribute("direction") = static_cast<int>(creature.direction);
		}
		if (isOtbm5 && creature.weight > 0) {
			creatureNode.append_attribute("weight") = creature.weight;
		}
	}

	// Polling the progress dialog pumps events, so it is not free: the
	// auxiliary writers check for cancellation every this many items.
	constexpr size_t kCancelPollStride = 128;

	bool shouldAbortWriting(const MapXMLIO::CancelCheck& shouldCancel, size_t& processed) {
		if (!shouldCancel || ++processed % kCancelPollStride != 0) {
			return false;
		}
		return shouldCancel();
	}

	bool writeSpawnDocument(const Map& map, pugi::xml_document& doc, const SpawnFileFormat& format, const MapXMLIO::CancelCheck& shouldCancel) {
		pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
		if (!decl) {
			return false;
		}
		decl.append_attribute("version") = "1.0";

		const bool isOtbm5 = format.filter != CreatureFilter::All;
		pugi::xml_node rootNode = doc.append_child(format.root);
		struct ResetSavedGuard {
			std::vector<Creature*>& list;
			~ResetSavedGuard() {
				for (auto* creature : list) {
					creature->reset();
				}
			}
		};
		std::vector<Creature*> creatureList;
		ResetSavedGuard guard { creatureList };

		size_t processed = 0;
		for (const auto& spawnPos : map.spawns) {
			if (shouldAbortWriting(shouldCancel, processed)) {
				return false;
			}

			const Tile* tile = map.getTile(spawnPos);
			if (!tile) {
				continue;
			}
			Spawn* spawn = tile->spawn.get();
			if (!spawn) {
				continue;
			}

			Position spawnPosition = spawnPos;
			pugi::xml_node spawnNode = rootNode.append_child(format.spawnTag);
			spawnNode.append_attribute("centerx") = spawnPosition.x;
			spawnNode.append_attribute("centery") = spawnPosition.y;
			spawnNode.append_attribute("centerz") = spawnPosition.z;

			const bool ownNpcRadius = format.filter == CreatureFilter::Npcs && spawn->getNpcSize() > 0;
			int32_t radius = ownNpcRadius ? spawn->getNpcSize() : spawn->getSize();
			spawnNode.append_attribute("radius") = radius;

			bool skippedOtherKind = false;
			for (int32_t y = -radius; y <= radius; ++y) {
				for (int32_t x = -radius; x <= radius; ++x) {
					const Tile* creatureTile = map.getTile(spawnPosition + Position(x, y, 0));
					if (!creatureTile || !creatureTile->creature || creatureTile->creature->isSaved()) {
						continue;
					}
					Creature* creature = creatureTile->creature.get();
					if (isOtbm5 && creature->isNpc() != (format.filter == CreatureFilter::Npcs)) {
						skippedOtherKind = true;
						continue;
					}

					const SpawnAlternative entry { creature->getName(), creature->getSpawnTime(), creature->getDirection(), creature->getWeight() };
					appendCreatureNode(spawnNode, creature->isNpc() ? "npc" : "monster", x, y, spawnPosition.z, entry, isOtbm5);
					if (isOtbm5) {
						for (const auto& alternative : creature->getAlternatives()) {
							appendCreatureNode(spawnNode, "monster", x, y, spawnPosition.z, alternative, isOtbm5);
						}
					}

					creature->save();
					creatureList.push_back(creature);
				}
			}

			// A spawn holding only the other kind of creature belongs to the other file; empty spawns stay with the monsters
			if (isOtbm5 && !spawnNode.first_child() && (skippedOtherKind || format.filter == CreatureFilter::Npcs)) {
				rootNode.remove_child(spawnNode);
			}
		}

		return true;
	}

	// In OTBM 5, house size counts every tile that is not a wall, plus tables and doors
	int32_t calculateOtbm5HouseSize(const Map& map, const House& house) {
		return static_cast<int32_t>(std::ranges::count_if(house.getTiles(), [&](const Position& pos) {
			const Tile* tile = map.getTile(pos);
			if (!tile) {
				return false;
			}
			const Item* top = tile->getTopItem();
			return !tile->getWall() || tile->getTable() || (top && top->isDoor());
		}));
	}

	bool saveSpawnFile(const Map& map, const wxFileName& dir, const std::string& filename, const SpawnFileFormat& format, const MapXMLIO::CancelCheck& shouldCancel) {
		if (filename.empty()) {
			return false;
		}
		auto paths = MapXMLIO::normalizeMapFilePaths(dir, filename);

		pugi::xml_document doc;
		if (writeSpawnDocument(map, doc, format, shouldCancel)) {
			return saveXMLFile(doc, paths.first, paths.second);
		}
		return false;
	}
}

bool MapXMLIO::loadSpawns(Map& map, const wxFileName& dir) {
	return loadSpawnFile(map, dir, map.getSpawnFilename());
}

bool MapXMLIO::loadMonsterSpawns(Map& map, const wxFileName& dir) {
	return loadSpawnFile(map, dir, map.getMonsterFilename());
}

bool MapXMLIO::loadNpcSpawns(Map& map, const wxFileName& dir) {
	return loadSpawnFile(map, dir, map.getNpcFilename());
}

bool MapXMLIO::loadSpawns(Map& map, pugi::xml_document& doc) {
	pugi::xml_node node;
	SpawnFileFormat format = STANDARD_SPAWN_FILE;
	for (const auto& candidate : { STANDARD_SPAWN_FILE, MONSTERS_SPAWN_FILE, NPCS_SPAWN_FILE }) {
		if ((node = doc.child(candidate.root))) {
			format = candidate;
			break;
		}
	}
	if (!node) {
		return false;
	}

	size_t alternatives = 0;
	for (auto spawnNode : node.children(format.spawnTag)) {
		Position spawnPosition {
			spawnNode.attribute("centerx").as_int(),
			spawnNode.attribute("centery").as_int(),
			spawnNode.attribute("centerz").as_int()
		};

		if (spawnPosition.x == 0 || spawnPosition.y == 0) {
			spdlog::warn("MapXMLIO: Bad position data on spawn, discarding...");
			continue;
		}

		int32_t radius = spawnNode.attribute("radius").as_int();
		if (radius < 1) {
			spdlog::warn("MapXMLIO: Invalid radius on spawn, discarding...");
			continue;
		}

		Tile* tile = map.getTile(spawnPosition);
		if (!tile) {
			tile = map.createTile(spawnPosition.x, spawnPosition.y, spawnPosition.z);
		}

		if (!tile) {
			spdlog::warn("MapXMLIO: Failed to create tile at {}:{}:{}", spawnPosition.x, spawnPosition.y, spawnPosition.z);
			continue;
		}

		// In partitioned spawn files (OTBM 5), monster and NPC files can each have a spawn on the same tile, with its own radius
		const bool sharedNpcSpawn = format.filter == CreatureFilter::Npcs && tile->spawn && (tile->spawn->getKinds() & Spawn::MONSTERS);
		const auto setRadius = [&](int32_t value) {
			sharedNpcSpawn ? tile->spawn->setNpcSize(value) : tile->spawn->setSize(value);
		};
		if (tile->spawn) {
			if (!sharedNpcSpawn) {
				radius = std::max(radius, tile->spawn->getSize());
			}
			setRadius(radius);
		} else {
			tile->spawn = std::make_unique<Spawn>(radius);
			map.addSpawn(tile);
		}
		if (format.filter != CreatureFilter::All) {
			tile->spawn->addKind(format.filter == CreatureFilter::Npcs ? Spawn::NPCS : Spawn::MONSTERS);
		}

		for (auto creatureNode : spawnNode.children()) {
			std::string nodeName = as_lower_str(creatureNode.name());
			if (nodeName != "monster" && nodeName != "npc") {
				continue;
			}

			bool isNpc = (nodeName == "npc");
			std::string name = creatureNode.attribute("name").as_string();
			if (name.empty()) {
				spdlog::warn("MapXMLIO: Creature missing name at spawn {}:{}:{}", spawnPosition.x, spawnPosition.y, spawnPosition.z);
				continue;
			}

			int32_t spawntime = creatureNode.attribute("spawntime").as_int();
			if (spawntime == 0) {
				spawntime = g_settings.getInteger(Config::DEFAULT_SPAWNTIME);
			}

			Direction direction = NORTH;
			int dir = creatureNode.attribute("direction").as_int(static_cast<int>(NORTH));
			if (dir >= DIRECTION_FIRST && dir <= DIRECTION_LAST) {
				direction = static_cast<Direction>(dir);
			}

			Position creaturePosition = spawnPosition;
			auto xAttr = creatureNode.attribute("x");
			auto yAttr = creatureNode.attribute("y");

			if (!xAttr || !yAttr) {
				spdlog::warn("MapXMLIO: Creature '{}' missing offset position at spawn {}:{}:{}", name, spawnPosition.x, spawnPosition.y, spawnPosition.z);
				continue;
			}

			creaturePosition.x += xAttr.as_int();
			creaturePosition.y += yAttr.as_int();
			if (auto zAttr = creatureNode.attribute("z")) {
				creaturePosition.z = zAttr.as_int();
			}
			const uint32_t weight = creatureNode.attribute("weight").as_uint();

			radius = std::clamp<int32_t>(
				std::max({ radius, std::abs(creaturePosition.x - spawnPosition.x), std::abs(creaturePosition.y - spawnPosition.y) }),
				1,
				g_settings.getInteger(Config::MAX_SPAWN_RADIUS)
			);
			setRadius(radius);

			Tile* creatureTile = (creaturePosition == spawnPosition) ? tile : map.getTile(creaturePosition);

			if (!creatureTile) {
				spdlog::warn("MapXMLIO: Creature '{}' at invalid position {}:{}:{}", name, creaturePosition.x, creaturePosition.y, creaturePosition.z);
				continue;
			}

			if (creatureTile->creature) {
				if (!isNpc && !creatureTile->creature->isNpc()) {
					creatureTile->creature->addAlternative({ name, spawntime, direction, weight });
					++alternatives;
				} else {
					spdlog::warn("MapXMLIO: Duplicate creature '{}' at {}:{}:{}", name, creaturePosition.x, creaturePosition.y, creaturePosition.z);
				}
				continue;
			}

			CreatureType* type = g_creatures[name];
			if (!type) {
				type = g_creatures.addMissingCreatureType(name, isNpc);
			}

			creatureTile->creature = std::make_unique<Creature>(type);
			creatureTile->creature->setDirection(direction);
			creatureTile->creature->setSpawnTime(spawntime);
			creatureTile->creature->setWeight(weight);
			if (isNpc && !type->isNpc) {
				creatureTile->creature->markAsNpcSpawn();
			}

			if (creatureTile->getLocation()->getSpawnCount() == 0) {
				if (!creatureTile->spawn) {
					creatureTile->spawn = std::make_unique<Spawn>(5);
					map.addSpawn(creatureTile);
				}
			}
		}
	}
	if (alternatives > 0) {
		spdlog::info("MapXMLIO: Kept {} weighted spawn alternatives (extra monsters sharing a tile; the server spawns one of them by weight)", alternatives);
	}
	return true;
}

bool MapXMLIO::saveSpawns(const Map& map, const wxFileName& dir, const CancelCheck& shouldCancel) {
	if (map.getVersion().otbm == MAP_OTBM_5) {
		const std::string monsterFile = map.getMonsterFilename().empty() ? (nstr(dir.GetName()) + "-monster.xml") : map.getMonsterFilename();
		const std::string npcFile = map.getNpcFilename().empty() ? (nstr(dir.GetName()) + "-npc.xml") : map.getNpcFilename();
		return saveSpawnFile(map, dir, monsterFile, MONSTERS_SPAWN_FILE, shouldCancel) && saveSpawnFile(map, dir, npcFile, NPCS_SPAWN_FILE, shouldCancel);
	}
	const std::string spawnFile = map.getSpawnFilename().empty() ? (nstr(dir.GetName()) + "-spawn.xml") : map.getSpawnFilename();
	return saveSpawnFile(map, dir, spawnFile, STANDARD_SPAWN_FILE, shouldCancel);
}

bool MapXMLIO::loadHouses(Map& map, const wxFileName& dir) {
	auto paths = normalizeMapFilePaths(dir, map.housefile);
	if (!FileName(wxstr(paths.first)).FileExists()) {
		return false;
	}

	pugi::xml_document doc;
	if (!loadXMLFile(doc, paths.first, paths.second)) {
#if defined(OTSERV_DEBUG_XML)
		spdlog::warn("MapXMLIO::loadHouses: load_file failed for {}", paths.second);
		return true;
#else
		return false;
#endif
	}
	return loadHouses(map, doc);
}

bool MapXMLIO::loadHouses(Map& map, pugi::xml_document& doc) {
	pugi::xml_node node = doc.child("houses");
	if (!node) {
		return false;
	}

	for (auto houseNode : node.children("house")) {
		uint32_t houseId = houseNode.attribute("houseid").as_uint();
		House* house = map.houses.getHouse(houseId);
		if (!house) {
			continue;
		}

		if (auto nameAttr = houseNode.attribute("name")) {
			house->name = nameAttr.as_string();
		} else {
			house->name = std::format("House #{}", house->getID());
		}

		Position exitPos(
			houseNode.attribute("entryx").as_int(),
			houseNode.attribute("entryy").as_int(),
			houseNode.attribute("entryz").as_int()
		);

		if (exitPos.x != 0 && exitPos.y != 0) {
			house->setExit(exitPos);
		}

		if (auto rentAttr = houseNode.attribute("rent")) {
			house->rent = rentAttr.as_int();
		}

		if (auto guildhallAttr = houseNode.attribute("guildhall")) {
			house->guildhall = guildhallAttr.as_bool();
		}

		house->clientid = houseNode.attribute("clientid").as_uint();
		house->beds = houseNode.attribute("beds").as_int(-1);

		if (auto townIdAttr = houseNode.attribute("townid")) {
			house->townid = townIdAttr.as_uint();
		} else {
			spdlog::warn("MapXMLIO: House {} has no town! Removed.", house->getID());
			map.houses.removeHouse(house);
			continue;
		}
	}
	return true;
}

bool MapXMLIO::saveHouses(const Map& map, const wxFileName& dir, const CancelCheck& shouldCancel) {
	const std::string houseFile = map.getHouseFilename().empty() ? (nstr(dir.GetName()) + "-house.xml") : map.getHouseFilename();
	auto paths = normalizeMapFilePaths(dir, houseFile);

	pugi::xml_document doc;
	if (saveHouses(map, doc, shouldCancel)) {
		return saveXMLFile(doc, paths.first, paths.second);
	}
	return false;
}

bool MapXMLIO::saveHouses(const Map& map, pugi::xml_document& doc, const CancelCheck& shouldCancel) {
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	if (!decl) {
		return false;
	}
	decl.append_attribute("version") = "1.0";

	pugi::xml_node houseNodes = doc.append_child("houses");
	size_t processed = 0;
	for (const auto& [id, housePtr] : map.houses) {
		if (shouldAbortWriting(shouldCancel, processed)) {
			return false;
		}

		const auto* house = housePtr.get();
		pugi::xml_node houseNode = houseNodes.append_child("house");

		houseNode.append_attribute("name") = house->name.c_str();
		houseNode.append_attribute("houseid") = house->getID();

		const Position& exitPos = house->getExit();
		houseNode.append_attribute("entryx") = exitPos.x;
		houseNode.append_attribute("entryy") = exitPos.y;
		houseNode.append_attribute("entryz") = exitPos.z;

		houseNode.append_attribute("rent") = house->rent;
		if (house->guildhall) {
			houseNode.append_attribute("guildhall") = true;
		}

		houseNode.append_attribute("townid") = house->townid;
		const bool isOtbm5 = map.getVersion().otbm == MAP_OTBM_5;
		houseNode.append_attribute("size") = isOtbm5 ? calculateOtbm5HouseSize(map, *house) : static_cast<int32_t>(house->size());
		if (isOtbm5) {
			houseNode.append_attribute("clientid") = house->clientid;
			if (house->beds >= 0) {
				houseNode.append_attribute("beds") = house->beds;
			}
		}
	}
	return true;
}

bool MapXMLIO::loadZones(Map& map, const wxFileName& dir) {
	auto paths = normalizeMapFilePaths(dir, map.zonefile);
	pugi::xml_document doc;
	if (!loadXMLFile(doc, paths.first, paths.second)) {
		return false;
	}

	pugi::xml_node node = doc.child("zones");
	if (!node) {
		return false;
	}

	map.zones.clear();
	map.max_assigned_zone_id = 0;
	for (auto zoneNode : node.children("zone")) {
		const std::string name = zoneNode.attribute("name").as_string();
		const unsigned int raw_id = zoneNode.attribute("zoneid").as_uint();
		if (name.empty() || raw_id == 0 || raw_id > 0xFFFF) {
			spdlog::warn("MapXMLIO: Malformed zone data (name: '{}', id: {}), discarding...", name, raw_id);
			continue;
		}
		const uint16_t id = static_cast<uint16_t>(raw_id);
		if (map.findZone(id) != nullptr || map.findZone(name) != nullptr) {
			spdlog::warn("MapXMLIO: Duplicate zone id ({}) or name ('{}'), discarding...", id, name);
			continue;
		}
		map.zones.push_back({ name, id });
		map.max_assigned_zone_id = std::max(map.max_assigned_zone_id, id);
	}
	return true;
}

bool MapXMLIO::saveZones(const Map& map, const wxFileName& dir) {
	auto paths = normalizeMapFilePaths(dir, map.zonefile);

	pugi::xml_document doc;
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";

	pugi::xml_node zoneNodes = doc.append_child("zones");
	for (const auto& zone : map.zones) {
		pugi::xml_node zoneNode = zoneNodes.append_child("zone");
		zoneNode.append_attribute("name") = zone.name.c_str();
		zoneNode.append_attribute("zoneid") = zone.id;
	}
	return saveXMLFile(doc, paths.first, paths.second);
}

bool MapXMLIO::loadWaypoints(Map& map, const wxFileName& dir, bool replace) {
	auto paths = normalizeMapFilePaths(dir, map.waypointfile);
	if (!FileName(wxstr(paths.first)).FileExists()) {
		return false;
	}

	pugi::xml_document doc;
	if (!loadXMLFile(doc, paths.first, paths.second)) {
		return false;
	}
	return loadWaypoints(map, doc, replace);
}

bool MapXMLIO::loadWaypoints(Map& map, pugi::xml_node node, bool replace) {
	if (!node) {
		return false;
	}

	for (auto wpNode : node.children("waypoint")) {
		std::string name = wpNode.attribute("name").as_string();
		Position pos(
			wpNode.attribute("x").as_int(),
			wpNode.attribute("y").as_int(),
			wpNode.attribute("z").as_int()
		);

		if (name.empty() || pos.x == 0 || pos.y == 0) {
			spdlog::warn("MapXMLIO: Malformed waypoint data, discarding...");
			continue;
		}

		map.waypoints.addWaypoint(std::make_unique<Waypoint>(name, pos), replace);
	}
	return true;
}

bool MapXMLIO::loadWaypoints(Map& map, pugi::xml_document& doc, bool replace) {
	return loadWaypoints(map, doc.child("waypoints"), replace);
}

bool MapXMLIO::saveWaypoints(const Map& map, const wxFileName& dir, const CancelCheck& shouldCancel) {
	auto paths = normalizeMapFilePaths(dir, map.waypointfile);

	pugi::xml_document doc;
	if (saveWaypoints(map, doc, shouldCancel)) {
		return saveXMLFile(doc, paths.first, paths.second);
	}
	return false;
}

bool MapXMLIO::saveWaypoints(const Map& map, pugi::xml_document& doc, const CancelCheck& shouldCancel) {
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	if (!decl) {
		return false;
	}
	decl.append_attribute("version") = "1.0";

	pugi::xml_node rootNode = doc.append_child("waypoints");

	size_t processed = 0;
	for (const auto& [name, waypoint] : map.waypoints) {
		if (shouldAbortWriting(shouldCancel, processed)) {
			return false;
		}

		pugi::xml_node wpNode = rootNode.append_child("waypoint");

		wpNode.append_attribute("name") = waypoint->name.c_str();
		wpNode.append_attribute("x") = waypoint->pos.x;
		wpNode.append_attribute("y") = waypoint->pos.y;
		wpNode.append_attribute("z") = waypoint->pos.z;
	}
	return true;
}
