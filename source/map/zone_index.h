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
 * @brief Represents a single connected component / island of zone tiles on a specific floor.
 */
struct ZoneCluster {
	Position centroid_pos;
	int min_x = 0;
	int max_x = 0;
	int min_y = 0;
	int max_y = 0;
	uint32_t tile_count = 0;
};

/**
 * @brief High-throughput spatial index mapping OTBM 5 zone IDs to tile positions.
 *
 * Provides O(1) retrieval of all positions associated with a zone ID, eliminating
 * full-map tile scans for viewport labeling, camera jumping, and zone removal.
 * Also provides cached 8-connected component clusters for dynamic per-room labeling.
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

	// Returns cached 8-connected clusters for this zone across all floors.
	[[nodiscard]] const std::vector<ZoneCluster>& getClusters(uint16_t zoneId, bool recompute_if_dirty = true) const;

	// Returns whether the index contains any tiles for the given zone.
	[[nodiscard]] bool hasZone(uint16_t zoneId) const noexcept;

	// Returns a sorted list of all unique zone IDs currently indexed.
	[[nodiscard]] std::vector<uint16_t> getZoneIds() const;

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
	[[nodiscard]] bool hasFloor(int z) const noexcept;

private:
	struct ZoneData {
		std::vector<Position> positions;
		mutable std::vector<ZoneCluster> clusters;
		mutable bool clusters_dirty = true;
		uint16_t floors_mask = 0;
	};

	void updateClusters(const ZoneData& data) const;

	std::unordered_map<uint16_t, ZoneData> index_;
};

#endif // RME_MAP_ZONE_INDEX_H_
