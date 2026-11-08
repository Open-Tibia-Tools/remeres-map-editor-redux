#ifndef RME_RENDERING_DRAWERS_OVERLAYS_SERVER_MARKER_DRAWER_H_
#define RME_RENDERING_DRAWERS_OVERLAYS_SERVER_MARKER_DRAWER_H_

#include "game/srv_markers.h"

#include <string_view>

struct NVGcontext;
class Map;
struct RenderView;
struct DrawingOptions;

namespace rme::rendering {

	// NanoVG overlay for the read-only server markers of sector maps (see
	// game/srv_markers.h): a badge on each marked tile, as the tile indicators
	// of towns and waypoints have, and the area a raid's monsters can appear
	// in. Each kind is shown on its own (DrawingOptions::server_marker_kinds).
	// The markers are few and sparse, so they are drawn here rather than in
	// the tile shaders, whose indicator slots are all taken.
	class ServerMarkerDrawer {
	public:
		void draw(NVGcontext* vg, const Map& map, const RenderView& view, const DrawingOptions& options) const;
	};

	// The badge of `kind` on a `size` x `size` square at (x, y); the map and
	// the marker tooltips draw the same one.
	void drawServerMarkerBadge(NVGcontext* vg, rme::srv::MarkerKind kind, float x, float y, float size);

	// The server file a marker kind comes from, for its tooltip.
	[[nodiscard]] std::string_view serverMarkerSource(rme::srv::MarkerKind kind);

} // namespace rme::rendering

#endif
