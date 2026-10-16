//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "ui/gui.h" // loadbar

#include "map/map.h"
#include "map/map_converter.h"
#include "map/map_spawn_manager.h"

#include <sstream>
#include <algorithm>
#include <ranges>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <format>
#include <spdlog/spdlog.h>

namespace {
	static std::atomic_uint64_t g_nextMapGeneration { 1 };
}

Map::Map() :
	BaseMap(),
	width(512),
	height(512),
	houses(*this),
	has_changed(false),
	unnamed(false),
	waypoints(*this),
	generation(g_nextMapGeneration.fetch_add(1, std::memory_order_relaxed)) {
	spdlog::info("Map created [Map={}]", static_cast<void*>(this));
	// Earliest version possible
	// Caller is responsible for converting us to proper version
	mapVersion.otbm = MAP_OTBM_1;
	mapVersion.client = OTB_VERSION_NONE;
}

void Map::initializeEmpty() {
	spdlog::info("Map::initializeEmpty [Map={}]", static_cast<void*>(this));
	generation = g_nextMapGeneration.fetch_add(1, std::memory_order_relaxed);
	height = 2048;
	width = 2048;
	spawnIssues.clear();

	static int unnamed_counter = 0;

	std::string sname = std::format("Untitled-{}", ++unnamed_counter);
	name = sname + ".otbm";
	spawnfile = sname + "-spawn.xml";
	monsterfile = sname + "-monster.xml";
	housefile = sname + "-house.xml";
	npcfile = sname + "-npc.xml";
	zonefile = sname + "-zones.xml";
	waypointfile = sname + "-waypoint.xml";
	description = "No map description available.";
	unnamed = true;
	zones.clear();
	zone_index.clear();
	max_assigned_zone_id = 0;

	doChange();
}

Map::~Map() {
	spdlog::info("Map destroying [Map={}]", static_cast<void*>(this));
}

bool Map::open(const std::string& file) {
	if (file == filename) {
		return true; // Do not reopen ourselves!
	}

	generation = g_nextMapGeneration.fetch_add(1, std::memory_order_relaxed);
	tilecount = 0;
	spawnIssues.clear();

	IOMapOTBM maploader(getVersion());

	bool success = maploader.loadMap(*this, wxstr(file));

	mapVersion = maploader.version;

	warnings = maploader.getWarnings();

	if (!success) {
		error = maploader.getError();
		return false;
	}

	has_changed = false;
	change_tracker.markAllDirty();

	wxFileName fn = wxstr(file);
	filename = fn.GetFullPath().mb_str(wxConvUTF8);
	name = fn.GetFullName().mb_str(wxConvUTF8);

	// convert(getReplacementMapClassic(), true);

	return true;
}

bool Map::convert(MapVersion to, bool showdialog) {
	return MapConverter::convert(*this, to, showdialog);
}

bool Map::convert(const ConversionMap& rm, bool showdialog) {
	return MapConverter::convert(*this, rm, showdialog);
}

void Map::cleanInvalidTiles(bool showdialog) {
	MapConverter::cleanInvalidTiles(*this, showdialog);
}

void Map::cleanInvalidZones(bool showdialog) {
	MapConverter::cleanInvalidZones(*this, showdialog);
}

void Map::convertHouseTiles(uint32_t fromId, uint32_t toId) {
	MapConverter::convertHouseTiles(*this, fromId, toId);
}

MapVersion Map::getVersion() const {
	return mapVersion;
}

bool Map::hasChanged() const {
	return has_changed;
}

bool Map::doChange() {
	bool doupdate = !has_changed;
	has_changed = true;
	return doupdate;
}

bool Map::clearChanges() {
	bool doupdate = has_changed;
	has_changed = false;
	return doupdate;
}

bool Map::hasFile() const {
	return filename != "";
}

void Map::setWidth(int new_width) {
	width = std::clamp(new_width, 64, 65000);
}

void Map::setHeight(int new_height) {
	height = std::clamp(new_height, 64, 65000);
}
void Map::setMapDescription(const std::string& new_description) {
	description = new_description;
}

void Map::setHouseFilename(const std::string& new_housefile) {
	housefile = new_housefile;
	unnamed = false;
}

