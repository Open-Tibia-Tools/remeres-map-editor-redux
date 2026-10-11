//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_ZONE_INDEX_H_
#define RME_MAP_ZONE_INDEX_H_

#include "map/position.h"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

class Map;

/**
 * @brief High-throughput spatial index mapping OTBM 5 zone IDs to tile positions.
 *
 * Provides O(1) retrieval of all positions associated with a zone ID, eliminating
 * full-map tile scans for viewport labeling, camera jumping, and zone removal.
 */
class ZoneIndex {
public:
	ZoneIndex() = default;
	~ZoneIndex() = default;

	ZoneIndex(const ZoneIndex&) = delete;
	ZoneIndex& operator=(const ZoneIndex&) = delete;
	ZoneIndex(ZoneIndex&&) noexcept = default;
	ZoneIndex& operator=(ZoneIndex&&) noexcept = default;

	// Adds a position to the specified zone. Idempotent (maintains sorted order without duplicates).
	void addTile(uint16_t zoneId, const Position& pos);

	// Removes a position from the specified zone.
	void removeTile(uint16_t zoneId, const Position& pos);

	// Bulk insertion for multi-threaded loader finalization. Sorts and deduplicates.
	void addTilesBulk(uint16_t zoneId, std::span<const Position> positions);

	// Fast O(1) lookup returning a const reference to the contiguous position buffer.
	[[nodiscard]] const std::vector<Position>& getPositions(uint16_t zoneId) const noexcept;

	// Returns whether the index contains any tiles for the given zone.
	[[nodiscard]] bool hasZone(uint16_t zoneId) const noexcept;

	// Removes all indexed positions for a zone ID.
	void removeZone(uint16_t zoneId);

	// Clears the entire index.
	void clear() noexcept;

	// Full sweep rebuild across all tiles on the map.
	void rebuild(Map& map);

	[[nodiscard]] size_t totalTileCount() const noexcept;
	[[nodiscard]] size_t zoneCount() const noexcept {
		return index_.size();
	}

private:
	std::unordered_map<uint16_t, std::vector<Position>> index_;
};

#endif // RME_MAP_ZONE_INDEX_H_
