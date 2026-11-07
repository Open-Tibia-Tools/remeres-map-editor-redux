#ifndef RME_RENDERING_INDICATORS_ZONE_CLUSTER_FINDER_H_
#define RME_RENDERING_INDICATORS_ZONE_CLUSTER_FINDER_H_

#include "rendering/core/render_view.h"
#include "rendering/indicators/zone_flags.h"

#include <vector>
#include <deque>
#include <utility>
#include <cstdint>
#include <span>

namespace rme::rendering {

/**
 * @brief Record of a visible zone tile discovered during the floor rendering pass.
 */
struct VisibleZoneTile {
	int x = 0;
	int y = 0;
	uint8_t flags = 0; // Bit 0: PZ, Bit 1: NOPVP, Bit 2: NOLOGOUT, Bit 3: PVPZONE
};

/**
 * @brief Represents a single cluster badge rendered at the Pole of Inaccessibility of a connected zone.
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
 * @brief Dedicated Service for discovering connected zone clusters and computing their badges.
 *
 * Enforces Single Responsibility Principle (SRP) by decoupling topological cluster discovery
 * and Distance Transform geometric calculations from the rendering drawer.
 *
 * Operates purely on visible zone tiles in O(N_tiles) time with zero extra map lookups or hash sets,
 * ensuring maximum performance with 0 FPS impact.
 */
class ZoneClusterFinder {
public:
	ZoneClusterFinder() = default;
	~ZoneClusterFinder() = default;

	/**
	 * @brief Discovers and returns all cluster badges visible on floor z.
	 *
	 * Operates directly on the tiles collected during the row pass in O(N) time with flat arrays.
	 * Evaluates 4-connected components and Pole of Inaccessibility (multi-source BFS distance transform)
	 * for each disconnected cluster independently, guaranteeing every zone shows its badge and scales
	 * proportionately to the cluster's size.
	 */
	const std::vector<ZoneClusterBadge>& findClusters(
		int z,
		const ViewBounds& bounds,
		std::span<const VisibleZoneTile> visible_tiles
	);

	[[nodiscard]] const std::vector<ZoneClusterBadge>& getLastBadges() const noexcept {
		return visible_badges_result_;
	}

private:
	std::vector<uint8_t> tile_grid_;
	std::vector<uint8_t> visited_grid_;
	std::vector<ZoneClusterBadge> visible_badges_result_;

	// Reusable scratch buffers for DOD cache locality and zero heap allocations per frame
	std::vector<std::pair<int, int>> cluster_tiles_;
	std::deque<std::pair<int, int>> bfs_queue_;
	std::vector<int> cluster_dist_;
	std::vector<uint8_t> cluster_in_cluster_;
	std::deque<std::pair<int, int>> dt_queue_;
	std::vector<std::pair<uint64_t, size_t>> badge_keys_;
};

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_ZONE_CLUSTER_FINDER_H_
