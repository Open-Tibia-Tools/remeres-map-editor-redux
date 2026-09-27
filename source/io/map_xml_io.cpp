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
#include <format>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <ranges>
#include <vector>

namespace {

bool g_spread_randomized_spawns = false;

struct SpawnTieRoll {
	uint32_t state;

	explicit SpawnTieRoll(uint32_t seed) :
		state(seed == 0 ? 1u : seed) { }

	int next() {
		state = state * 1103515245u + 12345u;
		return static_cast<int>((state >> 16) % 100u);
	}
};

struct SpawnTileProbe {
	bool login_possible = false;
	bool login_clean = false;
	bool expansion_ok = false;
};

int signedSpawnSearchDistance(int radius, int already_placed) {
	int max_radius = radius < 0 ? 1 : radius;
	if (max_radius > 10) {
		max_radius = 10;
	}
	if (already_placed == 0) {
		return std::min(max_radius, 1);
	}
	return -max_radius;
}

SpawnTileProbe probeSpawnTile(const Map& map, const Position& pos, bool place_in_pz, uint32_t home_house_id) {
	const Tile* tile = map.getTile(pos);
	if (tile == nullptr || tile->ground == nullptr) {
		return {};
	}

	const uint32_t house_id = tile->getHouseID();
	if (home_house_id == 0) {
		if (house_id != 0) {
			return {};
		}
	} else if (house_id != home_house_id) {
		return {};
	}

	if (place_in_pz) {
		if (!tile->isPZ()) {
			return {};
		}
	} else if (tile->isPZ()) {
		return {};
	}

	SpawnTileProbe probe;
	probe.expansion_ok = true;
	probe.login_possible = true;

	bool login_bad = false;
	if (tile->creature) {
		probe.login_possible = false;
	}

	auto consider = [&](const Item* item) {
		if (item == nullptr || !item->isBlocking()) {
			return;
		}
		if (!item->isMoveable()) {
			probe.expansion_ok = false;
			probe.login_possible = false;
		} else {
			login_bad = true;
		}
	};
	consider(tile->ground.get());
	for (const auto& item : tile->items) {
		consider(item.get());
	}

	probe.login_clean = probe.login_possible && !login_bad;
	return probe;
}

std::optional<Position> searchSpawnField(const Map& map, const Position& center, int signed_distance, bool place_in_pz, uint32_t home_house_id, SpawnTieRoll& rolls) {
	const bool minimize = signed_distance >= 0;
	const int distance = std::min(std::abs(signed_distance), 30);
	if (distance == 0) {
		const SpawnTileProbe probe = probeSpawnTile(map, center, place_in_pz, home_house_id);
		if (probe.login_possible) {
			return center;
		}
		return std::nullopt;
	}

	const int grid = 2 * distance + 1;
	std::vector<int> phases(static_cast<size_t>(grid) * static_cast<size_t>(grid), std::numeric_limits<int>::max());
	const auto idx = [distance, grid](int ox, int oy) {
		return static_cast<size_t>(oy + distance) * static_cast<size_t>(grid) + static_cast<size_t>(ox + distance);
	};
	phases[idx(0, 0)] = 0;

	std::optional<Position> best_pos;
	int best_tie = -1;
	int expansion_phase = 0;

	for (;;) {
		bool found = false;
		bool expanded = false;

		for (int oy = -distance; oy <= distance; ++oy) {
			for (int ox = -distance; ox <= distance; ++ox) {
				if (phases[idx(ox, oy)] != expansion_phase) {
					continue;
				}

				const Position pos(center.x + ox, center.y + oy, center.z);
				const SpawnTileProbe probe = probeSpawnTile(map, pos, place_in_pz, home_house_id);

				if (probe.expansion_ok || expansion_phase == 0) {
					for (int ny = -1; ny <= 1; ++ny) {
						for (int nx = -1; nx <= 1; ++nx) {
							if (nx == 0 && ny == 0) {
								continue;
							}
							const int nox = ox + nx;
							const int noy = oy + ny;
							if (nox < -distance || nox > distance || noy < -distance || noy > distance) {
								continue;
							}
							const int step = std::abs(nox - ox) + std::abs(noy - oy);
							const size_t neighbor = idx(nox, noy);
							if (phases[neighbor] > expansion_phase + step) {
								phases[neighbor] = expansion_phase + step;
							}
						}
					}
					expanded = true;
				}

				if (probe.login_possible) {
					const int tie = rolls.next() + (probe.login_clean ? 100 : 0);
					if (tie > best_tie) {
						best_tie = tie;
						best_pos = pos;
					}
					found = true;
				}
			}
		}

		if ((found && minimize) || !expanded) {
			break;
		}
		++expansion_phase;
	}

	return best_pos;
}

} // namespace

void MapXMLIO::setSpreadRandomizedSpawns(bool enabled) {
	g_spread_randomized_spawns = enabled;
}

