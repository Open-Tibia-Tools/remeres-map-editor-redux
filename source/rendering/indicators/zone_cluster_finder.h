#ifndef RME_RENDERING_INDICATORS_ZONE_CLUSTER_FINDER_H_
#define RME_RENDERING_INDICATORS_ZONE_CLUSTER_FINDER_H_

#include "app/main.h"
#include "map/position.h"
#include "rendering/core/render_view.h"
#include "rendering/indicators/zone_flags.h"

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

class Map;
class BaseMap;

namespace rme::rendering {

/**
 * @brief Represents a single cluster badge rendered at the Fixed World Center of a connected zone.
 */
struct ZoneClusterBadge {
	uint32_t zone_flag = 0; // ZONE_FLAG_PZ, ZONE_FLAG_NOPVP, ZONE_FLAG_NOLOGOUT, ZONE_FLAG_PVPZONE
	int center_x = 0;
	int center_y = 0;
	int z = 0;
	int tile_count = 0;
	float width = 0.0f;
	float height = 0.0f;
	float offset_x = 0.0f; // Screen offset when multiple badges share the same center tile
	float offset_y = 0.0f;
};

/**
 * @brief Dedicated Service for discovering connected zone clusters and computing their Fixed World Center.
 *
 * Enforces Single Responsibility Principle (SRP) by decoupling topological cluster discovery
 * and Distance Transform geometric calculations from the rendering drawer.
 */
class ZoneClusterFinder {
public:
	ZoneClusterFinder() = default;
	~ZoneClusterFinder() = default;

	/**
	 * @brief Discovers and returns all Fixed World Center zone badges visible on floor z.
	 *
	 * Uses Breadth-First Search (BFS) to trace complete connected components across the map,
	 * then evaluates the Pole of Inaccessibility (multi-source BFS distance transform) to
	 * guarantee the badge is placed in the thickest, most interior part of the zone.
	 *
	 * Caches results per floor and generation counter so stationary viewing / panning within
	 * discovered regions costs zero BFS overhead.
	 */
	const std::vector<ZoneClusterBadge>& getVisibleBadges(
		int z,
		const ViewBounds& bounds,
		const Map& map,
		const BaseMap* secondary_map,
		uint64_t current_generation
	);

	void invalidate() noexcept {
		cached_generation_ = 0;
		floor_data_.clear();
		visible_badges_result_.clear();
	}

private:
	struct FloorClusterData {
		std::vector<ZoneClusterBadge> badges;
		std::unordered_set<uint64_t> visited_tiles; // Packed zone_bit and (x, y)
	};

	uint64_t cached_generation_ = 0;
	std::unordered_map<int, FloorClusterData> floor_data_;
	std::vector<ZoneClusterBadge> visible_badges_result_;
};

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_ZONE_CLUSTER_FINDER_H_
