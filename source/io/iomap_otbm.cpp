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

#include <format>
#include <vector>
#include <filesystem>
#include <optional>
#include <spdlog/spdlog.h>

#include "app/settings.h"
#include "ext/pugixml.hpp"
#include "ui/gui.h"

#include "game/creatures.h"
#include "game/creature.h"
#include "map/map.h"
#include "map/map_region.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "game/town.h"
#include "brushes/wall/wall_brush.h"
#include "item_definitions/core/item_definition_store.h"

#include "io/iomap_otbm.h"
#include "io/map_xml_io.h"
#include "io/otbm/item_serialization_otbm.h"

// New specialized serialization classes
#include "io/otbm/header_serialization_otbm.h"
#include "io/otbm/town_serialization_otbm.h"
#include "io/otbm/waypoint_serialization_otbm.h"
#include "io/otbm/tile_serialization_otbm.h"
#include "io/otbm/fast_otbm_reader.h"
#include "io/compression/gzip_stream.h"
#include "io/compression/gzip_node_file_write_handle.h"

#include <thread>
#include <atomic>
#include <numeric>
#include <unordered_map>
#include <chrono>

namespace {
// Progress stages for saving. The progress bar closes itself when it reaches
// 100%, so tile writing stops below that and the XML phases fill the rest.
constexpr int32_t kTileWriteStagePercent = 85;
constexpr int32_t kFinalizeStagePercent = 88;
// On OTBM 5 the spawn stage builds the monster and the NPC file, each walking
// the whole spawn list, so it is by far the longest auxiliary step and owns a
// wide band of its own.
constexpr int32_t kSpawnStagePercent = 91;
constexpr int32_t kSpawnStageEndPercent = 96;
constexpr int32_t kHouseStagePercent = 97;
constexpr int32_t kWaypointStagePercent = 98;

// Reports a named save stage and tells whether the save should continue
// (the user can abort through the progress dialog). Switching the message to
// "Cancelling..." keeps the dialog responsive while the save unwinds.
bool reportSaveStage(int32_t percent, const wxString& message) {
	g_gui.SetLoadDone(percent, message);
	if (g_gui.WasLoadCancelled()) {
		g_gui.SetLoadText("Cancelling...");
		return false;
	}
	return true;
}

// Cancellation predicate handed to the auxiliary XML writers, which poll it
// while they build their documents: pumping here is what lets an Abort click
// be observed during those phases.
bool shouldAbortSave() {
	return g_gui.PollCancellation();
}
} // namespace

// Item OTBM operations delegated to ItemSerializationOTBM (used by LiveSocket)
std::unique_ptr<Item> Item::Create_OTBM(const IOMap& maphandle, BinaryNode* stream) {
	return ItemSerializationOTBM::createFromStream(maphandle, stream);
}

bool Item::unserializeItemNode_OTBM(const IOMap& maphandle, BinaryNode* node) {
	return ItemSerializationOTBM::unserializeItemNode(maphandle, node, *this);
}

void Item::serializeItemCompact_OTBM(const IOMap& maphandle, NodeFileWriteHandle& stream) const {
	ItemSerializationOTBM::serializeItemCompact(maphandle, stream, *this);
}

bool Item::serializeItemNode_OTBM(const IOMap& maphandle, NodeFileWriteHandle& file) const {
	return ItemSerializationOTBM::serializeItemNode(maphandle, file, *this);
}

bool Container::unserializeItemNode_OTBM(const IOMap& maphandle, BinaryNode* node) {
	return ItemSerializationOTBM::unserializeItemNode(maphandle, node, *this);
}

bool Container::serializeItemNode_OTBM(const IOMap& maphandle, NodeFileWriteHandle& file) const {
	return ItemSerializationOTBM::serializeItemNode(maphandle, file, *this);
}

/* Entry level calls */

namespace {
	// Initial prefix for header probes; grown when the header node does not fit
	// (escaped descriptions can take up to ~128 KiB).
	constexpr size_t kHeaderPeekBytes = 64 * 1024;
	constexpr uint8_t kNodeStart = 0xFE;

	std::filesystem::path toPath(const FileName& filename) {
		return std::filesystem::path(filename.GetFullPath().ToStdWstring());
	}

	bool hasValidOtbmPrefix(std::span<const uint8_t> data) noexcept {
		if (data.size() < 5) {
			return false;
		}
		constexpr uint8_t wildcard[4] = { 0, 0, 0, 0 };
		const bool known_id = std::memcmp(data.data(), "OTBM", 4) == 0 || std::memcmp(data.data(), wildcard, 4) == 0;
		return known_id && data[4] == kNodeStart;
	}

