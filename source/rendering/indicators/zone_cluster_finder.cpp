#include "rendering/indicators/zone_cluster_finder.h"
#include "map/map.h"
#include "map/basemap.h"
#include "map/tile.h"

#include <deque>
#include <array>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace rme::rendering {

namespace {

struct ZoneTypeConfig {
	uint8_t bit_mask;
	uint32_t zone_bit;
};

inline constexpr std::array<ZoneTypeConfig, 4> ZONE_TYPES = {{
	{ 0x01, static_cast<uint32_t>(ZONE_FLAG_PZ) },
	{ 0x02, static_cast<uint32_t>(ZONE_FLAG_NOPVP) },
	{ 0x04, static_cast<uint32_t>(ZONE_FLAG_NOLOGOUT) },
	{ 0x08, static_cast<uint32_t>(ZONE_FLAG_PVPZONE) }
}};

inline constexpr std::array<std::pair<int, int>, 4> DIRS = {{{0, -1}, {0, 1}, {-1, 0}, {1, 0}}};

inline uint64_t packPos(int x, int y) noexcept {
	return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
	        static_cast<uint64_t>(static_cast<uint32_t>(y));
}

inline bool tileHasZone(const Map& map, const BaseMap* secondary_map, int x, int y, int z, uint8_t bit_mask) {
	const Tile* t = secondary_map ? secondary_map->getTile(x, y, z) : nullptr;
	if (!t) {
		t = map.getTile(x, y, z);
	}
	if (!t) {
		return false;
	}
	if ((bit_mask & 0x01) && t->isPZ()) {
		return true;
	}
	const uint32_t mf = t->getMapFlags();
	if ((bit_mask & 0x02) && (mf & TILESTATE_NOPVP)) {
		return true;
	}
	if ((bit_mask & 0x04) && (mf & TILESTATE_NOLOGOUT)) {
		return true;
	}
	if ((bit_mask & 0x08) && (mf & TILESTATE_PVPZONE)) {
		return true;
	}
	return false;
}

} // namespace