bool MapXMLIO::spreadRandomizedSpawns() {
	return g_spread_randomized_spawns;
}

std::pair<std::string, std::string> MapXMLIO::normalizeMapFilePaths(const wxFileName& dir, const std::string& filename) {
	std::string utf8_path = (const char*)(dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME).mb_str(wxConvUTF8));
	utf8_path += filename;

	std::string encoded_path = (const char*)(dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME).mb_str(wxConvLocal));
	encoded_path += filename;

	return { utf8_path, encoded_path };
}

bool MapXMLIO::loadSpawns(Map& map, const wxFileName& dir) {
	auto paths = normalizeMapFilePaths(dir, map.spawnfile);
	if (!FileName(wxstr(paths.first)).FileExists()) {
		return false;
	}

	pugi::xml_document doc;
	if (!doc.load_file(paths.second.c_str())) {
		return false;
	}
	return loadSpawns(map, doc);
}

bool MapXMLIO::loadSpawns(Map& map, pugi::xml_document& doc) {
	pugi::xml_node node = doc.child("spawns");
	if (!node) {
		return false;
	}

	for (auto spawnNode : node.children("spawn")) {
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
		// Radius 0 is a center-only zone. A missing value parses as 0.
		if (radius < 0) {
			radius = 0;
		}

		Tile* tile = map.getTile(spawnPosition);
		if (tile && tile->spawn) {
			spdlog::warn("MapXMLIO: Duplicate spawn at {}:{}:{}", tile->getX(), tile->getY(), tile->getZ());
			continue;
		}

		if (!tile) {
			tile = map.createTile(spawnPosition.x, spawnPosition.y, spawnPosition.z);
		}

		if (!tile) {
			spdlog::warn("MapXMLIO: Failed to create tile at {}:{}:{}", spawnPosition.x, spawnPosition.y, spawnPosition.z);
			continue;
		}

		tile->spawn = std::make_unique<Spawn>(radius);
		map.addSpawn(tile);

		const bool place_in_pz = tile->isPZ();
		const uint32_t home_house_id = tile->getHouseID();
		const uint32_t tie_seed = static_cast<uint32_t>(spawnPosition.x) * 73856093u ^ static_cast<uint32_t>(spawnPosition.y) * 19349663u ^ static_cast<uint32_t>(spawnPosition.z) * 83492791u;
		SpawnTieRoll tie_roll(tie_seed);
		std::unordered_map<std::string, int> placed_by_name;

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

			const int file_x = xAttr.as_int();
			const int file_y = yAttr.as_int();
			creaturePosition.x += file_x;
			creaturePosition.y += file_y;

			radius = std::clamp<int32_t>(
				std::max({ radius, std::abs(creaturePosition.x - spawnPosition.x), std::abs(creaturePosition.y - spawnPosition.y) }),
				0,
				g_settings.getInteger(Config::MAX_SPAWN_RADIUS)
			);
			tile->spawn->setSize(radius);

			if (g_spread_randomized_spawns) {
				const int already_placed = isNpc ? 0 : placed_by_name[name];
				const int signed_distance = signedSpawnSearchDistance(radius, already_placed);
				const std::optional<Position> found = searchSpawnField(map, spawnPosition, signed_distance, place_in_pz, home_house_id, tie_roll);
				if (!found) {
					spdlog::warn("MapXMLIO: No free tile for '{}' in spawn {}:{}:{}", name, spawnPosition.x, spawnPosition.y, spawnPosition.z);
					continue;
				}
				creaturePosition = *found;
			}

			Tile* creatureTile = (creaturePosition == spawnPosition) ? tile : map.getTile(creaturePosition);

			if (!creatureTile) {
				spdlog::warn("MapXMLIO: Creature '{}' at invalid position {}:{}:{}", name, creaturePosition.x, creaturePosition.y, creaturePosition.z);
				continue;
			}

			if (creatureTile->creature) {
				spdlog::warn("MapXMLIO: Duplicate creature '{}' at {}:{}:{}", name, creaturePosition.x, creaturePosition.y, creaturePosition.z);
				continue;
			}

			CreatureType* type = g_creatures[name];
			if (!type) {
				type = g_creatures.addMissingCreatureType(name, isNpc);
			}

			creatureTile->creature = std::make_unique<Creature>(type);
			creatureTile->creature->setDirection(direction);
			creatureTile->creature->setSpawnTime(spawntime);
			if (g_spread_randomized_spawns) {
				creatureTile->creature->setSpawnFileOffset(file_x, file_y);
				if (!isNpc) {
					placed_by_name[name] += 1;
				}
			}

			if (creatureTile->getLocation()->getSpawnCount() == 0) {
				if (!creatureTile->spawn) {
					creatureTile->spawn = std::make_unique<Spawn>(5);
					map.addSpawn(creatureTile);
				}
			}
		}
	}
	return true;
}