	// Runs parse on a decompressed or raw prefix; nullopt when the file is unreadable or not OTBM.
	template <typename Parse>
	std::optional<bool> parseHeaderPrefix(const FileName& filename, Parse&& parse) {
		const auto path = toPath(filename);
		const bool is_gzip = Compression::isGzipFile(path);
		for (size_t limit = kHeaderPeekBytes;; limit *= 4) {
			const auto bytes = is_gzip
				? Compression::decompressGzipFile(path, limit)
				: Compression::readRawFile(path, limit);
			if (!bytes || !hasValidOtbmPrefix(*bytes)) {
				return std::nullopt;
			}
			MemoryNodeFileReadHandle handle(bytes->data() + 4, bytes->size() - 4);
			const bool ok = parse(handle);
			if (bytes->size() < limit || handle.error_code != FILE_PREMATURE_END) {
				return ok;
			}
		}
	}
}

bool IOMapOTBM::getVersionInfo(const FileName& filename, MapVersion& out_ver) {
	return parseHeaderPrefix(filename, [&](NodeFileReadHandle& f) { return getVersionInfo(&f, out_ver); }).value_or(false);
}

bool IOMapOTBM::getVersionInfo(NodeFileReadHandle* f, MapVersion& out_ver) {
	return HeaderSerializationOTBM::getVersionInfo(*f, out_ver);
}

bool IOMapOTBM::peekStartupInfo(const FileName& identifier, OTBMStartupPeekResult& out_info) {
	out_info = {};
	out_info.map_name = identifier.GetName();

	wxDateTime modified_time;
	if (identifier.GetTimes(nullptr, &modified_time, nullptr)) {
		out_info.modified_time = modified_time;
	}

	out_info.is_compressed = Compression::isGzipFile(toPath(identifier));

	const auto parsed = parseHeaderPrefix(identifier, [&](NodeFileReadHandle& f) { return HeaderSerializationOTBM::peekStartupInfo(f, out_info); });
	if (!parsed) {
		out_info.has_error = true;
		out_info.error_message = "Could not open the file or file magic number not recognized.";
		return false;
	}

	if (!*parsed) {
		out_info.has_error = true;
		out_info.error_message = "Could not read the OTBM header.";
		return false;
	}

	if (out_info.otbm_version == 5) {
		if (out_info.monster_xml_file.empty()) {
			out_info.monster_xml_file = out_info.map_name + "-monster.xml";
		}
		if (out_info.npc_xml_file.empty()) {
			out_info.npc_xml_file = out_info.map_name + "-npc.xml";
		}
	} else {
		if (out_info.spawn_xml_file.empty()) {
			out_info.spawn_xml_file = out_info.map_name + "-spawn.xml";
		}
	}
	if (out_info.house_xml_file.empty()) {
		out_info.house_xml_file = out_info.map_name + "-house.xml";
	}

	return true;
}