void Map::setSpawnFilename(const std::string& new_spawnfile) {
	spawnfile = new_spawnfile;
	unnamed = false;
}

void Map::setMonsterFilename(const std::string& new_monsterfile) {
	monsterfile = new_monsterfile;
	unnamed = false;
}

void Map::setNpcFilename(const std::string& new_npcfile) {
	npcfile = new_npcfile;
	unnamed = false;
}

void Map::setWaypointFilename(const std::string& new_waypointfile) {
	waypointfile = new_waypointfile;
	unnamed = false;
}

const Map::Zone* Map::findZone(uint16_t id) const {
	const auto it = std::ranges::find(zones, id, &Zone::id);
	return it != zones.end() ? &*it : nullptr;
}

const Map::Zone* Map::findZone(std::string_view name) const {
	const auto it = std::ranges::find(zones, name, &Zone::name);
	return it != zones.end() ? &*it : nullptr;
}

uint16_t Map::addZone(std::string name) {
	if (!supportsZones() || name.empty() || findZone(name)) {
		return 0;
	}
	uint16_t id = std::max<uint16_t>(max_assigned_zone_id + 1, 1);
	// Skip IDs referenced by tiles too: a sidecar may omit a zone the tiles
	// still point at, and handing that ID to a new zone would silently adopt
	// those tiles.
	while (findZone(id) || zone_index.hasZone(id)) {
		if (id == UINT16_MAX) {
			return 0;
		}
		++id;
	}
	max_assigned_zone_id = id;
	zones.push_back({ std::move(name), id });
	doChange();
	return id;
}

bool Map::renameZone(uint16_t id, std::string name) {
	if (!supportsZones()) {
		return false;
	}
	const Zone* existing = findZone(name);
	if (name.empty() || (existing && existing->id != id)) {
		return false;
	}
	const auto it = std::ranges::find(zones, id, &Zone::id);
	if (it == zones.end()) {
		return false;
	}
	it->name = std::move(name);
	doChange();
	return true;
}

void Map::removeZone(uint16_t id) {
	if (!supportsZones()) {
		return;
	}
	std::erase_if(zones, [id](const Zone& zone) { return zone.id == id; });
	const auto& positions = zone_index.getPositions(id);
	for (const Position& pos : positions) {
		if (Tile* tile = getTile(pos)) {
			tile->removeZone(id);
		}
	}
	zone_index.removeZone(id);
	doChange();
}

const std::vector<Position>& Map::getZoneTilePositions(uint16_t id) const noexcept {
	static const std::vector<Position> s_empty;
	return supportsZones() ? zone_index.getPositions(id) : s_empty;
}

const std::vector<ZoneCluster>& Map::getZoneClusters(uint16_t id, bool recompute_if_dirty) const {
	static const std::vector<ZoneCluster> s_empty;
	return supportsZones() ? zone_index.getClusters(id, recompute_if_dirty) : s_empty;
}

void Map::addTileToZone(uint16_t id, const Position& pos) {
	if (!supportsZones()) {
		return;
	}
	zone_index.addTile(id, pos);
}

void Map::removeTileFromZone(uint16_t id, const Position& pos) {
	if (!supportsZones()) {
		return;
	}
	zone_index.removeTile(id, pos);
}

void Map::updateTileZones(const Position& pos, std::span<const uint16_t> old_zones, std::span<const uint16_t> new_zones) {
	if (!supportsZones()) {
		return;
	}
	for (uint16_t old_id : old_zones) {
		if (!std::ranges::binary_search(new_zones, old_id)) {
			zone_index.removeTile(old_id, pos);
		}
	}
	for (uint16_t new_id : new_zones) {
		if (!std::ranges::binary_search(old_zones, new_id)) {
			zone_index.addTile(new_id, pos);
		}
	}
}

void Map::rebuildZoneIndex() {
	if (!supportsZones()) {
		zone_index.clear();
		return;
	}
	zone_index.rebuild(*this);
}

bool Map::addSpawn(Tile* tile) {
	return MapSpawnManager::addSpawn(*this, tile);
}

void Map::removeSpawn(Tile* tile) {
	MapSpawnManager::removeSpawn(*this, tile);
}

SpawnList Map::getSpawnList(Tile* where) {
	return MapSpawnManager::getSpawnList(*this, where);
}