bool MapXMLIO::saveSpawns(const Map& map, const wxFileName& dir) {
	auto paths = normalizeMapFilePaths(dir, map.spawnfile);

	pugi::xml_document doc;
	if (saveSpawns(map, doc)) {
		return doc.save_file(paths.second.c_str(), "\t", pugi::format_default, pugi::encoding_utf8);
	}
	return false;
}

bool MapXMLIO::saveSpawns(const Map& map, pugi::xml_document& doc) {
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	if (!decl) {
		return false;
	}
	decl.append_attribute("version") = "1.0";

	pugi::xml_node rootNode = doc.append_child("spawns");
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

	for (const auto& spawnPos : map.spawns) {
		const Tile* tile = map.getTile(spawnPos);
		if (!tile) {
			continue;
		}
		Spawn* spawn = tile->spawn.get();
		if (!spawn) {
			continue;
		}

		Position spawnPosition = spawnPos;
		pugi::xml_node spawnNode = rootNode.append_child("spawn");
		spawnNode.append_attribute("centerx") = spawnPosition.x;
		spawnNode.append_attribute("centery") = spawnPosition.y;
		spawnNode.append_attribute("centerz") = spawnPosition.z;

		int32_t radius = spawn->getSize();
		spawnNode.append_attribute("radius") = radius;

		for (int32_t y = -radius; y <= radius; ++y) {
			for (int32_t x = -radius; x <= radius; ++x) {
				const Tile* creatureTile = map.getTile(spawnPosition + Position(x, y, 0));
				if (creatureTile && creatureTile->creature && !creatureTile->creature->isSaved()) {
					Creature* creature = creatureTile->creature.get();
					pugi::xml_node creatureNode = spawnNode.append_child(creature->isNpc() ? "npc" : "monster");

					const int out_x = creature->hasSpawnFileOffset() ? creature->getSpawnFileOffsetX() : x;
					const int out_y = creature->hasSpawnFileOffset() ? creature->getSpawnFileOffsetY() : y;
					creatureNode.append_attribute("name") = creature->getName().c_str();
					creatureNode.append_attribute("x") = out_x;
					creatureNode.append_attribute("y") = out_y;
					creatureNode.append_attribute("spawntime") = creature->getSpawnTime();

					if (creature->getDirection() != NORTH) {
						creatureNode.append_attribute("direction") = static_cast<int>(creature->getDirection());
					}

					creature->save();
					creatureList.push_back(creature);
				}
			}
		}
	}

	return true;
}

bool MapXMLIO::loadHouses(Map& map, const wxFileName& dir) {
	auto paths = normalizeMapFilePaths(dir, map.housefile);
	if (!FileName(wxstr(paths.first)).FileExists()) {
		return false;
	}

	pugi::xml_document doc;
	if (!doc.load_file(paths.second.c_str())) {
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

bool MapXMLIO::saveHouses(const Map& map, const wxFileName& dir) {
	auto paths = normalizeMapFilePaths(dir, map.housefile);

	pugi::xml_document doc;
	if (saveHouses(map, doc)) {
		return doc.save_file(paths.second.c_str(), "\t", pugi::format_default, pugi::encoding_utf8);
	}
	return false;
}

bool MapXMLIO::saveHouses(const Map& map, pugi::xml_document& doc) {
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	if (!decl) {
		return false;
	}
	decl.append_attribute("version") = "1.0";

	pugi::xml_node houseNodes = doc.append_child("houses");
	for (const auto& [id, housePtr] : map.houses) {
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
		houseNode.append_attribute("size") = static_cast<int32_t>(house->size());
	}
	return true;
}

bool MapXMLIO::loadWaypoints(Map& map, const wxFileName& dir, bool replace) {
	auto paths = normalizeMapFilePaths(dir, map.waypointfile);
	if (!FileName(wxstr(paths.first)).FileExists()) {
		return false;
	}

	pugi::xml_document doc;
	if (!doc.load_file(paths.second.c_str())) {
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

bool MapXMLIO::saveWaypoints(const Map& map, const wxFileName& dir) {
	auto paths = normalizeMapFilePaths(dir, map.waypointfile);

	pugi::xml_document doc;
	if (saveWaypoints(map, doc)) {
		return doc.save_file(paths.second.c_str(), "\t", pugi::format_default, pugi::encoding_utf8);
	}
	return false;
}

bool MapXMLIO::saveWaypoints(const Map& map, pugi::xml_document& doc) {
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	if (!decl) {
		return false;
	}
	decl.append_attribute("version") = "1.0";

	pugi::xml_node rootNode = doc.append_child("waypoints");

	for (const auto& [name, waypoint] : map.waypoints) {
		pugi::xml_node wpNode = rootNode.append_child("waypoint");

		wpNode.append_attribute("name") = waypoint->name.c_str();
		wpNode.append_attribute("x") = waypoint->pos.x;
		wpNode.append_attribute("y") = waypoint->pos.y;
		wpNode.append_attribute("z") = waypoint->pos.z;
	}
	return true;
}