bool IOMapOTBM::loadMapFromDisk(Map& map, const FileName& filename) {
	spdlog::debug("Loading OTBM map from disk: {}", filename.GetFullPath().ToStdString());
	const auto path = toPath(filename);

	const bool is_gzip = Compression::isGzipFile(path);
	map.setCompression(is_gzip ? OtbmCompression::Gzip : OtbmCompression::None);
	if (is_gzip) {
		g_gui.SetLoadDone(0, "Decompressing map...");
	}

	const auto buffer = is_gzip ? Compression::decompressGzipFile(path) : Compression::readRawFile(path);
	if (!buffer) {
		spdlog::error("Couldn't read{} map file: {}", is_gzip ? " compressed" : "", filename.GetFullPath().ToStdString());
		return false;
	}

	if (!hasValidOtbmPrefix(*buffer)) {
		spdlog::error("File magic number not recognized");
		return false;
	}

	if (!loadMapFast(map, buffer->data() + 4, buffer->size() - 4)) {
		spdlog::error("Failed to load OTBM map: {}", filename.GetFullPath().ToStdString());
		return false;
	}

	// Read auxiliary files
	auto loadAux = [&](bool (*func)(Map&, const FileName&), const std::string& suffix, const std::string& label, std::string& target) {
		std::string defaultFile = nstr(filename.GetName()) + "-" + suffix + ".xml";
		bool expected = !target.empty();

		if (expected) {
			// Validate/sanitize OTBM-provided target
			auto paths = MapXMLIO::normalizeMapFilePaths(filename, target);
			if (!FileName(wxstr(paths.first)).FileExists()) {
				// File does not exist or invalid, try default
				expected = false;
			}
		}

		if (!expected) {
			auto paths = MapXMLIO::normalizeMapFilePaths(filename, defaultFile);
			if (FileName(wxstr(paths.first)).FileExists()) {
				target = defaultFile;
				expected = true;
			}
		}

		if (expected) {
			// Name the companion file being read; the loader otherwise only
			// reports the OTBM stages and the XML sidecars stay invisible.
			g_gui.SetLoadText(wxstr(std::format("Loading {} ({})...", label, target)));
			if (!func(map, filename)) {
				spdlog::warn("Failed to load {} file: {}", suffix, target);
			}
		} else {
			// No file specified in OTBM and no default file found.
			// Set the default filename for future saves so we don't end up with empty strings.
			target = defaultFile;
		}
	};

	if (version.otbm == MAP_OTBM_5) {
		loadAux(MapXMLIO::loadMonsterSpawns, "monster", "monster spawns", map.monsterfile);
		loadAux(MapXMLIO::loadNpcSpawns, "npc", "npc spawns", map.npcfile);
	} else {
		loadAux(MapXMLIO::loadSpawns, "spawn", "spawns", map.spawnfile);
	}
	loadAux(MapXMLIO::loadHouses, "house", "houses", map.housefile);
	loadAux(MapXMLIO::loadZones, "zones", "zones", map.zonefile);

	// If the map has zone assignments on tiles but no corresponding zone metadata was loaded
	// (e.g. missing, corrupt, or unreferenced sidecar XML), synthesize placeholder definitions
	// so orphan zone IDs are preserved in the palette and re-saved rather than silently lost.
	if (map.supportsZones()) {
		for (uint16_t zid : map.zone_index.getZoneIds()) {
			if (!map.findZone(zid)) {
				map.zones.push_back({ std::format("Zone {}", zid), zid });
				map.max_assigned_zone_id = std::max(map.max_assigned_zone_id, zid);
				spdlog::warn("IOMapOTBM: Synthesized placeholder zone 'Zone {}' (id {}) from indexed tile references", zid, zid);
			}
		}
	}

	// Ensure all auxiliary filenames have defaults so converting between OTBM versions never encounters empty paths
	if (map.spawnfile.empty()) {
		map.spawnfile = nstr(filename.GetName()) + "-spawn.xml";
	}
	if (map.monsterfile.empty()) {
		map.monsterfile = nstr(filename.GetName()) + "-monster.xml";
	}
	if (map.npcfile.empty()) {
		map.npcfile = nstr(filename.GetName()) + "-npc.xml";
	}
	if (map.housefile.empty()) {
		map.housefile = nstr(filename.GetName()) + "-house.xml";
	}

	// Waypoints handling: if OTBM has waypoints, it is the authority and XML is ignored entirely.
	// Only load from companion XML if OTBM has no waypoints.
	if (map.waypoints.empty()) {
		std::string waypointFile = map.waypointfile;
		if (waypointFile.empty()) {
			waypointFile = nstr(filename.GetName()) + "-waypoint.xml";
		}

		auto waypointPaths = MapXMLIO::normalizeMapFilePaths(filename, waypointFile);
		if (FileName(wxstr(waypointPaths.first)).FileExists()) {
			if (map.waypointfile.empty()) {
				map.waypointfile = waypointFile;
			}
			g_gui.SetLoadText(wxstr(std::format("Loading waypoints ({})...", waypointFile)));
			if (MapXMLIO::loadWaypoints(map, filename, false)) {
				spdlog::info("IOMapOTBM: Loaded waypoints from '{}' (total waypoints: {})",
					map.waypointfile, map.waypoints.size());
			}
		} else if (map.waypointfile.empty()) {
			map.waypointfile = waypointFile;
		}
	} else if (map.waypointfile.empty()) {
		map.waypointfile = nstr(filename.GetName()) + "-waypoint.xml";
	}
	map.getChangeTracker().markAllDirty();

	// Everything (OTBM tiles and companion XML) is in; close the bar here so the
	// companion files stay visible while they load.
	g_gui.SetLoadDone(100);
	return true;
}

