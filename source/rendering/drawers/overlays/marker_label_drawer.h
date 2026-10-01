#ifndef RME_RENDERING_DRAWERS_OVERLAYS_MARKER_LABEL_DRAWER_H_
#define RME_RENDERING_DRAWERS_OVERLAYS_MARKER_LABEL_DRAWER_H_

#include <nanovg.h>

class Map;
struct RenderView;
struct DrawingOptions;

namespace rme::rendering {

/**
 * @brief NanoVG overlay drawer for Waypoint, Town Temple, and House Name labels.
 *
 * Renders concise typographic badges floating above WAYPT and TOWN tiles, and centered
 * in House rooms with their House Name, when their view options are active and in-game mode is off.
 */
class MarkerLabelDrawer {
public:
	MarkerLabelDrawer();
	~MarkerLabelDrawer();

	void draw(NVGcontext* vg, const Map& map, const RenderView& view, const DrawingOptions& options);
};

} // namespace rme::rendering

#endif // RME_RENDERING_DRAWERS_OVERLAYS_MARKER_LABEL_DRAWER_H_
