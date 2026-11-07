//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/zone_index.h"
#include "map/map.h"
#include "map/tile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
	const std::vector<Position> kEmptyPositions;
	const std::vector<ZoneCluster> kEmptyClusters;

	struct Dsu {
		std::vector<int> parent;
		std::vector<uint8_t> rank;
		explicit Dsu(size_t n) : parent(n), rank(n, 0) {
			std::iota(parent.begin(), parent.end(), 0);
		}
		int find(int x) {
			if (parent[x] == x) {
				return x;
			}
			return parent[x] = find(parent[x]);
		}
		void unite(int a, int b) {
			int root_a = find(a);
			int root_b = find(b);
			if (root_a != root_b) {
				if (rank[root_a] < rank[root_b]) {
					std::swap(root_a, root_b);
				}
				parent[root_b] = root_a;
				if (rank[root_a] == rank[root_b]) {
					++rank[root_a];
				}
			}
		}
	};
}

void ZoneIndex::updateClusters(const ZoneData& data) const {
	data.clusters.clear();
	data.clusters_dirty = false;

	const auto& positions = data.positions;
	if (positions.empty()) {
		return;
	}

	// Positions are guaranteed sorted by z, then y, then x.
	// Process floor by floor to find 8-connected components.
	size_t i = 0;
	while (i < positions.size()) {
		const int cur_z = positions[i].z;
		const size_t start = i;
		while (i < positions.size() && positions[i].z == cur_z) {
			++i;
		}
		const size_t end = i;
		const size_t floor_count = end - start;

		if (floor_count == 1) {
			const Position p = positions[start];
			data.clusters.push_back(ZoneCluster {
				.centroid_pos = p,
				.min_x = p.x,
				.max_x = p.x,
				.min_y = p.y,
				.max_y = p.y,
				.tile_count = 1
			});
			continue;
		}

		Dsu dsu(floor_count);
		std::unordered_map<uint64_t, int> coord_to_idx;
		coord_to_idx.reserve(floor_count);

		// Causal 8-neighbors (top-left, top, top-right, left).
		// Because tiles are sorted by y and then x, these 4 predecessors
		// cover all possible edges in the 8-connected neighborhood.
		static constexpr std::array<std::pair<int, int>, 4> kPredecessors = {{
			{-1, -1}, {0, -1}, {1, -1}, {-1, 0}
		}};

		for (size_t idx = 0; idx < floor_count; ++idx) {
			const Position& p = positions[start + idx];
			const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(p.x)) << 32) |
			                     static_cast<uint32_t>(p.y);
			coord_to_idx.emplace(key, static_cast<int>(idx));

			for (const auto& [dx, dy] : kPredecessors) {
				const int nx = p.x + dx;
				const int ny = p.y + dy;
				const uint64_t nkey = (static_cast<uint64_t>(static_cast<uint32_t>(nx)) << 32) |
				                      static_cast<uint32_t>(ny);
				if (auto it = coord_to_idx.find(nkey); it != coord_to_idx.end()) {
					dsu.unite(static_cast<int>(idx), it->second);
				}
			}
		}

		struct ClusterAccumulator {
			int min_x = std::numeric_limits<int>::max();
			int max_x = std::numeric_limits<int>::min();
			int min_y = std::numeric_limits<int>::max();
			int max_y = std::numeric_limits<int>::min();
			int64_t sum_x = 0;
			int64_t sum_y = 0;
			uint32_t count = 0;
			double avg_x = 0.0;
			double avg_y = 0.0;
			double min_dist_sq = std::numeric_limits<double>::infinity();
			Position best_pos;
		};

		// DOD Refactoring: Flat vector indexed directly by root index [0, floor_count)
		std::vector<ClusterAccumulator> groups(floor_count);
		std::vector<int> root_indices;
		root_indices.reserve(floor_count);

		for (size_t idx = 0; idx < floor_count; ++idx) {
			const int root = dsu.find(static_cast<int>(idx));
			const Position& p = positions[start + idx];
			auto& acc = groups[root];
			if (acc.count == 0) {
				root_indices.push_back(root);
			}
			acc.min_x = std::min(acc.min_x, p.x);
			acc.max_x = std::max(acc.max_x, p.x);
			acc.min_y = std::min(acc.min_y, p.y);
			acc.max_y = std::max(acc.max_y, p.y);
			acc.sum_x += p.x;
			acc.sum_y += p.y;
			++acc.count;
		}

		for (int root : root_indices) {
			auto& acc = groups[root];
			acc.avg_x = static_cast<double>(acc.sum_x) / acc.count;
			acc.avg_y = static_cast<double>(acc.sum_y) / acc.count;
		}

		for (size_t idx = 0; idx < floor_count; ++idx) {
			const int root = dsu.find(static_cast<int>(idx));
			const Position& p = positions[start + idx];
			auto& acc = groups[root];
			const double dx = static_cast<double>(p.x) - acc.avg_x;
			const double dy = static_cast<double>(p.y) - acc.avg_y;
			const double dsq = dx * dx + dy * dy;
			if (dsq < acc.min_dist_sq) {
				acc.min_dist_sq = dsq;
				acc.best_pos = p;
			}
		}

		for (int root : root_indices) {
			const auto& acc = groups[root];
			data.clusters.push_back(ZoneCluster {
				.centroid_pos = acc.best_pos,
				.min_x = acc.min_x,
				.max_x = acc.max_x,
				.min_y = acc.min_y,
				.max_y = acc.max_y,
				.tile_count = acc.count
			});
		}
	}

	std::ranges::sort(data.clusters, [](const ZoneCluster& a, const ZoneCluster& b) {
		if (a.centroid_pos.z != b.centroid_pos.z) {
			return a.centroid_pos.z < b.centroid_pos.z;
		}
		if (a.centroid_pos.y != b.centroid_pos.y) {
			return a.centroid_pos.y < b.centroid_pos.y;
		}
		return a.centroid_pos.x < b.centroid_pos.x;
	});
}

