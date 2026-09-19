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

#include <wx/wfstream.h>
#include <wx/mstream.h>
#include <wx/datstrm.h>

#include <format>
#include <fstream>
#include <vector>
#include <filesystem>
#include <string_view>
#include <spdlog/spdlog.h>

#include "app/settings.h"
#include "ext/pugixml.hpp"
#include "ui/gui.h"
#include "ui/dialog_util.h"

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

#include <thread>
#include <atomic>
#include <numeric>
#include <unordered_map>
#include <chrono>

// Item OTBM serialization operations delegated to ItemSerializationOTBM
void Item::serializeItemCompact_OTBM(const IOMap& maphandle, NodeFileWriteHandle& stream) const {
	ItemSerializationOTBM::serializeItemCompact(maphandle, stream, *this);
}

bool Item::serializeItemNode_OTBM(const IOMap& maphandle, NodeFileWriteHandle& file) const {
	return ItemSerializationOTBM::serializeItemNode(maphandle, file, *this);
}

bool Container::serializeItemNode_OTBM(const IOMap& maphandle, NodeFileWriteHandle& file) const {
	return ItemSerializationOTBM::serializeItemNode(maphandle, file, *this);
}

/* Entry level calls */

bool IOMapOTBM::getVersionInfo(const FileName& filename, MapVersion& out_ver) {
	return HeaderSerializationOTBM::getVersionInfo(filename, out_ver);
}

bool IOMapOTBM::peekStartupInfo(const FileName& identifier, OTBMStartupPeekResult& out_info) {
	return HeaderSerializationOTBM::peekStartupInfo(identifier, out_info);
}

