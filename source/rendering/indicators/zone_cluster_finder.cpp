#include "rendering/indicators/zone_cluster_finder.h"

#include <deque>
#include <array>
#include <algorithm>
#include <cmath>

namespace rme::rendering {

namespace {

struct ZoneTypeConfig {
	uint8_t bit_mask;
	uint32_t zone_bit;
};

inline constexpr std::array<ZoneTypeConfig, 4> ZONE_TYPES = {{
	{ 0x01, ZONE_FLAG_PZ },
	{ 0x02, ZONE_FLAG_NOPVP },
	{ 0x04, ZONE_FLAG_NOLOGOUT },
	{ 0x08, ZONE_FLAG_PVPZONE }
}};

inline constexpr std::array<std::pair<int, int>, 4> DIRS = {{{0, -1}, {0, 1}, {-1, 0}, {1, 0}}};

} // namespace

const std::vector<ZoneClusterBadge>& ZoneClusterFinder::findClusters(
	int z,
	const ViewBounds& bounds,
	std::span<const VisibleZoneTile> visible_tiles
) {
	visible_badges_result_.clear();
	if (visible_tiles.empty()) {
		return visible_badges_result_;
	}

	const int grid_w = bounds.end_x - bounds.start_x + 1;
	const int grid_h = bounds.end_y - bounds.start_y + 1;
	if (grid_w <= 0 || grid_h <= 0) {
		return visible_badges_result_;
	}

	const size_t total_cells = static_cast<size_t>(grid_w) * grid_h;
	if (tile_grid_.size() < total_cells) {
		tile_grid_.resize(total_cells);
		visited_grid_.resize(total_cells);
	}
	std::fill_n(tile_grid_.data(), total_cells, static_cast<uint8_t>(0));
	std::fill_n(visited_grid_.data(), total_cells, static_cast<uint8_t>(0));

	for (const auto& vt : visible_tiles) {
		const int lx = vt.x - bounds.start_x;
		const int ly = vt.y - bounds.start_y;
		if (lx >= 0 && lx < grid_w && ly >= 0 && ly < grid_h) {
			tile_grid_[ly * grid_w + lx] = vt.flags;
		}
	}

	for (const auto& zt : ZONE_TYPES) {
		for (const auto& vt : visible_tiles) {
			if ((vt.flags & zt.bit_mask) == 0) {
				continue;
			}

			const int start_lx = vt.x - bounds.start_x;
			const int start_ly = vt.y - bounds.start_y;
			if (start_lx < 0 || start_lx >= grid_w || start_ly < 0 || start_ly >= grid_h) {
				continue;
			}

			const int start_idx = start_ly * grid_w + start_lx;
			if ((visited_grid_[start_idx] & zt.bit_mask) != 0) {
				continue;
			}

			// 1. Explore connected component for this zone
			cluster_tiles_.clear();
			bfs_queue_.clear();

			visited_grid_[start_idx] |= zt.bit_mask;
			bfs_queue_.emplace_back(vt.x, vt.y);

			while (!bfs_queue_.empty()) {
				auto [cx, cy] = bfs_queue_.front();
				bfs_queue_.pop_front();
				cluster_tiles_.emplace_back(cx, cy);

				for (auto [dx, dy] : DIRS) {
					int nx = cx + dx;
					int ny = cy + dy;
					int nlx = nx - bounds.start_x;
					int nly = ny - bounds.start_y;
					if (nlx < 0 || nlx >= grid_w || nly < 0 || nly >= grid_h) {
						continue;
					}

					int nidx = nly * grid_w + nlx;
					if ((tile_grid_[nidx] & zt.bit_mask) != 0 && (visited_grid_[nidx] & zt.bit_mask) == 0) {
						visited_grid_[nidx] |= zt.bit_mask;
						bfs_queue_.emplace_back(nx, ny);
					}
				}
			}

			if (cluster_tiles_.empty()) {
				continue;
			}

			// 2. Evaluate Pole of Inaccessibility (multi-source BFS distance transform)
			int min_x = cluster_tiles_[0].first;
			int max_x = cluster_tiles_[0].first;
			int min_y = cluster_tiles_[0].second;
			int max_y = cluster_tiles_[0].second;

			for (const auto& [tx, ty] : cluster_tiles_) {
				min_x = std::min(min_x, tx);
				max_x = std::max(max_x, tx);
				min_y = std::min(min_y, ty);
				max_y = std::max(max_y, ty);
			}

			int c_w = max_x - min_x + 1;
			int c_h = max_y - min_y + 1;

			int best_cx = cluster_tiles_[0].first;
			int best_cy = cluster_tiles_[0].second;

			if (c_w > 0 && c_h > 0 && static_cast<size_t>(c_w) * c_h <= 65536) {
				const size_t cluster_cells = static_cast<size_t>(c_w) * c_h;
				if (cluster_dist_.size() < cluster_cells) {
					cluster_dist_.resize(cluster_cells);
					cluster_in_cluster_.resize(cluster_cells);
				}
				std::fill_n(cluster_dist_.data(), cluster_cells, 0);
				std::fill_n(cluster_in_cluster_.data(), cluster_cells, static_cast<uint8_t>(0));
				dt_queue_.clear();

				for (const auto& [tx, ty] : cluster_tiles_) {
					cluster_in_cluster_[(ty - min_y) * c_w + (tx - min_x)] = 1;
				}

				// Enqueue boundary tiles
				for (const auto& [tx, ty] : cluster_tiles_) {
					int clx = tx - min_x;
					int cly = ty - min_y;
					bool is_boundary = false;
					for (auto [dx, dy] : DIRS) {
						int nclx = clx + dx;
						int ncly = cly + dy;
						if (nclx < 0 || nclx >= c_w || ncly < 0 || ncly >= c_h || !cluster_in_cluster_[ncly * c_w + nclx]) {
							is_boundary = true;
							break;
						}
					}
					if (is_boundary) {
						cluster_dist_[cly * c_w + clx] = 1;
						dt_queue_.emplace_back(tx, ty);
					}
				}

				int max_dist = 1;
				while (!dt_queue_.empty()) {
					auto [cx, cy] = dt_queue_.front();
					dt_queue_.pop_front();
					int cur_d = cluster_dist_[(cy - min_y) * c_w + (cx - min_x)];
					if (cur_d > max_dist) {
						max_dist = cur_d;
					}

					for (auto [dx, dy] : DIRS) {
						int nx = cx + dx;
						int ny = cy + dy;
						int nclx = nx - min_x;
						int ncly = ny - min_y;
						if (nclx >= 0 && nclx < c_w && ncly >= 0 && ncly < c_h &&
							cluster_in_cluster_[ncly * c_w + nclx] && cluster_dist_[ncly * c_w + nclx] == 0) {
							cluster_dist_[ncly * c_w + nclx] = cur_d + 1;
							dt_queue_.emplace_back(nx, ny);
						}
					}
				}

				// Find tile with maximum clearance closest to bounding box midpoint
				float mid_x = (min_x + max_x) * 0.5f;
				float mid_y = (min_y + max_y) * 0.5f;
				float min_dist_to_mid = 1e9f;

				for (const auto& [tx, ty] : cluster_tiles_) {
					int d = cluster_dist_[(ty - min_y) * c_w + (tx - min_x)];
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

			// 3. Dynamic badge scaling based on cluster size & clearance
			// Uniform badge size across all zone types, adjusted to the largest label ("Protection Zone")
			const int tile_count = static_cast<int>(cluster_tiles_.size());
			const float base_w = 112.0f;

			float bw = base_w;
			float bh = 24.0f;
			if (tile_count > 4) {
				bw += 4.0f;
				bh = 25.0f;
			}
			if (tile_count > 25) {
				bw += 6.0f;
				bh = 26.0f;
			}

			// Constrain badge size so it does not overflow narrow corridors, but never clip below base_w
			const float max_bw = std::max(base_w, static_cast<float>(c_w * 32 - 4));
			const float max_bh = std::max(24.0f, static_cast<float>(c_h * 32 - 4));
			bw = std::min(bw, max_bw);
			bh = std::min(bh, max_bh);

			visible_badges_result_.push_back(ZoneClusterBadge {
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
		}
	}

	// 4. Resolve multi-badge relative offsets for shared center tiles
	if (visible_badges_result_.size() > 1) {
		badge_keys_.clear();
		badge_keys_.reserve(visible_badges_result_.size());
		for (size_t i = 0; i < visible_badges_result_.size(); ++i) {
			const uint64_t ux = static_cast<uint32_t>(visible_badges_result_[i].center_x);
			const uint64_t uy = static_cast<uint32_t>(visible_badges_result_[i].center_y);
			uint64_t pos_key = (ux << 32) | uy;
			badge_keys_.emplace_back(pos_key, i);
		}

		std::sort(badge_keys_.begin(), badge_keys_.end(), [](const auto& a, const auto& b) {
			return a.first < b.first;
		});

		size_t run_start = 0;
		while (run_start < badge_keys_.size()) {
			size_t run_end = run_start + 1;
			while (run_end < badge_keys_.size() && badge_keys_[run_end].first == badge_keys_[run_start].first) {
				++run_end;
			}
			const size_t group_size = run_end - run_start;

			if (group_size == 1) {
				visible_badges_result_[badge_keys_[run_start].second].offset_x = 0.0f;
				visible_badges_result_[badge_keys_[run_start].second].offset_y = 0.0f;
			} else if (group_size == 2) {
				size_t idx0 = badge_keys_[run_start].second;
				size_t idx1 = badge_keys_[run_start + 1].second;
				float w0 = visible_badges_result_[idx0].width;
				float w1 = visible_badges_result_[idx1].width;
				float gap = 2.0f;
				float total_w = w0 + gap + w1;
				visible_badges_result_[idx0].offset_x = -total_w * 0.5f + w0 * 0.5f;
				visible_badges_result_[idx0].offset_y = 0.0f;
				visible_badges_result_[idx1].offset_x = total_w * 0.5f - w1 * 0.5f;
				visible_badges_result_[idx1].offset_y = 0.0f;
			} else {
				size_t idx0 = badge_keys_[run_start].second;
				float w0 = visible_badges_result_[idx0].width;
				float h0 = visible_badges_result_[idx0].height;
				float shift_x = w0 * 0.5f + 1.0f;
				float shift_y = h0 * 0.5f + 1.0f;

				visible_badges_result_[idx0].offset_x = -shift_x;
				visible_badges_result_[idx0].offset_y = -shift_y;
				visible_badges_result_[badge_keys_[run_start + 1].second].offset_x = shift_x;
				visible_badges_result_[badge_keys_[run_start + 1].second].offset_y = -shift_y;
				if (group_size >= 3) {
					visible_badges_result_[badge_keys_[run_start + 2].second].offset_x = -shift_x;
					visible_badges_result_[badge_keys_[run_start + 2].second].offset_y = shift_y;
				}
				if (group_size >= 4) {
					visible_badges_result_[badge_keys_[run_start + 3].second].offset_x = shift_x;
					visible_badges_result_[badge_keys_[run_start + 3].second].offset_y = shift_y;
				}
			}

			run_start = run_end;
		}
	}

	return visible_badges_result_;
}

} // namespace rme::rendering
