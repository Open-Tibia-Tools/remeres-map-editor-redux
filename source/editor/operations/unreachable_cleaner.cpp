//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "editor/operations/unreachable_cleaner.h"

#include "editor/editor.h"
#include "editor/action_queue.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/map_region.h"
#include "map/spatial_hash_grid.h"
#include "game/item.h"
#include "io/iomap_otbm.h"
#include "ui/gui.h"
#include "ui/dialog_util.h"
#include "app/definitions.h"
#include "util/file_system.h"
#include "util/common.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <utility>
#include <spdlog/spdlog.h>

namespace {

struct CleanStats {
	uint64_t walkable_count = 0;
	uint64_t non_walkable_count = 0;
	int logged_walkable = 0;
	int logged_removal = 0;
	int logged_kept = 0;
};

CleanStats g_stats;

} // namespace

namespace EditorOperations {

bool UnreachableCleaner::IsWalkable(const Tile* tile) {
	if (!tile || !tile->hasGround() || tile->empty() || !tile->ground) {
		++g_stats.non_walkable_count;
		return false;
	}

	// Blocking state: true if ground or any item is unpassable or blocking
	if (tile->isBlocking() || tile->ground->isBlocking()) {
		++g_stats.non_walkable_count;
		return false;
	}

	const auto ground_def = tile->ground->getDefinition();
	if (!ground_def) {
		++g_stats.non_walkable_count;
		return false;
	}

	// Must be an actual ground tile and full ground (DatFlagFullGround)
	if (!ground_def.isGroundTile() || !ground_def.hasFlag(ItemFlag::FullTile)) {
		++g_stats.non_walkable_count;
		return false;
	}

	// Border items are edges/fringe, not walkable ground
	if (ground_def.hasFlag(ItemFlag::IsBorder) || ground_def.hasFlag(ItemFlag::IsOptionalBorder)) {
		++g_stats.non_walkable_count;
		return false;
	}

	// Unpassable (DatFlagNotWalkable) or pathfinder blocking tiles cannot be walked on
	if (ground_def.hasFlag(ItemFlag::Unpassable) || ground_def.hasFlag(ItemFlag::BlockPathfinder)) {
		++g_stats.non_walkable_count;
		return false;
	}

	// Trashholders (water, lava, swamp, tar) are hazard grounds that destroy items and cannot be walked on
	if (ground_def.isTrashHolder()) {
		++g_stats.non_walkable_count;
		return false;
	}

	// Check loose items on the tile: if any is unpassable, blocking, wall, or hazard, the tile is non-walkable
	for (const auto& item : tile->items) {
		if (!item) {
			continue;
		}
		if (item->isBlocking()) {
			++g_stats.non_walkable_count;
			return false;
		}
		const auto item_def = item->getDefinition();
		if (item_def) {
			if (item_def.hasFlag(ItemFlag::Unpassable) ||
			    item_def.hasFlag(ItemFlag::BlockPathfinder) ||
			    item_def.hasFlag(ItemFlag::IsWall) ||
			    item_def.isTrashHolder()) {
				++g_stats.non_walkable_count;
				return false;
			}
		}
	}

	++g_stats.walkable_count;
	if (g_stats.logged_walkable < 10) {
		++g_stats.logged_walkable;
		spdlog::info(
			"UnreachableCleaner::IsWalkable: Tile at ({}, {}, {}) with ground item {} ('{}') -> Classified as WALKABLE viewpoint.",
			tile->getPosition().x, tile->getPosition().y, tile->getPosition().z,
			tile->ground->getID(), ground_def.name()
		);
	}
	return true;
}

std::vector<Position> UnreachableCleaner::FindUnreachableTiles(
	Map& map,
	const UnreachableCleanerSettings& settings,
	uint64_t* out_total_tiles
) {
	const int rx = std::max(1, (settings.viewport_width / 2) + settings.safety_margin);
	const int ry = std::max(1, (settings.viewport_height / 2) + settings.safety_margin);

	g_stats = CleanStats();

	spdlog::info(
		"UnreachableCleaner::FindUnreachableTiles: Starting unreachable tile analysis (viewport: {}x{}, safety margin: {}, multi-floor: {}, query radius rx={}, ry={})",
		settings.viewport_width, settings.viewport_height, settings.safety_margin, settings.multi_floor, rx, ry
	);

	// Step 1: Walkable Indexing using SpatialHashGrid
	struct NodeWalkableInfo {
		uint16_t floor_mask = 0;
		uint16_t tile_masks[MAP_LAYERS] = {0};
	};

	std::unordered_map<const MapNode*, NodeWalkableInfo> walkable_nodes;
	std::unordered_map<uint64_t, uint16_t> walkable_cells;

	for (const auto& cell_entry : map.getGrid()) {
		const auto& cell = cell_entry.cell;
		if (!cell) {
			continue;
		}

		uint16_t cell_mask = 0;
		for (int n = 0; n < SpatialHashGrid::NODES_IN_CELL; ++n) {
			const MapNode* node = cell->nodes[n].get();
			if (!node) {
				continue;
			}

			NodeWalkableInfo node_info;
			for (int z = 0; z < MAP_LAYERS; ++z) {
				const Floor* floor = node->getFloor(z);
				if (!floor) {
					continue;
				}

				uint16_t tmask = 0;
				for (int t = 0; t < SpatialHashGrid::TILES_PER_NODE; ++t) {
					const Tile* tile = floor->locs[t].get();
					if (tile && IsWalkable(tile)) {
						tmask |= static_cast<uint16_t>(1u << t);
					}
				}

				if (tmask != 0) {
					node_info.floor_mask |= static_cast<uint16_t>(1u << z);
					node_info.tile_masks[z] = tmask;
				}
			}

			if (node_info.floor_mask != 0) {
				walkable_nodes[node] = node_info;
				cell_mask |= node_info.floor_mask;
			}
		}

		if (cell_mask != 0) {
			walkable_cells[cell_entry.key] = cell_mask;
		}
	}

	g_gui.SetLoadDone(20);

	spdlog::info(
		"UnreachableCleaner::FindUnreachableTiles: Step 1 indexing complete. Indexed {} walkable nodes across {} cells (walkable viewpoints: {}, non-walkable: {}).",
		walkable_nodes.size(), walkable_cells.size(), g_stats.walkable_count, g_stats.non_walkable_count
	);

	// Cell neighborhood check: returns true if any cell in the 3x3 neighborhood of (cx, cy)
	// contains at least one walkable tile on any of the target_floors_mask floors.
	auto has_walkable_in_cell_neighborhood = [&](int cx, int cy, uint16_t target_floors_mask) -> bool {
		for (int dcx = -1; dcx <= 1; ++dcx) {
			for (int dcy = -1; dcy <= 1; ++dcy) {
				const uint64_t nkey = SpatialHashGrid::makeKeyFromCell(cx + dcx, cy + dcy);
				auto it = walkable_cells.find(nkey);
				if (it != walkable_cells.end() && (it->second & target_floors_mask) != 0) {
					return true;
				}
			}
		}
		return false;
	};

	// A cell can be culled as isolated if the maximum search distance (including floor projection |dz| <= 7)
	// fits strictly inside the 3x3 cell neighborhood (i.e. < 64 tiles in each axis).
	const bool can_use_cell_culling = ((rx + 7) < SpatialHashGrid::CELL_SIZE && (ry + 7) < SpatialHashGrid::CELL_SIZE);

	std::vector<Position> to_remove;
	uint64_t total_tiles_checked = 0;
	size_t cells_done = 0;
	const size_t total_cells = map.getGrid().cellCount();

	struct WalkableTileCoord {
		int x;
		int y;
		int z;
	};
	std::vector<WalkableTileCoord> nearby_walkable;
	nearby_walkable.reserve(256);

	for (const auto& cell_entry : map.getGrid()) {
		const auto& cell = cell_entry.cell;
		if (!cell) {
			continue;
		}

		int cx = 0;
		int cy = 0;
		SpatialHashGrid::getCellCoordsFromKey(cell_entry.key, cx, cy);

		bool all_floors_isolated = false;
		bool surface_isolated = false;
		bool underground_isolated = false;

		if (can_use_cell_culling) {
			all_floors_isolated = !has_walkable_in_cell_neighborhood(cx, cy, 0xFFFF);
			if (settings.multi_floor) {
				surface_isolated = all_floors_isolated || !has_walkable_in_cell_neighborhood(cx, cy, 0x00FF);
				underground_isolated = all_floors_isolated || !has_walkable_in_cell_neighborhood(cx, cy, 0xFF00);
			}
		}

		for (int n = 0; n < SpatialHashGrid::NODES_IN_CELL; ++n) {
			const MapNode* node = cell->nodes[n].get();
			if (!node) {
				continue;
			}

			auto node_walk_it = walkable_nodes.find(node);
			const bool node_has_walkable = (node_walk_it != walkable_nodes.end());

			for (int z = 0; z < MAP_LAYERS; ++z) {
				const Floor* floor = node->getFloor(z);
				if (!floor) {
					continue;
				}

				const bool is_surface = (z <= GROUND_LAYER);
				const bool is_floor_cell_isolated = all_floors_isolated ||
					(settings.multi_floor && (is_surface ? surface_isolated : underground_isolated));

				const uint16_t this_node_tmask = node_has_walkable ? node_walk_it->second.tile_masks[z] : 0;

				// Fast path: Entire cell (or floor layer) has no walkable tiles in 3x3 neighborhood
				if (is_floor_cell_isolated) {
					for (int t = 0; t < SpatialHashGrid::TILES_PER_NODE; ++t) {
						const Tile* tile = floor->locs[t].get();
						if (!tile || tile->empty()) {
							continue;
						}
						++total_tiles_checked;
						const Position pos = floor->locs[t].getPosition();
						to_remove.push_back(pos);

						if (g_stats.logged_removal < 25) {
							++g_stats.logged_removal;
							spdlog::info(
								"UnreachableCleaner::FindUnreachableTiles: Tile at ({}, {}, {}) [ground item={}] ADDED to removal. "
								"Reason: Entire cell ({}, {}) on floor {} is completely isolated (no walkable tiles in 3x3 neighborhood).",
								pos.x, pos.y, pos.z,
								tile->ground ? tile->ground->getID() : 0,
								cx, cy, z
							);
						}
					}
					continue;
				}

				// Fast path: All 16 tiles in this node on floor z are walkable (never removed)
				if (this_node_tmask == 0xFFFF) {
					total_tiles_checked += SpatialHashGrid::TILES_PER_NODE;
					continue;
				}

				// Candidate floors that could see floor z
				uint16_t candidate_mask = 0;
				int min_dz = 0;
				int max_dz = 0;

				if (settings.multi_floor) {
					if (is_surface) {
						candidate_mask = 0x00FF; // surface floors 0..7
						min_dz = z - GROUND_LAYER; // z - 7 <= 0
						max_dz = z - 0;            // z >= 0
					} else {
						const int min_z = std::max(GROUND_LAYER + 1, z - 2);
						const int max_z = std::min(MAP_MAX_LAYER, z + 2);
						for (int wz = min_z; wz <= max_z; ++wz) {
							candidate_mask |= static_cast<uint16_t>(1u << wz);
						}
						min_dz = 0;
						max_dz = 0;
					}
				} else {
					candidate_mask = static_cast<uint16_t>(1u << z);
					min_dz = 0;
					max_dz = 0;
				}

				const int node_base_x = floor->locs[0].getPosition().x;
				const int node_base_y = floor->locs[0].getPosition().y;

				// Bounding box of all walkable tile coordinates (wx, wy) that could see any tile in this node:
				const int query_min_x = node_base_x + min_dz - rx;
				const int query_max_x = node_base_x + 3 + max_dz + rx;
				const int query_min_y = node_base_y + min_dz - ry;
				const int query_max_y = node_base_y + 3 + max_dz + ry;

				nearby_walkable.clear();
				map.visitLeaves(query_min_x, query_min_y, query_max_x + 1, query_max_y + 1, [&](const MapNode* qnode, int qx, int qy) {
					auto it = walkable_nodes.find(qnode);
					if (it == walkable_nodes.end() || (it->second.floor_mask & candidate_mask) == 0) {
						return;
					}
					for (int wz = 0; wz < MAP_LAYERS; ++wz) {
						if ((candidate_mask & (1u << wz)) == 0) {
							continue;
						}
						const uint16_t wtmask = it->second.tile_masks[wz];
						if (wtmask == 0) {
							continue;
						}
						for (int t = 0; t < SpatialHashGrid::TILES_PER_NODE; ++t) {
							if (wtmask & (1u << t)) {
								nearby_walkable.push_back({ qx + (t >> 2), qy + (t & 3), wz });
							}
						}
					}
				});

				// Fast path: No walkable tiles in range anywhere
				if (nearby_walkable.empty()) {
					for (int t = 0; t < SpatialHashGrid::TILES_PER_NODE; ++t) {
						const Tile* tile = floor->locs[t].get();
						if (!tile || tile->empty()) {
							continue;
						}
						++total_tiles_checked;
						if ((this_node_tmask & (1u << t)) == 0) {
							const Position pos = floor->locs[t].getPosition();
							to_remove.push_back(pos);

							if (g_stats.logged_removal < 25) {
								++g_stats.logged_removal;
								spdlog::info(
									"UnreachableCleaner::FindUnreachableTiles: Tile at ({}, {}, {}) [ground item={}] ADDED to removal. "
									"Reason: No walkable tiles anywhere in query bounding box [{}, {}] to [{}, {}].",
									pos.x, pos.y, pos.z,
									tile->ground ? tile->ground->getID() : 0,
									query_min_x, query_min_y, query_max_x, query_max_y
								);
							}
						}
					}
					continue;
				}

				// Walkable tiles exist nearby: check each non-walkable tile
				for (int t = 0; t < SpatialHashGrid::TILES_PER_NODE; ++t) {
					const Tile* tile = floor->locs[t].get();
					if (!tile || tile->empty()) {
						continue;
					}
					++total_tiles_checked;
					if (this_node_tmask & (1u << t)) {
						// Walkable tile - never removed!
						continue;
					}

					const Position pos = floor->locs[t].getPosition();
					bool can_be_seen = false;
					WalkableTileCoord seeing_wt = {0, 0, 0};

					for (const auto& wt : nearby_walkable) {
						const int dz = (pos.z <= GROUND_LAYER && wt.z <= GROUND_LAYER) ? (pos.z - wt.z) : 0;
						if (std::abs((pos.x + dz) - wt.x) <= rx && std::abs((pos.y + dz) - wt.y) <= ry) {
							can_be_seen = true;
							seeing_wt = wt;
							break;
						}
					}

					if (!can_be_seen) {
						to_remove.push_back(pos);
						if (g_stats.logged_removal < 50) {
							++g_stats.logged_removal;
							spdlog::info(
								"UnreachableCleaner::FindUnreachableTiles: Tile at ({}, {}, {}) [ground item={}] ADDED to removal. "
								"Reason: Non-walkable and unreachable; checked {} nearby walkable candidate(s), all exceeded viewport bounds (rx={}, ry={}).",
								pos.x, pos.y, pos.z,
								tile->ground ? tile->ground->getID() : 0,
								nearby_walkable.size(), rx, ry
							);
						}
					} else {
						if (g_stats.logged_kept < 25) {
							++g_stats.logged_kept;
							const int dz = (pos.z <= GROUND_LAYER && seeing_wt.z <= GROUND_LAYER) ? (pos.z - seeing_wt.z) : 0;
							spdlog::info(
								"UnreachableCleaner::FindUnreachableTiles: Tile at ({}, {}, {}) [ground item={}] NOT added to removal (KEPT). "
								"Reason: Visible from walkable viewpoint at ({}, {}, {}) (screen dx={}, dy={}, max_rx={}, max_ry={}, dz={}).",
								pos.x, pos.y, pos.z,
								tile->ground ? tile->ground->getID() : 0,
								seeing_wt.x, seeing_wt.y, seeing_wt.z,
								std::abs((pos.x + dz) - seeing_wt.x),
								std::abs((pos.y + dz) - seeing_wt.y),
								rx, ry, dz
							);
						}
					}
				}
			}
		}

		++cells_done;
		if (cells_done % 32 == 0 && total_cells > 0) {
			g_gui.SetLoadDone(20 + static_cast<int>(cells_done * 75.0 / static_cast<double>(total_cells)));
		}
	}

	if (out_total_tiles) {
		*out_total_tiles = total_tiles_checked;
	}

	spdlog::info(
		"UnreachableCleaner::FindUnreachableTiles: Analysis complete. "
		"Total tiles checked: {}, Unreachable tiles marked for removal: {}. "
		"Walkable viewpoints: {}, Non-walkable evaluations: {}.",
		total_tiles_checked, to_remove.size(),
		g_stats.walkable_count, g_stats.non_walkable_count
	);

	return to_remove;
}

bool UnreachableCleaner::CreateBackup(Editor& editor, std::string& out_backup_path, std::string& out_error) {
	std::time_t now = std::time(nullptr);
	std::tm tm_now = *std::localtime(&now);
	char time_buf[64];
	std::strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", &tm_now);

	FileName target_file;
	if (editor.map.hasFile()) {
		FileName src(wxstr(editor.map.getFilename()));
		target_file = src;
		target_file.SetName(src.GetName() + "_backup_" + time_buf);
	} else {
		std::string dir = nstr(FileSystem::GetLocalDataDirectory());
		target_file.Assign(wxstr(dir + "untitled_backup_" + time_buf + ".otbm"));
	}

	out_backup_path = nstr(target_file.GetFullPath());

	const std::string original_waypointfile = editor.map.getWaypointFilename();
	const bool original_changed = editor.map.hasChanged();

	IOMapOTBM mapsaver(editor.map.getVersion());
	const bool save_ok = mapsaver.saveMap(editor.map, target_file);

	// Restore waypointfile and preserve dirty state on both success and failure paths
	editor.map.setWaypointFilename(original_waypointfile);
	if (!original_changed && editor.map.hasChanged()) {
		editor.map.clearChanges();
	}

	if (!save_ok) {
		out_error = "Failed to save backup map file to: " + out_backup_path;
		spdlog::error("UnreachableCleaner::CreateBackup: {}", out_error);
		return false;
	}

	spdlog::info("UnreachableCleaner::CreateBackup: Created backup at {}", out_backup_path);
	return true;
}

UnreachableCleanerResult UnreachableCleaner::Clean(Editor& editor, const UnreachableCleanerSettings& settings) {
	UnreachableCleanerResult result;
	Map& map = editor.map;

	spdlog::info("UnreachableCleaner::Clean: Invoked. Starting analysis of unreachable tiles.");
	g_gui.CreateLoadBar("Analyzing unreachable tiles...");

	std::vector<Position> to_remove = FindUnreachableTiles(map, settings, &result.total_tiles_checked);

	g_gui.DestroyLoadBar();

	result.unreachable_tiles_found = to_remove.size();

	if (to_remove.empty()) {
		spdlog::info("UnreachableCleaner::Clean: No unreachable tiles found.");
		DialogUtil::PopupDialog("Remove Unreachable Tiles", "No unreachable tiles were found on the map.", wxOK);
		return result;
	}

	// Confirm removal with user
	wxString prompt;
	prompt << "Found " << to_remove.size() << " unreachable tile(s) that are never visible from any player viewpoint.\n\n";
	if (settings.create_backup) {
		prompt << "A backup will be created before any tiles are removed.\n\n";
	}
	prompt << "Do you want to permanently delete these tiles?";

	int confirm = DialogUtil::PopupDialog("Remove Unreachable Tiles", prompt, wxYES | wxNO);
	if (confirm != wxID_YES) {
		spdlog::info("UnreachableCleaner::Clean: User cancelled tile removal.");
		return result;
	}

	// Step 3: Create backup if requested
	if (settings.create_backup) {
		std::string backup_err;
		if (!CreateBackup(editor, result.backup_path, backup_err)) {
			int proceed = DialogUtil::PopupDialog(
				"Backup Failed",
				"Failed to create map backup:\n" + backup_err + "\n\nDo you want to proceed with tile removal anyway?",
				wxYES | wxNO
			);
			if (proceed != wxID_YES) {
				result.error_message = backup_err;
				spdlog::warn("UnreachableCleaner::Clean: Aborted after backup failure.");
				return result;
			}
		} else {
			result.backup_created = true;
		}
	}

	// Step 4: Delete unreachable tiles
	spdlog::info("UnreachableCleaner::Clean: Beginning deletion of {} unreachable tile(s)...", to_remove.size());
	g_gui.CreateLoadBar("Removing unreachable tiles...");

	editor.selection.clear();
	if (editor.actionQueue) {
		editor.actionQueue->clear();
	}

	uint64_t remove_done = 0;
	const uint64_t total_to_remove = to_remove.size();

	for (const auto& pos : to_remove) {
		(void)map.setTile(pos, nullptr);
		++result.tiles_removed;
		++remove_done;

		if (remove_done % 0x2000 == 0 && total_to_remove > 0) {
			g_gui.SetLoadDone(static_cast<int>(remove_done * 100.0 / static_cast<double>(total_to_remove)));
		}
	}

	g_gui.DestroyLoadBar();

	map.doChange();
	g_gui.RefreshView();

	spdlog::info("UnreachableCleaner::Clean: Successfully removed {} unreachable tile(s).", result.tiles_removed);

	wxString status;
	status << "Removed " << result.tiles_removed << " unreachable tile(s).";
	if (result.backup_created) {
		status << " Backup: " << wxstr(result.backup_path);
	}
	g_gui.SetStatusText(status);

	wxString complete_msg;
	complete_msg << "Successfully removed " << result.tiles_removed << " unreachable tile(s).";
	if (result.backup_created) {
		complete_msg << "\n\nBackup saved to:\n" << wxstr(result.backup_path);
	}
	DialogUtil::PopupDialog("Removal Complete", complete_msg, wxOK);

	return result;
}

} // namespace EditorOperations
