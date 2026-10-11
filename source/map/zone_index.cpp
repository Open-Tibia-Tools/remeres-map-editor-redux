//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/zone_index.h"
#include "map/map.h"
#include "map/tile.h"

#include <algorithm>
#include <numeric>

namespace {
	const std::vector<Position> kEmptyPositions;
}

void ZoneIndex::addTile(uint16_t zoneId, const Position& pos) {
	if (zoneId == 0) {
		return;
	}

	auto& vec = index_[zoneId];
	const auto it = std::lower_bound(vec.begin(), vec.end(), pos);
	if (it == vec.end() || *it != pos) {
		vec.insert(it, pos);
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

	auto& vec = it->second;
	const auto pos_it = std::lower_bound(vec.begin(), vec.end(), pos);
	if (pos_it != vec.end() && *pos_it == pos) {
		vec.erase(pos_it);
	}
	if (vec.empty()) {
		index_.erase(it);
	}
}

void ZoneIndex::addTilesBulk(uint16_t zoneId, std::span<const Position> positions) {
	if (zoneId == 0 || positions.empty()) {
		return;
	}

	auto& vec = index_[zoneId];
	vec.reserve(vec.size() + positions.size());
	vec.insert(vec.end(), positions.begin(), positions.end());
	std::sort(vec.begin(), vec.end());
	const auto last = std::unique(vec.begin(), vec.end());
	vec.erase(last, vec.end());
}

const std::vector<Position>& ZoneIndex::getPositions(uint16_t zoneId) const noexcept {
	if (zoneId == 0) {
		return kEmptyPositions;
	}

	const auto it = index_.find(zoneId);
	return (it != index_.end()) ? it->second : kEmptyPositions;
}

bool ZoneIndex::hasZone(uint16_t zoneId) const noexcept {
	if (zoneId == 0) {
		return false;
	}

	const auto it = index_.find(zoneId);
	return it != index_.end() && !it->second.empty();
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
				index_[zoneId].push_back(pos);
			}
		}
	}

	for (auto& [_, vec] : index_) {
		std::sort(vec.begin(), vec.end());
		const auto last = std::unique(vec.begin(), vec.end());
		vec.erase(last, vec.end());
	}
}

size_t ZoneIndex::totalTileCount() const noexcept {
	return std::accumulate(index_.begin(), index_.end(), size_t { 0 },
		[](size_t sum, const auto& pair) {
			return sum + pair.second.size();
		});
}