const std::vector<ZoneClusterBadge>& ZoneClusterFinder::updateAndGetVisibleBadges(
	int z,
	const ViewBounds& bounds,
	std::span<const VisibleZoneTile> visible_tiles,
	const Map& map,
	const BaseMap* secondary_map,
	uint64_t current_generation
) {
	if (current_generation != cached_generation_ || secondary_map != cached_secondary_map_) {
		invalidate();
		cached_generation_ = current_generation;
		cached_secondary_map_ = secondary_map;
	}

	visible_badges_result_.clear();
	if (z < 0 || z >= MAP_LAYERS) {
		return visible_badges_result_;
	}

	auto& data = floor_data_[z];
	bool added_new_cluster = false;

	if (!visible_tiles.empty()) {
		for (size_t zt_idx = 0; zt_idx < ZONE_TYPES.size(); ++zt_idx) {
			const auto& zt = ZONE_TYPES[zt_idx];
			auto& visited_set = data.visited_tiles[zt_idx];

			for (const auto& vt : visible_tiles) {
				if ((vt.flags & zt.bit_mask) == 0) {
					continue;
				}

				const uint64_t start_key = packPos(vt.x, vt.y);
				if (visited_set.contains(start_key)) {
					continue;
				}

				// Unvisited zone cluster discovered!
				// Explore the full connected component across the world map.
				std::vector<std::pair<int, int>> cluster_tiles;
				std::deque<std::pair<int, int>> bfs_queue;

				visited_set.insert(start_key);
				bfs_queue.emplace_back(vt.x, vt.y);

				constexpr size_t MAX_CLUSTER_TILES = 10000;

				while (!bfs_queue.empty() && cluster_tiles.size() < MAX_CLUSTER_TILES) {
					auto [cx, cy] = bfs_queue.front();
					bfs_queue.pop_front();
					cluster_tiles.emplace_back(cx, cy);

					for (auto [dx, dy] : DIRS) {
						int nx = cx + dx;
						int ny = cy + dy;
						uint64_t nkey = packPos(nx, ny);
						if (visited_set.contains(nkey)) {
							continue;
						}

						if (tileHasZone(map, secondary_map, nx, ny, z, zt.bit_mask)) {
							visited_set.insert(nkey);
							bfs_queue.emplace_back(nx, ny);
						}
					}
				}

				if (cluster_tiles.empty()) {
					continue;
				}

				// Compute world bounding box
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

				int c_w = max_x - min_x + 1;
				int c_h = max_y - min_y + 1;

				int best_cx = cluster_tiles[0].first;
				int best_cy = cluster_tiles[0].second;

				if (c_w > 0 && c_h > 0 && static_cast<size_t>(c_w) * c_h <= 65536) {
					std::vector<int> dist(c_w * c_h, 0);
					std::vector<uint8_t> in_cluster(c_w * c_h, 0);

					for (const auto& [tx, ty] : cluster_tiles) {
						in_cluster[(ty - min_y) * c_w + (tx - min_x)] = 1;
					}

					std::deque<std::pair<int, int>> dt_queue;
					for (const auto& [tx, ty] : cluster_tiles) {
						int clx = tx - min_x;
						int cly = ty - min_y;
						bool is_boundary = false;
						for (auto [dx, dy] : DIRS) {
							int nclx = clx + dx;
							int ncly = cly + dy;
							if (nclx < 0 || nclx >= c_w || ncly < 0 || ncly >= c_h || !in_cluster[ncly * c_w + nclx]) {
								is_boundary = true;
								break;
							}
						}
						if (is_boundary) {
							dist[cly * c_w + clx] = 1;
							dt_queue.emplace_back(tx, ty);
						}
					}

					int max_dist = 1;
					while (!dt_queue.empty()) {
						auto [cx, cy] = dt_queue.front();
						dt_queue.pop_front();
						int cur_d = dist[(cy - min_y) * c_w + (cx - min_x)];
						if (cur_d > max_dist) {
							max_dist = cur_d;
						}

						for (auto [dx, dy] : DIRS) {
							int nx = cx + dx;
							int ny = cy + dy;
							int nclx = nx - min_x;
							int ncly = ny - min_y;
							if (nclx >= 0 && nclx < c_w && ncly >= 0 && ncly < c_h &&
							    in_cluster[ncly * c_w + nclx] && dist[ncly * c_w + nclx] == 0) {
								dist[ncly * c_w + nclx] = cur_d + 1;
								dt_queue.emplace_back(nx, ny);
							}
						}
					}

					// Find tile with maximum clearance closest to bounding box midpoint
					float mid_x = (min_x + max_x) * 0.5f;
					float mid_y = (min_y + max_y) * 0.5f;
					float min_dist_to_mid = 1e9f;

					for (const auto& [tx, ty] : cluster_tiles) {
						int d = dist[(ty - min_y) * c_w + (tx - min_x)];
						if (d == max_dist) {
							float dist_mid = (tx - mid_x) * (tx - mid_x) + (ty - mid_y) * (ty - mid_y);
							if (dist_mid < min_dist_to_mid) {
								min_dist_to_mid = dist_mid;
								best_cx = tx;
								best_cy = ty;
							}
						}
					}
				} else {
					// Fallback for massive clusters: pick cluster tile closest to bounding box center
					float mid_x = (min_x + max_x) * 0.5f;
					float mid_y = (min_y + max_y) * 0.5f;
					float min_dist_to_mid = 1e9f;
					for (const auto& [tx, ty] : cluster_tiles) {
						float dist_mid = (tx - mid_x) * (tx - mid_x) + (ty - mid_y) * (ty - mid_y);
						if (dist_mid < min_dist_to_mid) {
							min_dist_to_mid = dist_mid;
							best_cx = tx;
							best_cy = ty;
						}
					}
				}

				// Dynamic badge scaling based on cluster size & clearance
				const int tile_count = static_cast<int>(cluster_tiles.size());
				float bw, bh;
				if (tile_count == 1) {
					bw = 18.0f;
					bh = 12.0f;
				} else if (tile_count <= 4) {
					bw = 26.0f;
					bh = 15.0f;
				} else if (tile_count <= 12) {
					bw = 38.0f;
					bh = 20.0f;
				} else if (tile_count <= 25) {
					bw = 52.0f;
					bh = 26.0f;
				} else if (tile_count <= 50) {
					bw = 66.0f;
					bh = 32.0f;
				} else if (tile_count <= 100) {
					bw = 82.0f;
					bh = 38.0f;
				} else {
					bw = 98.0f;
					bh = 44.0f;
				}

				// Constrain badge size so it does not overflow narrow corridors
				const float max_bw = std::max(18.0f, static_cast<float>(c_w * 32 - 4));
				const float max_bh = std::max(12.0f, static_cast<float>(c_h * 32 - 4));
				bw = std::min(bw, max_bw);
				bh = std::min(bh, max_bh);

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

	if (added_new_cluster) {
		resolveMultiBadgeOffsets(data.badges);
	}

	// Filter badges visible within the current viewport
	const int margin = 4;
	for (const auto& badge : data.badges) {
		if (badge.center_x >= bounds.start_x - margin && badge.center_x <= bounds.end_x + margin &&
		    badge.center_y >= bounds.start_y - margin && badge.center_y <= bounds.end_y + margin) {
			visible_badges_result_.push_back(badge);
		}
	}

	return visible_badges_result_;
}

void ZoneClusterFinder::resolveMultiBadgeOffsets(std::vector<ZoneClusterBadge>& badges) {
	if (badges.empty()) {
		return;
	}
	if (badges.size() == 1) {
		badges[0].offset_x = 0.0f;
		badges[0].offset_y = 0.0f;
		return;
	}

	std::unordered_map<uint64_t, std::vector<size_t>> badges_by_pos;
	for (size_t i = 0; i < badges.size(); ++i) {
		uint64_t pos_key = packPos(badges[i].center_x, badges[i].center_y);
		badges_by_pos[pos_key].push_back(i);
	}

	for (const auto& [_, indices] : badges_by_pos) {
		if (indices.size() == 1) {
			badges[indices[0]].offset_x = 0.0f;
			badges[indices[0]].offset_y = 0.0f;
		} else if (indices.size() == 2) {
			float w0 = badges[indices[0]].width;
			float w1 = badges[indices[1]].width;
			float gap = 2.0f;
			float total_w = w0 + gap + w1;
			badges[indices[0]].offset_x = -total_w * 0.5f + w0 * 0.5f;
			badges[indices[0]].offset_y = 0.0f;
			badges[indices[1]].offset_x = total_w * 0.5f - w1 * 0.5f;
			badges[indices[1]].offset_y = 0.0f;
		} else {
			// 3 or 4 badges: 2x2 grid
			float w0 = badges[indices[0]].width;
			float h0 = badges[indices[0]].height;
			float shift_x = w0 * 0.5f + 1.0f;
			float shift_y = h0 * 0.5f + 1.0f;

			badges[indices[0]].offset_x = -shift_x;
			badges[indices[0]].offset_y = -shift_y;
			badges[indices[1]].offset_x = shift_x;
			badges[indices[1]].offset_y = -shift_y;
			if (indices.size() >= 3) {
				badges[indices[2]].offset_x = -shift_x;
				badges[indices[2]].offset_y = shift_y;
			}
			if (indices.size() >= 4) {
				badges[indices[3]].offset_x = shift_x;
				badges[indices[3]].offset_y = shift_y;
			}
		}
	}
}

} // namespace rme::rendering