bool IOMapOTBM::loadMapFromDisk(Map& map, const FileName& filename) {
	spdlog::debug("Loading OTBM map from disk: {}", filename.GetFullPath().ToStdString());
	std::filesystem::path path(nstr(filename.GetFullPath()));

	std::error_code ec;
	const auto size = std::filesystem::file_size(path, ec);
	if (ec) {
		spdlog::error("Couldn't get file size: {} ({})", path.string(), ec.message());
		return false;
	}

	if (size < 4) {
		spdlog::error("File is too short to be an OTBM file.");
		return false;
	}

	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		spdlog::error("Couldn't open file for reading: {}", filename.GetFullPath().ToStdString());
		return false;
	}

	std::vector<uint8_t> buffer(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size))) {
		spdlog::error("Failed to read file.");
		return false;
	}

	std::string_view magic(reinterpret_cast<const char*>(buffer.data()), 4);
	if (magic != "OTBM" && magic != std::string_view("\0\0\0\0", 4)) {
		spdlog::error("File magic number not recognized");
		return false;
	}

	if (!loadMapFast(map, buffer.data() + 4, size - 4)) {
		spdlog::error("Failed to load OTBM map: {}", filename.GetFullPath().ToStdString());
		return false;
	}

	// Read auxiliary files
	auto loadAux = [&](bool (*func)(Map&, const FileName&), const std::string& suffix, std::string& target) {
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
			if (!func(map, filename)) {
				spdlog::warn("Failed to load {} file: {}", suffix, target);
			}
		} else {
			// No file specified in OTBM and no default file found.
			// Set the default filename for future saves so we don't end up with empty strings.
			target = defaultFile;
		}
	};

	loadAux(MapXMLIO::loadSpawns, "spawn", map.spawnfile);
	loadAux(MapXMLIO::loadHouses, "house", map.housefile);

	// Waypoints handling
	if (map.waypoints.size() > 0) {
		// Case 1: OTBM has waypoints
		std::string waypointFile = map.waypointfile;

		// If no external file linked, check the default one
		if (waypointFile.empty()) {
			waypointFile = nstr(filename.GetName()) + "-waypoint.xml";
		}

		auto paths = MapXMLIO::normalizeMapFilePaths(filename, waypointFile);
		if (FileName(wxstr(paths.first)).FileExists()) {
			// Case 3: Both exist - Merge and warn
			// Ensure map knows about the file
			if (map.waypointfile.empty()) {
				map.waypointfile = waypointFile;
			}

			// Load from XML to merge with existing OTBM waypoints
			// Pass false to NOT replace existing OTBM waypoints (OTBM takes precedence)
			size_t beforeCount = map.waypoints.size();
			if (MapXMLIO::loadWaypoints(map, filename, false)) {
				size_t afterCount = map.waypoints.size();
				if (afterCount > beforeCount) {
					DialogUtil::PopupDialog(g_gui.root, "Warning", "Waypoints detected in both OTBM and external XML file.\n"
																   "They have been merged.\n"
																   "Note: OTBM waypoints took precedence over XML duplicates.\n"
																   "\n"
																   "Future saves will ONLY use the XML file.",
											wxOK | wxICON_WARNING);
				}
			}
		} else {
			// Case 2: OTBM has waypoints, XML does not - Migrate
			// Set the default filename if not set
			if (map.waypointfile.empty()) {
				map.waypointfile = waypointFile;
			}

			DialogUtil::PopupDialog(g_gui.root, "Waypoint Migration", std::format("Waypoints detected in OTBM file.\n\nThey have been migrated to the external file:\n{}\n\nThey will be removed from the OTBM file on the next save.", map.waypointfile), wxOK | wxICON_INFORMATION);

			// Save immediately to XML
			if (!MapXMLIO::saveWaypoints(map, filename)) {
				// Migration failed
				map.waypointfile.clear();
				DialogUtil::PopupDialog(g_gui.root, "Error", "Failed to migrate waypoints to external XML file.\n"
															 "Waypoints will REMAIN in the OTBM file until migration succeeds.",
										wxOK | wxICON_ERROR);
			}
			// If successful, next save will skip OTBM writing because map.waypointfile is set
		}
	} else {
		// OTBM has no waypoints

		bool expected = !map.waypointfile.empty();

		// If map.waypointfile is empty (not set in OTBM), try to look for the default file
		if (!expected) {
			std::string defaultFile = nstr(filename.GetName()) + "-waypoint.xml";
			auto paths = MapXMLIO::normalizeMapFilePaths(filename, defaultFile);
			if (FileName(wxstr(paths.first)).FileExists()) {
				map.waypointfile = defaultFile;
				expected = true;
			}
		}

		if (expected) {
			// Try to load from XML
			if (!MapXMLIO::loadWaypoints(map, filename)) {
				spdlog::warn("Failed to load waypoint file: {}", map.waypointfile);
			}
		} else {
			// No waypoint file specified and no default file found, just set the default for future saves
			map.waypointfile = nstr(filename.GetName()) + "-waypoint.xml";
		}
	}

	map.getChangeTracker().markAllDirty();
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
	version.otbm = static_cast<MapVersionID>(raw_version);
	if (version.otbm > MAP_OTBM_4) {
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
		spdlog::warn("Outdated items.otb (major {}), could not load map", majorVersion);
	}
	version.client = static_cast<OtbVersionID>(minorVersion);

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
	if (!HeaderSerializationOTBM::readMapAttributes(map, stream)) {
		return false;
	}

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

	std::unordered_map<uint64_t, size_t> block_index_map;
	std::vector<BlockBucket> block_buckets;
	std::vector<TileAreaJob> unaligned_jobs;
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
				continue;
			}

			// Find end of this OTBM_TILE_AREA node
			stream.skipNode();
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
		} else if (child_type == OTBM_TOWNS) {
			FastOTBMNode townsNode(OTBM_TOWNS, stream.p, stream.end);
			TownSerializationOTBM::readTowns(map, townsNode);
			stream.p = townsNode.stream.p;
		} else if (child_type == OTBM_WAYPOINTS) {
			FastOTBMNode waypointsNode(OTBM_WAYPOINTS, stream.p, stream.end);
			WaypointSerializationOTBM::readWaypoints(map, waypointsNode);
			stream.p = waypointsNode.stream.p;
		} else {
			stream.skipNode();
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
	std::vector<std::vector<std::pair<uint32_t, Tile*>>> thread_house_tiles(n_threads);
	std::vector<uint64_t> thread_tile_counts(n_threads, 0);

	g_gui.SetLoadDone(20, "Loading map tiles in parallel...");
	auto t_load_start = std::chrono::high_resolution_clock::now();

	std::vector<std::thread> workers;
	workers.reserve(n_threads);

	for (unsigned int tid = 0; tid < n_threads; ++tid) {
		workers.emplace_back([&, tid]() {
			auto& house_tiles = thread_house_tiles[tid];
			auto& tile_count = thread_tile_counts[tid];
			while (true) {
				size_t bidx = next_bucket_idx.fetch_add(1, std::memory_order_relaxed);
				if (bidx >= block_buckets.size()) {
					break;
				}

				auto& bucket = block_buckets[bidx];
				for (const auto& job : bucket.jobs) {
					FastOTBMNode areaNode(OTBM_TILE_AREA, job.node_start, job.node_end);
					TileSerializationOTBM::readTileArea(*this, map, areaNode, &bucket.cell_indices, house_tiles, tile_count);
				}
			}
		});
	}

	// Smoothly update loading bar on main thread while workers are running
	const size_t total_buckets = block_buckets.size();
	while (true) {
		size_t done = next_bucket_idx.load(std::memory_order_relaxed);
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

	// Process any unaligned jobs (single-threaded fallback)
	if (!unaligned_jobs.empty()) {
		auto& house_tiles = thread_house_tiles[0];
		auto& tile_count = thread_tile_counts[0];
		for (const auto& job : unaligned_jobs) {
			FastOTBMNode areaNode(OTBM_TILE_AREA, job.node_start, job.node_end);
			TileSerializationOTBM::readTileArea(*this, map, areaNode, nullptr, house_tiles, tile_count);
		}
	}

	auto t_load_end = std::chrono::high_resolution_clock::now();
	spdlog::info("FastOTBM: Parallel tile loading completed in {:.2f} ms",
		std::chrono::duration<double, std::milli>(t_load_end - t_load_start).count());

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

	// Update tilecount directly from thread counters (O(1) instead of traversing all cells/floors)
	uint64_t tile_count = 0;
	for (uint64_t tc : thread_tile_counts) {
		tile_count += tc;
	}
	map.tilecount = tile_count;

	g_gui.SetLoadDone(100);
	return true;
}

bool IOMapOTBM::loadMap(Map& map, const FileName& filename) {
	return loadMapFromDisk(map, filename);
}

bool IOMapOTBM::saveMapToDisk(Map& map, const FileName& identifier) {
	DiskNodeFileWriteHandle f(
		nstr(identifier.GetFullPath()),
		(g_settings.getInteger(Config::SAVE_WITH_OTB_MAGIC_NUMBER) ? "OTBM" : std::string(4, '\0'))
	);

	if (!f.isOk()) {
		spdlog::error("Can not open file {} for writing", identifier.GetFullPath().ToStdString());
		return false;
	}

	if (!saveMap(map, f)) {
		return false;
	}

	g_gui.SetLoadDone(99, "Saving spawns...");
	if (!MapXMLIO::saveSpawns(map, identifier)) {
		spdlog::error("Failed to save spawns!");
		return false;
	}

	g_gui.SetLoadDone(99, "Saving houses...");
	if (!MapXMLIO::saveHouses(map, identifier)) {
		spdlog::error("IOMapOTBM::saveMapToDisk: Failed to save houses");
		return false;
	}

	// Always save waypoints to XML if they exist, creating a default file if needed
	if (map.waypoints.size() > 0) {
		if (map.waypointfile.empty()) {
			map.waypointfile = identifier.GetName() + "-waypoint.xml";
			// Notify user of auto-creation? Maybe not needed here as it's standard behavior now,
			// but we could log it.
			spdlog::info("Auto-created waypoint file: {}", map.waypointfile);
		}
		// Save waypoints to the external file
		if (!MapXMLIO::saveWaypoints(map, identifier)) {
			spdlog::error("IOMapOTBM::saveMapToDisk: Failed to save waypoints");
			return false;
		}
	} else if (!map.waypointfile.empty()) {
		// If we have an empty waypoint list but a file is defined, we should probably still save (to verify emptiness/update file)
		if (!MapXMLIO::saveWaypoints(map, identifier)) {
			spdlog::error("IOMapOTBM::saveMapToDisk: Failed to save waypoints");
			return false;
		}
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
				std::filesystem::path path(path_str);
				auto fname = path.filename().string();
				f.addU8(attr);
				f.addString(fname.empty() ? path_str : fname);
			};

			addExtFile(OTBM_ATTR_EXT_SPAWN_FILE, map.spawnfile);
			addExtFile(OTBM_ATTR_EXT_HOUSE_FILE, map.housefile);

			writeTileData(map, f);
			writeTowns(map, f);

			// Waypoints are strictly forbidden in OTBM (saved to XML only)
		}
		f.endNode();
	}
	f.endNode();

	return true;
}

void IOMapOTBM::writeTileData(const Map& map, NodeFileWriteHandle& f) {
	TileSerializationOTBM::writeTileData(*this, map, f);
}

void IOMapOTBM::writeTowns(const Map& map, NodeFileWriteHandle& f) {
	TownSerializationOTBM::writeTowns(map, f);
}

IOMapOTBM::WriteResult IOMapOTBM::writeWaypoints(const Map& map, NodeFileWriteHandle& f, MapVersion mapVersion) {
	return WaypointSerializationOTBM::writeWaypoints(map, f, mapVersion);
}