bool IOMapOTBM::loadMapFast(Map& map, const uint8_t* data, size_t size) {
	if (size < 4) {
		return false;
	}

	FastOTBMStream stream(data, size);

	// Root node
	if (!stream.hasMore() || stream.readByte() != OTBM_NODE_START) {
		spdlog::error("FastOTBM: Could not read root node start");
		return false;
	}

	stream.readByte(); // Skip root type byte (matches HeaderSerializationOTBM::loadMapRoot)

	uint32_t raw_version = 0;
	if (!stream.getU32(raw_version)) {
		return false;
	}
	version.otbm = normalizeOtbmVersion(raw_version);
	if (version.otbm > MAP_OTBM_5) {
		spdlog::warn("Unsupported or damaged map version: {}", static_cast<int>(version.otbm));
	}

	if (!stream.getU16(map.width) || !stream.getU16(map.height)) {
		return false;
	}

	uint32_t majorVersion = 0, minorVersion = 0;
	if (!stream.getU32(majorVersion) || !stream.getU32(minorVersion)) {
		return false;
	}

	if (majorVersion > static_cast<uint32_t>(g_item_definitions.MajorVersion)) {
		spdlog::warn("Newer items.otb major version ({}); continuing with unresolved item preservation", majorVersion);
	}
	version.client = static_cast<OtbVersionID>(minorVersion);
	version.items_major = majorVersion;

	stream.skipRemainingProps();

	// OTBM_MAP_DATA child
	if (!stream.hasMore() || stream.readByte() != OTBM_NODE_START) {
		spdlog::error("FastOTBM: Could not find OTBM_MAP_DATA node");
		return false;
	}

	uint8_t mapDataType = stream.readByte();
	if (mapDataType != OTBM_MAP_DATA) {
		spdlog::error("FastOTBM: Expected OTBM_MAP_DATA, got {}", mapDataType);
		return false;
	}

	// Read header attributes
	if (!HeaderSerializationOTBM::readMapAttributes(map, stream, version.otbm)) {
		return false;
	}
	stream.skipRemainingProps();

	// Data structures for parallel block-partitioned loading
	struct TileAreaJob {
		const uint8_t* node_start = nullptr;
		const uint8_t* node_end = nullptr;
		uint16_t base_x = 0;
		uint16_t base_y = 0;
		uint8_t base_z = 0;
	};

	struct BlockBucket {
		uint16_t block_x = 0;
		uint16_t block_y = 0;
		std::array<size_t, 16> cell_indices {};
		std::vector<TileAreaJob> jobs;
	};

	struct AuxiliaryJob {
		uint8_t type = 0;
		const uint8_t* node_start = nullptr;
		const uint8_t* node_end = nullptr;
	};

	std::unordered_map<uint64_t, size_t> block_index_map;
	std::vector<BlockBucket> block_buckets;
	std::vector<TileAreaJob> unaligned_jobs;
	std::vector<AuxiliaryJob> auxiliary_jobs;
	std::vector<uint64_t> cell_keys;

	auto t_prescan_start = std::chrono::high_resolution_clock::now();
	g_gui.SetLoadDone(5, "Scanning map nodes...");

	// Stream through children of OTBM_MAP_DATA
	while (stream.hasMore()) {
		if (stream.peekByte() == OTBM_NODE_END) {
			stream.readByte(); // consume OTBM_NODE_END
			break;
		}

		if (stream.readByte() != OTBM_NODE_START) {
			continue;
		}

		uint8_t child_type = stream.readByte();
		if (child_type == OTBM_TILE_AREA) {
			const uint8_t* job_start = stream.p;
			uint16_t bx = 0, by = 0;
			uint8_t bz = 0;
			if (!stream.getU16(bx) || !stream.getU16(by) || !stream.getU8(bz)) {
				stream.skipNode();
				continue;
			}

			// Find end of this OTBM_TILE_AREA node
			if (!stream.skipNode()) {
				spdlog::error("FastOTBM: Truncated OTBM_TILE_AREA node encountered");
				return false;
			}
			const uint8_t* job_end = stream.p;

			TileAreaJob job {
				.node_start = job_start,
				.node_end = job_end,
				.base_x = bx,
				.base_y = by,
				.base_z = bz,
			};

			if ((bx % 256 == 0) && (by % 256 == 0)) {
				uint64_t bkey = (static_cast<uint64_t>(bx) << 32) | static_cast<uint64_t>(by);
				auto it = block_index_map.find(bkey);
				if (it == block_index_map.end()) {
					size_t new_idx = block_buckets.size();
					block_index_map[bkey] = new_idx;
					BlockBucket bucket;
					bucket.block_x = bx;
					bucket.block_y = by;
					bucket.jobs.push_back(job);
					block_buckets.push_back(std::move(bucket));

					// Pre-record the 16 cell keys for this 256x256 block
					for (int cy_off = 0; cy_off < 4; ++cy_off) {
						for (int cx_off = 0; cx_off < 4; ++cx_off) {
							cell_keys.push_back(SpatialHashGrid::makeKey(bx + cx_off * 64, by + cy_off * 64));
						}
					}
				} else {
					block_buckets[it->second].jobs.push_back(job);
				}
			} else {
				unaligned_jobs.push_back(job);
			}
		} else if (child_type == OTBM_TOWNS || child_type == OTBM_WAYPOINTS) {
			const uint8_t* aux_start = stream.p;
			if (!stream.skipNode()) {
				spdlog::error("FastOTBM: Truncated auxiliary node encountered");
				return false;
			}
			auxiliary_jobs.push_back(AuxiliaryJob {
				.type = child_type,
				.node_start = aux_start,
				.node_end = stream.p,
			});
		} else {
			if (!stream.skipNode()) {
				spdlog::error("FastOTBM: Truncated unknown node encountered");
				return false;
			}
		}
	}

	auto t_prescan_end = std::chrono::high_resolution_clock::now();
	spdlog::info("FastOTBM: Pre-scan completed in {:.2f} ms ({} blocks, {} unaligned areas)",
		std::chrono::duration<double, std::milli>(t_prescan_end - t_prescan_start).count(),
		block_buckets.size(), unaligned_jobs.size());

	// Pre-allocate spatial hash grid cells
	map.getGrid().preallocateCells(cell_keys);

	// Resolve the 16 cell indices for each block
	for (auto& bucket : block_buckets) {
		for (int cy_off = 0; cy_off < 4; ++cy_off) {
			for (int cx_off = 0; cx_off < 4; ++cx_off) {
				uint64_t k = SpatialHashGrid::makeKey(bucket.block_x + cx_off * 64, bucket.block_y + cy_off * 64);
				bucket.cell_indices[(cy_off << 2) | cx_off] = map.getGrid().findCellIndex(k);
			}
		}
	}

	// Parallel processing of block buckets
	unsigned int n_threads = std::thread::hardware_concurrency();
	if (n_threads == 0) {
		n_threads = 4;
	}

	std::atomic<size_t> next_bucket_idx { 0 };
	std::atomic<size_t> completed_buckets { 0 };
	std::atomic<bool> worker_failed { false };
	std::vector<std::vector<std::pair<uint32_t, Tile*>>> thread_house_tiles(n_threads);
	std::vector<std::vector<std::pair<uint16_t, Position>>> thread_zone_tiles(n_threads);
	std::vector<uint64_t> thread_tile_counts(n_threads, 0);

	g_gui.SetLoadDone(20, "Loading map tiles in parallel...");
	auto t_load_start = std::chrono::high_resolution_clock::now();

	std::vector<std::thread> workers;
	workers.reserve(n_threads);

	for (unsigned int tid = 0; tid < n_threads; ++tid) {
		workers.emplace_back([&, tid]() {
			auto& house_tiles = thread_house_tiles[tid];
			auto& zone_tiles = thread_zone_tiles[tid];
			auto& tile_count = thread_tile_counts[tid];
			try {
				while (true) {
					size_t bidx = next_bucket_idx.fetch_add(1, std::memory_order_relaxed);
					if (bidx >= block_buckets.size() || worker_failed.load(std::memory_order_relaxed)) {
						break;
					}

					auto& bucket = block_buckets[bidx];
					for (const auto& job : bucket.jobs) {
						FastOTBMNode areaNode(OTBM_TILE_AREA, job.node_start, job.node_end);
						TileSerializationOTBM::readTileArea(*this, map, areaNode, &bucket.cell_indices, house_tiles, tile_count, zone_tiles);
					}
					completed_buckets.fetch_add(1, std::memory_order_relaxed);
				}
			} catch (const std::exception& e) {
				spdlog::error("FastOTBM: Worker {} failed with exception: {}", tid, e.what());
				worker_failed.store(true, std::memory_order_relaxed);
				completed_buckets.store(block_buckets.size(), std::memory_order_relaxed);
			}
		});
	}

	// Smoothly update loading bar on main thread while workers are running
	const size_t total_buckets = block_buckets.size();
	while (true) {
		size_t done = completed_buckets.load(std::memory_order_relaxed);
		if (done >= total_buckets) {
			break;
		}
		int pct = 20 + static_cast<int>(70.0 * done / (total_buckets > 0 ? total_buckets : 1));
		g_gui.SetLoadDone(pct, wxString::Format("Loading map tiles (%zu/%zu blocks)...", done, total_buckets));
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	for (auto& w : workers) {
		if (w.joinable()) {
			w.join();
		}
	}

	if (worker_failed.load(std::memory_order_relaxed)) {
		spdlog::error("FastOTBM: Map tile loading failed due to worker error");
		return false;
	}

	// Process any unaligned jobs (single-threaded fallback)
	if (!unaligned_jobs.empty()) {
		auto& house_tiles = thread_house_tiles[0];
		auto& zone_tiles = thread_zone_tiles[0];
		auto& tile_count = thread_tile_counts[0];
		for (const auto& job : unaligned_jobs) {
			FastOTBMNode areaNode(OTBM_TILE_AREA, job.node_start, job.node_end);
			TileSerializationOTBM::readTileArea(*this, map, areaNode, nullptr, house_tiles, tile_count, zone_tiles);
		}
	}

	auto t_load_end = std::chrono::high_resolution_clock::now();
	spdlog::info("FastOTBM: Parallel tile loading completed in {:.2f} ms",
		std::chrono::duration<double, std::milli>(t_load_end - t_load_start).count());

	// Update tilecount directly from thread counters (O(1) instead of traversing all cells/floors)
	// Establish base count before auxiliary nodes (towns) create additional tiles through getOrCreateTile.
	uint64_t tile_count = 0;
	for (uint64_t tc : thread_tile_counts) {
		tile_count += tc;
	}
	map.tilecount = tile_count;

	// Process auxiliary nodes (towns, waypoints) sequentially now that all tiles are created
	for (const auto& aux : auxiliary_jobs) {
		if (aux.type == OTBM_TOWNS) {
			FastOTBMNode townsNode(OTBM_TOWNS, aux.node_start, aux.node_end);
			TownSerializationOTBM::readTowns(map, townsNode);
		} else if (aux.type == OTBM_WAYPOINTS) {
			FastOTBMNode waypointsNode(OTBM_WAYPOINTS, aux.node_start, aux.node_end);
			WaypointSerializationOTBM::readWaypoints(map, waypointsNode);
		}
	}

	g_gui.SetLoadDone(95, "Finalizing houses and map data...");

	// Register house tiles sequentially into map.houses
	for (const auto& hlist : thread_house_tiles) {
		for (const auto& [house_id, tile] : hlist) {
			House* house = map.houses.getHouse(house_id);
			if (!house) {
				auto new_house = std::make_unique<House>(map);
				house = new_house.get();
				new_house->setID(house_id);
				map.houses.addHouse(std::move(new_house));
			}
			house->addTile(tile);
		}
	}

	// Register zone tiles in bulk into map.zone_index
	map.zone_index.clear();
	std::unordered_map<uint16_t, std::vector<Position>> aggregated_zones;
	for (const auto& zlist : thread_zone_tiles) {
		for (const auto& [zone_id, pos] : zlist) {
			aggregated_zones[zone_id].push_back(pos);
		}
	}
	for (const auto& [zone_id, positions] : aggregated_zones) {
		map.zone_index.addTilesBulk(zone_id, positions);
	}

	// Keep the bar open (and the message current) for the companion files that
	// loadMapFromDisk reads next; it closes it once they are done.
	g_gui.SetLoadDone(96);
	return true;
}

bool IOMapOTBM::loadMap(Map& map, const FileName& filename) {
	return loadMapFromDisk(map, filename);
}

bool IOMapOTBM::saveMapToDisk(Map& map, const FileName& identifier) {
	const std::string magic = g_settings.getInteger(Config::SAVE_WITH_OTB_MAGIC_NUMBER) ? "OTBM" : std::string(4, '\0');

	// Zones created in the editor on a map that had none still need a file for the header to reference
	if (map.getVersion().otbm == MAP_OTBM_5 && !map.zones.empty()) {
		if (map.zonefile.empty()) {
			map.zonefile = nstr(identifier.GetName() + "-zones.xml");
		} else {
			const std::filesystem::path zone_p(map.zonefile);
			if (zone_p.is_absolute()) {
				std::error_code ec;
				const auto rel = std::filesystem::relative(zone_p, toPath(identifier).parent_path(), ec);
				if (!ec && !rel.empty() && !rel.is_absolute() && rel.generic_string().find("..") == std::string::npos) {
					map.zonefile = rel.generic_string();
				} else {
					map.zonefile = zone_p.filename().generic_string();
				}
			} else if (zone_p.is_relative() && map.zonefile.find("..") == std::string::npos) {
				map.zonefile = zone_p.generic_string();
			}
		}
	}

	const bool is_compressed = (map.getCompression() == OtbmCompression::Gzip);
	std::unique_ptr<NodeFileWriteHandle> f;
	if (is_compressed) {
		f = std::make_unique<GzipNodeFileWriteHandle>(toPath(identifier), magic);
	} else {
		f = std::make_unique<DiskNodeFileWriteHandle>(std::string(identifier.GetFullPath().utf8_str()), magic);
	}

	if (!f->isOk()) {
		spdlog::error("Can not open file {} for writing", identifier.GetFullPath().ToStdString());
		return false;
	}

	g_gui.SetLoadText(wxstr(std::format("Writing tiles into {}...", nstr(identifier.GetFullName()))));
	if (!saveMap(map, *f)) {
		return false;
	}

	if (!reportSaveStage(kFinalizeStagePercent, wxstr(std::format("Finishing file ({})...", nstr(identifier.GetFullName()))))) {
		return false;
	}

	if (is_compressed) {
		f->close();
		if (!f->isOk()) {
			spdlog::error("Failed to write compressed map {}: {}", identifier.GetFullPath().ToStdString(), f->getErrorMessage());
			return false;
		}
	}

	// Auxiliary companion files: name them in the stages so the dialog says
	// which files are being written, not just which kind of data.
	const std::string map_name = nstr(identifier.GetName());
	const bool otbm5 = map.getVersion().otbm == MAP_OTBM_5;
	auto aux_name = [&map_name](const std::string& configured, std::string_view suffix) {
		return configured.empty() ? map_name + std::string(suffix) : configured;
	};

	const std::string spawn_files = otbm5
		? std::format("{}, {}", aux_name(map.getMonsterFilename(), "-monster.xml"), aux_name(map.getNpcFilename(), "-npc.xml"))
		: aux_name(map.getSpawnFilename(), "-spawn.xml");

	if (!reportSaveStage(kSpawnStagePercent, wxstr(std::format("Saving spawns ({})...", spawn_files)))) {
		return false;
	}
	// The spawn stage is the long one: it rescans the area of every spawn while
	// it builds both files, so it reports which file it is on and how far it is.
	int32_t last_spawn_percent = -1;
	const auto on_spawn_progress = [&last_spawn_percent](const std::string& file, size_t done, size_t total) {
		const int32_t percent = total == 0
			? kSpawnStageEndPercent
			: kSpawnStagePercent + static_cast<int32_t>(static_cast<int64_t>(kSpawnStageEndPercent - kSpawnStagePercent) * static_cast<int64_t>(done) / static_cast<int64_t>(total));
		if (percent == last_spawn_percent) {
			// The gauge moves in 1% steps; repainting in between shows nothing.
			return;
		}
		last_spawn_percent = percent;
		g_gui.SetLoadDone(percent, wxstr(std::format("Saving spawns ({})...", file)));
	};

	if (!MapXMLIO::saveSpawns(map, identifier, shouldAbortSave, on_spawn_progress)) {
		spdlog::error("Failed to save spawns!");
		return false;
	}

	if (!reportSaveStage(kHouseStagePercent, wxstr(std::format("Saving houses ({})...", aux_name(map.getHouseFilename(), "-house.xml"))))) {
		return false;
	}
	if (!MapXMLIO::saveHouses(map, identifier, shouldAbortSave)) {
		spdlog::error("IOMapOTBM::saveMapToDisk: Failed to save houses");
		return false;
	}

	const auto waypointFormat = static_cast<Config::WaypointStorageFormat>(
		g_settings.getInteger(Config::WAYPOINT_STORAGE_FORMAT)
	);

	if (map.getVersion().otbm == MAP_OTBM_5 && !map.zones.empty()) {
		if (map.zonefile.empty()) {
			map.zonefile = identifier.GetName() + "-zones.xml";
			spdlog::info("Auto-created zone file: {}", map.zonefile);
		}
		g_gui.SetLoadText(wxstr(std::format("Saving zones ({})...", map.zonefile)));
		if (!MapXMLIO::saveZones(map, identifier)) {
			spdlog::error("IOMapOTBM::saveMapToDisk: Failed to save zones");
			return false;
		}
		// Waypoints for OTBM 5 live in the OTBM WAYPOINTS node (written in saveMap
		// per waypointFormat); only fall through for the optional XML companion.
	}

	if (waypointFormat == Config::WaypointStorageFormat::XML || waypointFormat == Config::WaypointStorageFormat::BOTH) {
		if (!reportSaveStage(kWaypointStagePercent, wxstr(std::format("Saving waypoints ({})...", aux_name(map.getWaypointFilename(), "-waypoint.xml"))))) {
			return false;
		}
		if (map.waypoints.size() > 0) {
			if (map.waypointfile.empty()) {
				map.waypointfile = identifier.GetName() + "-waypoint.xml";
				spdlog::info("Auto-created waypoint file: {}", map.waypointfile);
			}
			if (!MapXMLIO::saveWaypoints(map, identifier, shouldAbortSave)) {
				spdlog::error("IOMapOTBM::saveMapToDisk: Failed to save waypoints");
				return false;
			}
		} else if (!map.waypointfile.empty()) {
			if (!MapXMLIO::saveWaypoints(map, identifier, shouldAbortSave)) {
				spdlog::error("IOMapOTBM::saveMapToDisk: Failed to save waypoints");
				return false;
			}
		}
	}

	// An Abort clicked while the last files were written must still discard the
	// save instead of accepting it and marking the map as clean.
	if (shouldAbortSave()) {
		return false;
	}

	return true;
}

bool IOMapOTBM::saveMap(Map& map, const FileName& identifier) {
	return saveMapToDisk(map, identifier);
}

bool IOMapOTBM::saveMap(Map& map, NodeFileWriteHandle& f) {
	const MapVersion mapVersion = map.getVersion();

	f.addNode(0);
	{
		f.addU32(mapVersion.otbm);
		f.addU16(map.width);
		f.addU16(map.height);
		f.addU32(g_item_definitions.MajorVersion);
		f.addU32(g_item_definitions.MinorVersion);

		f.addNode(OTBM_MAP_DATA);
		{
			f.addU8(OTBM_ATTR_DESCRIPTION);
			f.addString(std::format("Saved with {} {}", __RME_APPLICATION_NAME__, __RME_VERSION__));

			f.addU8(OTBM_ATTR_DESCRIPTION);
			f.addString(map.description);

			auto addExtFile = [&](uint8_t attr, const std::string& path_str) {
				if (path_str.empty()) {
					return;
				}
				const std::filesystem::path path(path_str);
				std::string serialized_path;
				if (path.is_relative() && path_str.find("..") == std::string::npos) {
					serialized_path = path.generic_string();
				} else {
					serialized_path = path.filename().generic_string();
				}
				f.addU8(attr);
				f.addString(serialized_path.empty() ? path_str : serialized_path);
			};

			if (mapVersion.otbm == MAP_OTBM_5) {
				addExtFile(OTBM_ATTR_EXT_SPAWN_FILE, map.monsterfile);
				addExtFile(OTBM_ATTR_EXT_SPAWN_NPC_FILE, map.npcfile);
			} else {
				addExtFile(OTBM_ATTR_EXT_SPAWN_FILE, map.spawnfile);
			}
			addExtFile(OTBM_ATTR_EXT_HOUSE_FILE, map.housefile);
			if (mapVersion.otbm == MAP_OTBM_5 && !map.zones.empty()) {
				const std::filesystem::path zone_p(map.zonefile);
				if (zone_p.is_absolute() && !map.filename.empty()) {
					std::error_code ec;
					const auto rel = std::filesystem::relative(zone_p, std::filesystem::path(map.filename).parent_path(), ec);
					if (!ec && !rel.empty() && !rel.is_absolute() && rel.generic_string().find("..") == std::string::npos) {
						map.zonefile = rel.generic_string();
					} else {
						map.zonefile = zone_p.filename().generic_string();
					}
				} else if (zone_p.is_relative() && map.zonefile.find("..") == std::string::npos) {
					map.zonefile = zone_p.generic_string();
				}
				addExtFile(OTBM_ATTR_EXT_ZONE_FILE, map.zonefile);
			}

			if (!writeTileData(map, f)) {
				return false;
			}
			writeTowns(map, f);

			const auto waypointFormat = static_cast<Config::WaypointStorageFormat>(
				g_settings.getInteger(Config::WAYPOINT_STORAGE_FORMAT)
			);

			if (waypointFormat == Config::WaypointStorageFormat::OTBM || waypointFormat == Config::WaypointStorageFormat::BOTH) {
				writeWaypoints(map, f, mapVersion);
			} else if (mapVersion.otbm == MAP_OTBM_5) {
				// OTBM 5 loaders require the OTBM_WAYPOINTS node to be present even if empty
				f.addNode(OTBM_WAYPOINTS);
				f.endNode();
			}
		}
		f.endNode();
	}
	f.endNode();

	return true;
}

bool IOMapOTBM::writeTileData(const Map& map, NodeFileWriteHandle& f) {
	// Progress reported by the tile writer fills the tile stage of the bar;
	// the callback returns false to abort a cancelled save, and reports the
	// cancellation to the dialog before unwinding.
	return TileSerializationOTBM::writeTileData(*this, map, f, [](int tilesWritten) {
		g_gui.SetLoadDone(tilesWritten * kTileWriteStagePercent / 100);
		if (g_gui.WasLoadCancelled()) {
			g_gui.SetLoadText("Cancelling...");
			return false;
		}
		return true;
	});
}

void IOMapOTBM::writeTowns(const Map& map, NodeFileWriteHandle& f) {
	TownSerializationOTBM::writeTowns(map, f);
}

IOMapOTBM::WriteResult IOMapOTBM::writeWaypoints(const Map& map, NodeFileWriteHandle& f, MapVersion mapVersion) {
	return WaypointSerializationOTBM::writeWaypoints(map, f, mapVersion);
}
