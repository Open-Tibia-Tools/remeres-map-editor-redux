#ifndef RME_RENDERING_DRAWERS_OVERLAYS_ZONE_OVERLAY_DRAWER_H_
#define RME_RENDERING_DRAWERS_OVERLAYS_ZONE_OVERLAY_DRAWER_H_

class SpriteBatch;
struct RenderView;
class Map;
struct DrawingOptions;
class AtlasManager;

namespace rme::rendering {

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

	void draw(SpriteBatch& sprite_batch,
	          const RenderView& view,
	          const Map& map,
	          const DrawingOptions& options,
	          const AtlasManager& atlas);
};

} // namespace rme::rendering

#endif // RME_RENDERING_DRAWERS_OVERLAYS_ZONE_OVERLAY_DRAWER_H_
