#ifndef RME_RENDERING_DRAWERS_OVERLAYS_HOME_RADIUS_DRAWER_H_
#define RME_RENDERING_DRAWERS_OVERLAYS_HOME_RADIUS_DRAWER_H_

struct NVGcontext;
class Map;
class Selection;
struct RenderView;
struct DrawingOptions;

namespace rme::rendering {

	// NanoVG overlay for the radius of sector map monster and NPC homes (see
	// game/srv_homes.h): the square a home's creatures roam and respawn in,
	// for the creature under the mouse and the selected ones. Nearly every
	// home has radius 50, so drawing all of them would cover the map.
	class HomeRadiusDrawer {
	public:
		void draw(NVGcontext* vg, const Map& map, const RenderView& view, const DrawingOptions& options, const Selection& selection) const;

		// True when draw() has a home to show.
		[[nodiscard]] bool hasHomes(const Map& map, const RenderView& view, const DrawingOptions& options, const Selection& selection) const;
	};

} // namespace rme::rendering

#endif
