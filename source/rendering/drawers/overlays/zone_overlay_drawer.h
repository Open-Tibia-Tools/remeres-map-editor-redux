#ifndef RME_RENDERING_DRAWERS_OVERLAYS_ZONE_OVERLAY_DRAWER_H_
#define RME_RENDERING_DRAWERS_OVERLAYS_ZONE_OVERLAY_DRAWER_H_

#include "rendering/indicators/zone_cluster_finder.h"

class SpriteBatch;
struct RenderView;
class Map;
class BaseMap;
struct DrawingOptions;
class AtlasManager;
class Tile;

namespace rme::rendering {

/**
 * @brief Editor-wide LOD policy cutoff: beyond 10x zoom-out (zoom <= 10%, view.zoom > 10.0f),
 *        all zone overlays, text labels, and dynamic entities are culled.
 */
inline constexpr float kZoomLODCutoff = 10.0f;

/**
 * @brief Spawn detail LOD threshold: beyond ~15% zoom (view.zoom > 6.67f),
 *        individual 1px/2px borders and center flame badges become subpixel and illegible.
 *        In this range, spawns are rendered with simplified, high-throughput area fill quads.
 */
inline constexpr float kSpawnLODDetailThreshold = 1.0f / 0.15f; // ~6.6667f

/**
 * @brief Default fallback coarse radius for spawn AABB culling.
 */
inline constexpr int kMaxCoarseRadius = 128;

/**
 * @brief Evaluates whether a tile is considered path-blocking for map navigation overlays.
 *
 * Excludes technical invisible wall items (e.g. Server ID 1548 / TechInvisibleWall),
 * which have their own dedicated indicator badge ("BLOCK") and are not terrain/pathing obstacles.
 */
[[nodiscard]] bool IsTilePathBlocking(const Tile* t) noexcept;

/**
 * @brief Dedicated OpenGL On-Top Overlay Drawer for:
 *        - Special Zones (PZ, No-PvP, No-Logout, PvP Zone) with 1px outer connected borders
 *        - Pathing / Blocking (red wash + 1px outer connected borders)
 *        - Spawns (individual bounding boxes with independent outer borders + center badges)
 *
 * Rendered on top of the entire map and lighting pass, preventing occlusion by tall walls or trees.
 */
class ZoneOverlayDrawer {
public:
	ZoneOverlayDrawer() = default;
	~ZoneOverlayDrawer() = default;

	void drawFloor(SpriteBatch& sprite_batch,
	               int z,
	               const RenderView& view,
	               const Map& map,
	               const BaseMap* secondary_map,
	               const DrawingOptions& options,
	               const AtlasManager& atlas);

	void drawFloorBlocking(SpriteBatch& sprite_batch,
	                       int z,
	                       const RenderView& view,
	                       const Map& map,
	                       const BaseMap* secondary_map,
	                       const DrawingOptions& options,
	                       const AtlasManager& atlas);

	void drawFloorHouses(SpriteBatch& sprite_batch,
	                     int z,
	                     const RenderView& view,
	                     const Map& map,
	                     const BaseMap* secondary_map,
	                     const DrawingOptions& options,
	                     const AtlasManager& atlas);

	void drawFloorHighlightItems(SpriteBatch& sprite_batch,
	                             int z,
	                             const RenderView& view,
	                             const Map& map,
	                             const BaseMap* secondary_map,
	                             const DrawingOptions& options,
	                             const AtlasManager& atlas);

	void drawFloorBadges(SpriteBatch& sprite_batch,
	                     int z,
	                     const RenderView& view,
	                     const Map& map,
	                     const BaseMap* secondary_map,
	                     const DrawingOptions& options,
	                     const AtlasManager& atlas);

private:
	ZoneClusterFinder cluster_finder_;

	struct CachedRowTile {
		const Tile* tile = nullptr;
		bool is_pz = false;
		bool is_nopvp = false;
		bool is_nolog = false;
		bool is_pvp = false;
	};

	struct PendingZoneQuad {
		float x = 0.0f;
		float y = 0.0f;
		uint32_t flags = 0;
	};

	struct PendingSpawnQuad {
		float x = 0.0f;
		float y = 0.0f;
		float w = 0.0f;
		float h = 0.0f;
		float alpha = 0.0f;
		uint32_t flags = 0;
	};

	struct PendingBorderQuad {
		float x = 0.0f;
		float y = 0.0f;
		uint32_t flags = 0;
	};

	struct PendingHighlightQuad {
		float x = 0.0f;
		float y = 0.0f;
		uint32_t flags = 0;
	};

	// Contiguous reusable buffers across render frames (DOD)
	std::vector<CachedRowTile> row_prev_;
	std::vector<CachedRowTile> row_curr_;
	std::vector<CachedRowTile> row_next_;
	std::vector<VisibleZoneTile> visible_zone_tiles_;
	std::vector<PendingZoneQuad> alpha_zone_quads_;
	std::vector<PendingZoneQuad> mult_zone_quads_;
	std::vector<PendingZoneQuad> border_zone_quads_;

	std::vector<PendingSpawnQuad> alpha_spawn_quads_;
	std::vector<PendingSpawnQuad> mult_spawn_quads_;
	std::vector<PendingSpawnQuad> spawn_borders_;
	std::vector<PendingSpawnQuad> spawn_badges_;

	std::vector<uint8_t> blocking_row_prev_;
	std::vector<uint8_t> blocking_row_curr_;
	std::vector<uint8_t> blocking_row_next_;
	std::vector<PendingBorderQuad> blocking_border_quads_;

	std::vector<uint32_t> house_row_prev_;
	std::vector<uint32_t> house_row_curr_;
	std::vector<uint32_t> house_row_next_;
	std::vector<PendingBorderQuad> house_border_quads_;

	std::vector<PendingHighlightQuad> highlight_quads_;
};

} // namespace rme::rendering

#endif // RME_RENDERING_DRAWERS_OVERLAYS_ZONE_OVERLAY_DRAWER_H_
