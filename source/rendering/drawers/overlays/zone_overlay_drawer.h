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

	void draw(SpriteBatch& sprite_batch,
	          const RenderView& view,
	          const Map& map,
	          const BaseMap* secondary_map,
	          const DrawingOptions& options,
	          const AtlasManager& atlas);

	void invalidateClusters() noexcept {
		cluster_finder_.invalidate();
	}

private:
	ZoneClusterFinder cluster_finder_;
};

} // namespace rme::rendering

#endif // RME_RENDERING_DRAWERS_OVERLAYS_ZONE_OVERLAY_DRAWER_H_
