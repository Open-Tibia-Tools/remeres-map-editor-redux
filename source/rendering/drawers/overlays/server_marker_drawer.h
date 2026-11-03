#ifndef RME_RENDERING_DRAWERS_OVERLAYS_SERVER_MARKER_DRAWER_H_
#define RME_RENDERING_DRAWERS_OVERLAYS_SERVER_MARKER_DRAWER_H_

struct NVGcontext;
class Map;
struct RenderView;
struct DrawingOptions;

namespace rme::rendering {

	// NanoVG overlay for the read-only server markers of sector maps (see
	// game/srv_markers.h): a frame and a small badge on each marked tile, and
	// the area a raid's monsters can appear in. The markers are few and
	// sparse, so they are drawn here rather than in the tile shaders.
	class ServerMarkerDrawer {
	public:
		void draw(NVGcontext* vg, const Map& map, const RenderView& view, const DrawingOptions& options) const;
	};

} // namespace rme::rendering

#endif
