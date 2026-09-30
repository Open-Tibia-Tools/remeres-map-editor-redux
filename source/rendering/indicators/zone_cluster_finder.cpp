#include "rendering/indicators/zone_cluster_finder.h"
#include "map/map.h"
#include "map/tile.h"

#include <deque>
#include <array>
#include <algorithm>
#include <cmath>

namespace rme::rendering {

namespace {

struct ZoneTypeConfig {
	uint32_t map_flag;
	uint32_t zone_bit;
};

inline constexpr std::array<ZoneTypeConfig, 4> ZONE_TYPES = {{
	{ TILESTATE_PROTECTIONZONE, static_cast<uint32_t>(ZONE_FLAG_PZ) },
	{ TILESTATE_NOPVP,          static_cast<uint32_t>(ZONE_FLAG_NOPVP) },
	{ TILESTATE_NOLOGOUT,       static_cast<uint32_t>(ZONE_FLAG_NOLOGOUT) },
	{ TILESTATE_PVPZONE,        static_cast<uint32_t>(ZONE_FLAG_PVPZONE) }
}};

inline uint64_t makeTileKey(uint32_t zone_bit, int x, int y) noexcept {
	return (static_cast<uint64_t>(zone_bit) << 48) |
	       (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 24) |
	       static_cast<uint64_t>(static_cast<uint32_t>(y));
}

inline bool tileHasZone(const Map& map, const BaseMap* secondary_map, int x, int y, int z, uint32_t map_flag) {
	const Tile* t = secondary_map ? secondary_map->getTile(x, y, z) : nullptr;
	if (!t) {
		t = map.getTile(x, y, z);
	}
	if (!t || !t->hasGround()) {
		return false;
	}
	if (map_flag == TILESTATE_PROTECTIONZONE) {
		return t->isPZ();
	}
	return (t->getMapFlags() & map_flag) != 0;
}

} // namespace

const std::vector<ZoneClusterBadge>& ZoneClusterFinder::getVisibleBadges(
	int z,
	const ViewBounds& bounds,
	const Map& map,
	const BaseMap* secondary_map,
	uint64_t current_generation
) {
	if (current_generation != cached_generation_) {
		floor_data_.clear();
		cached_generation_ = current_generation;
	}

	auto& data = floor_data_[z];
	static constexpr std::array<std::pair<int, int>, 4> DIRS = {{{0, -1}, {0, 1}, {-1, 0}, {1, 0}}};

	bool added_new_cluster = false;

	// 1. Scan visible viewport to discover new clusters
	for (const auto& zt : ZONE_TYPES) {
		for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
			for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
				const uint64_t tile_key = makeTileKey(zt.zone_bit, x, y);
				if (data.visited_tiles.contains(tile_key)) {
					continue;
				}

				if (!tileHasZone(map, secondary_map, x, y, z, zt.map_flag)) {
					continue;
				}

				// Unvisited zone tile discovered! Run BFS to explore the entire connected component in world coordinates.
				std::vector<std::pair<int, int>> cluster_tiles;
				std::deque<std::pair<int, int>> bfs_queue;

				data.visited_tiles.insert(tile_key);
				bfs_queue.emplace_back(x, y);

				while (!bfs_queue.empty()) {
					auto [cx, cy] = bfs_queue.front();
					bfs_queue.pop_front();
					cluster_tiles.emplace_back(cx, cy);

					for (auto [dx, dy] : DIRS) {
						int nx = cx + dx;
						int ny = cy + dy;
						uint64_t nkey = makeTileKey(zt.zone_bit, nx, ny);
						if (data.visited_tiles.contains(nkey)) {
							continue;
						}

						if (tileHasZone(map, secondary_map, nx, ny, z, zt.map_flag)) {
							data.visited_tiles.insert(nkey);
							bfs_queue.emplace_back(nx, ny);
						}
					}
				}

				if (cluster_tiles.empty()) {
					continue;
				}

				// 2. Evaluate Pole of Inaccessibility (multi-source BFS distance transform)
				int min_x = cluster_tiles[0].first;
				int max_x = cluster_tiles[0].first;
				int min_y = cluster_tiles[0].second;
				int max_y = cluster_tiles[0].second;

				for (const auto& [tx, ty] : cluster_tiles) {
					min_x = std::min(min_x, tx);
					max_x = std::max(max_x, tx);
					min_y = std::min(min_y, ty);
					max_y = std::max(max_y, ty);
				}

				int grid_w = max_x - min_x + 1;
				int grid_h = max_y - min_y + 1;

				int best_cx = cluster_tiles[0].first;
				int best_cy = cluster_tiles[0].second;

				if (grid_w > 0 && grid_h > 0 && static_cast<size_t>(grid_w) * grid_h <= 250000) {
					std::vector<int> dist(grid_w * grid_h, 0);
					std::vector<uint8_t> in_cluster(grid_w * grid_h, 0);

					for (const auto& [tx, ty] : cluster_tiles) {
						in_cluster[(ty - min_y) * grid_w + (tx - min_x)] = 1;
					}

					std::deque<std::pair<int, int>> dt_queue;
					// Enqueue all boundary tiles (tiles adjacent to exterior)
					for (const auto& [tx, ty] : cluster_tiles) {
						int lx = tx - min_x;
						int ly = ty - min_y;
						bool is_boundary = false;
						for (auto [dx, dy] : DIRS) {
							int nlx = lx + dx;
							int nly = ly + dy;
							if (nlx < 0 || nlx >= grid_w || nly < 0 || nly >= grid_h || !in_cluster[nly * grid_w + nlx]) {
								is_boundary = true;
								break;
							}
						}
						if (is_boundary) {
							dist[ly * grid_w + lx] = 1;
							dt_queue.emplace_back(tx, ty);
						}
					}

					int max_dist = 1;
					while (!dt_queue.empty()) {
						auto [cx, cy] = dt_queue.front();
						dt_queue.pop_front();
						int cur_d = dist[(cy - min_y) * grid_w + (cx - min_x)];
						if (cur_d > max_dist) {
							max_dist = cur_d;
						}

						for (auto [dx, dy] : DIRS) {
							int nx = cx + dx;
							int ny = cy + dy;
							int nlx = nx - min_x;
							int nly = ny - min_y;
							if (nlx >= 0 && nlx < grid_w && nly >= 0 && nly < grid_h &&
								in_cluster[nly * grid_w + nlx] && dist[nly * grid_w + nlx] == 0) {
								dist[nly * grid_w + nlx] = cur_d + 1;
								dt_queue.emplace_back(nx, ny);
							}
						}
					}

					// Find tile with maximum clearance closest to bounding box midpoint
					float mid_x = (min_x + max_x) * 0.5f;
					float mid_y = (min_y + max_y) * 0.5f;
					float min_dist_to_mid = 1e9f;

					for (const auto& [tx, ty] : cluster_tiles) {
						int d = dist[(ty - min_y) * grid_w + (tx - min_x)];
						if (d == max_dist) {
							float dist_mid = (tx - mid_x) * (tx - mid_x) + (ty - mid_y) * (ty - mid_y);
							if (dist_mid < min_dist_to_mid) {
								min_dist_to_mid = dist_mid;
								best_cx = tx;
								best_cy = ty;
							}
						}
					}
				}

				// 3. Determine badge scaling based on cluster size
				const int tile_count = static_cast<int>(cluster_tiles.size());
				float bw, bh;
				if (tile_count == 1) {
					bw = 18.0f;
					bh = 12.0f;
				} else if (tile_count <= 4) {
					bw = 26.0f;
					bh = 15.0f;
				} else {
					bw = 38.0f;
					bh = 20.0f;
				}

				data.badges.push_back(ZoneClusterBadge {
					.zone_flag = zt.zone_bit,
					.center_x = best_cx,
					.center_y = best_cy,
					.z = z,
					.tile_count = tile_count,
					.width = bw,
					.height = bh,
					.offset_x = 0.0f,
					.offset_y = 0.0f
				});
				added_new_cluster = true;
			}
		}
	}

	// 4. If any new cluster was added, resolve multi-badge relative offsets for shared center tiles
	if (added_new_cluster) {
		std::unordered_map<uint64_t, std::vector<size_t>> badges_by_pos;
		for (size_t i = 0; i < data.badges.size(); ++i) {
			uint64_t pos_key = (static_cast<uint64_t>(static_cast<uint32_t>(data.badges[i].center_x)) << 32) |
			                   static_cast<uint64_t>(static_cast<uint32_t>(data.badges[i].center_y));
			badges_by_pos[pos_key].push_back(i);
		}

		for (const auto& [_, indices] : badges_by_pos) {
			if (indices.size() == 1) {
				data.badges[indices[0]].offset_x = 0.0f;
				data.badges[indices[0]].offset_y = 0.0f;
			} else if (indices.size() == 2) {
				float w0 = data.badges[indices[0]].width;
				float w1 = data.badges[indices[1]].width;
				float gap = 2.0f;
				float total_w = w0 + gap + w1;
				data.badges[indices[0]].offset_x = -total_w * 0.5f + w0 * 0.5f;
				data.badges[indices[0]].offset_y = 0.0f;
				data.badges[indices[1]].offset_x = total_w * 0.5f - w1 * 0.5f;
				data.badges[indices[1]].offset_y = 0.0f;
			} else {
				// 3 or 4 badges: 2x2 grid
				float w0 = data.badges[indices[0]].width;
				float h0 = data.badges[indices[0]].height;
				float shift_x = w0 * 0.5f + 1.0f;
				float shift_y = h0 * 0.5f + 1.0f;

				data.badges[indices[0]].offset_x = -shift_x;
				data.badges[indices[0]].offset_y = -shift_y;
				data.badges[indices[1]].offset_x = shift_x;
				data.badges[indices[1]].offset_y = -shift_y;
				if (indices.size() >= 3) {
					data.badges[indices[2]].offset_x = -shift_x;
					data.badges[indices[2]].offset_y = shift_y;
				}
				if (indices.size() >= 4) {
					data.badges[indices[3]].offset_x = shift_x;
					data.badges[indices[3]].offset_y = shift_y;
				}
			}
		}
	}

	// 5. Filter badges visible within the current viewport (with a 2-tile margin)
	visible_badges_result_.clear();
	const int margin = 2;
	for (const auto& badge : data.badges) {
		if (badge.center_x >= bounds.start_x - margin && badge.center_x <= bounds.end_x + margin &&
		    badge.center_y >= bounds.start_y - margin && badge.center_y <= bounds.end_y + margin) {
			visible_badges_result_.push_back(badge);
		}
	}

	return visible_badges_result_;
}

} // namespace rme::rendering
