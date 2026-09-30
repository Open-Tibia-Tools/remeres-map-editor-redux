#ifndef RME_RENDERING_INDICATORS_ZONE_CLUSTER_FINDER_H_
#define RME_RENDERING_INDICATORS_ZONE_CLUSTER_FINDER_H_

#include "app/definitions.h"
#include "rendering/core/render_view.h"
#include "rendering/indicators/zone_flags.h"

#include <vector>
#include <array>
#include <unordered_set>
#include <cstdint>
#include <span>

class Map;
class BaseMap;

namespace rme::rendering {

/**
 * @brief Record of a visible zone tile discovered during the floor rendering pass.
 */
struct VisibleZoneTile {
	int16_t x = 0;
	int16_t y = 0;
	uint8_t flags = 0; // Bit 0: PZ, Bit 1: NOPVP, Bit 2: NOLOGOUT, Bit 3: PVPZONE
};

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
 *
 * Discovers clusters across full world coordinates, guaranteeing badges stay anchored to a fixed
 * world location and never slide or shift across tiles during camera movement (panning/zooming).
 *
 * Employs persistent per-floor caching so camera movement executes 0 BFS and 0 memory allocations.
 */
class ZoneClusterFinder {
public:
	ZoneClusterFinder() = default;
	~ZoneClusterFinder() = default;

	/**
	 * @brief Updates cluster discoveries on floor z and returns all badges currently in the viewport.
	 */
	const std::vector<ZoneClusterBadge>& updateAndGetVisibleBadges(
		int z,
		const ViewBounds& bounds,
		std::span<const VisibleZoneTile> visible_tiles,
		const Map& map,
		const BaseMap* secondary_map,
		uint64_t current_generation
	);

	[[nodiscard]] const std::vector<ZoneClusterBadge>& getLastBadges() const noexcept {
		return visible_badges_result_;
	}

	void invalidate() noexcept {
		cached_generation_ = 0;
		cached_secondary_map_ = nullptr;
		visible_badges_result_.clear();
		for (auto& floor : floor_data_) {
			floor.badges.clear();
			for (auto& vset : floor.visited_tiles) {
				vset.clear();
			}
		}
	}

private:
	struct FloorClusterData {
		std::vector<ZoneClusterBadge> badges;
		std::array<std::unordered_set<uint64_t>, 4> visited_tiles;
	};

	uint64_t cached_generation_ = 0;
	const BaseMap* cached_secondary_map_ = nullptr;
	std::array<FloorClusterData, MAP_LAYERS> floor_data_;
	std::vector<ZoneClusterBadge> visible_badges_result_;

	void resolveMultiBadgeOffsets(std::vector<ZoneClusterBadge>& badges);
};

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_ZONE_CLUSTER_FINDER_H_