void ZoneIndex::addTile(uint16_t zoneId, const Position& pos) {
	if (zoneId == 0) {
		return;
	}

	auto& entry = index_[zoneId];
	if (pos.z >= 0 && pos.z < 16) {
		entry.floors_mask |= (1u << pos.z);
	}
	const auto it = std::lower_bound(entry.positions.begin(), entry.positions.end(), pos);
	if (it == entry.positions.end() || *it != pos) {
		entry.positions.insert(it, pos);
		entry.clusters_dirty = true;
	}
}

void ZoneIndex::removeTile(uint16_t zoneId, const Position& pos) {
	if (zoneId == 0) {
		return;
	}

	const auto it = index_.find(zoneId);
	if (it == index_.end()) {
		return;
	}

	auto& entry = it->second;
	const auto pos_it = std::lower_bound(entry.positions.begin(), entry.positions.end(), pos);
	if (pos_it != entry.positions.end() && *pos_it == pos) {
		const auto next_it = entry.positions.erase(pos_it);
		entry.clusters_dirty = true;
		if (pos.z >= 0 && pos.z < 16) {
			bool has_same_z = false;
			if (next_it != entry.positions.end() && next_it->z == pos.z) {
				has_same_z = true;
			} else if (next_it != entry.positions.begin()) {
				const auto prev_it = std::prev(next_it);
				if (prev_it->z == pos.z) {
					has_same_z = true;
				}
			}
			if (!has_same_z) {
				entry.floors_mask &= ~(1u << pos.z);
			}
		}
	}
	if (entry.positions.empty()) {
		index_.erase(it);
	}
}

void ZoneIndex::addTilesBulk(uint16_t zoneId, std::span<const Position> positions) {
	if (zoneId == 0 || positions.empty()) {
		return;
	}

	auto& entry = index_[zoneId];
	for (const auto& p : positions) {
		if (p.z >= 0 && p.z < 16) {
			entry.floors_mask |= (1u << p.z);
		}
	}

	const size_t old_size = entry.positions.size();
	if (old_size == 0) {
		entry.positions.assign(positions.begin(), positions.end());
		std::ranges::sort(entry.positions);
		const auto [drop_it, end_it] = std::ranges::unique(entry.positions);
		entry.positions.erase(drop_it, end_it);
		entry.clusters_dirty = true;
		return;
	}

	entry.positions.reserve(old_size + positions.size());
	entry.positions.insert(entry.positions.end(), positions.begin(), positions.end());

	// Sort only the newly appended elements, then merge sorted ranges in linear time
	auto middle = entry.positions.begin() + old_size;
	std::sort(middle, entry.positions.end());
	std::inplace_merge(entry.positions.begin(), middle, entry.positions.end());

	const auto [drop_it, end_it] = std::ranges::unique(entry.positions);
	entry.positions.erase(drop_it, end_it);
	entry.clusters_dirty = true;
}

const std::vector<Position>& ZoneIndex::getPositions(uint16_t zoneId) const noexcept {
	if (zoneId == 0) {
		return kEmptyPositions;
	}

	const auto it = index_.find(zoneId);
	return (it != index_.end()) ? it->second.positions : kEmptyPositions;
}

const std::vector<ZoneCluster>& ZoneIndex::getClusters(uint16_t zoneId, bool recompute_if_dirty) const {
	if (zoneId == 0) {
		return kEmptyClusters;
	}

	const auto it = index_.find(zoneId);
	if (it == index_.end()) {
		return kEmptyClusters;
	}

	if (it->second.clusters_dirty && recompute_if_dirty) {
		updateClusters(it->second);
	}
	return it->second.clusters;
}

bool ZoneIndex::hasZone(uint16_t zoneId) const noexcept {
	if (zoneId == 0) {
		return false;
	}

	const auto it = index_.find(zoneId);
	return it != index_.end() && !it->second.positions.empty();
}

std::vector<uint16_t> ZoneIndex::getZoneIds() const {
	std::vector<uint16_t> ids;
	ids.reserve(index_.size());
	for (const auto& [zoneId, _] : index_) {
		ids.push_back(zoneId);
	}
	std::ranges::sort(ids);
	return ids;
}

void ZoneIndex::removeZone(uint16_t zoneId) {
	index_.erase(zoneId);
}

void ZoneIndex::clear() noexcept {
	index_.clear();
}

void ZoneIndex::rebuild(Map& map) {
	index_.clear();
	for (const auto& tile_loc : map.tiles()) {
		if (const Tile* tile = tile_loc.get(); tile && tile->hasZones()) {
			const Position pos = tile->getPosition();
			for (const uint16_t zoneId : tile->getZoneIds()) {
				auto& entry = index_[zoneId];
				entry.positions.push_back(pos);
				if (pos.z >= 0 && pos.z < 16) {
					entry.floors_mask |= (1u << pos.z);
				}
			}
		}
	}

	for (auto& [_, entry] : index_) {
		std::sort(entry.positions.begin(), entry.positions.end());
		const auto last = std::unique(entry.positions.begin(), entry.positions.end());
		entry.positions.erase(last, entry.positions.end());
		entry.clusters_dirty = true;
	}
}

bool ZoneIndex::hasFloor(int z) const noexcept {
	if (z < 0 || z >= 16) {
		return false;
	}
	const uint16_t mask = (1u << z);
	for (const auto& [_, entry] : index_) {
		if ((entry.floors_mask & mask) != 0) {
			return true;
		}
	}
	return false;
}

size_t ZoneIndex::totalTileCount() const noexcept {
	return std::accumulate(index_.begin(), index_.end(), size_t { 0 },
		[](size_t sum, const auto& pair) {
			return sum + pair.second.positions.size();
		});
}
