#include "rendering/drawers/overlays/marker_label_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/indicators/zone_colors.h"
#include "brushes/managers/brush_manager.h"
#include "brushes/zone/zone_brush.h"
#include "map/map.h"
#include "game/waypoints.h"
#include "game/town.h"
#include "game/house.h"
#include <nanovg.h>
#include "util/phase_profiler.h"
#include <string>
#include <deque>
#include <vector>
#include <unordered_map>
#include <format>
#include <cmath>
#include <algorithm>
#include <limits>

namespace rme::rendering {

MarkerLabelDrawer::MarkerLabelDrawer() = default;
MarkerLabelDrawer::~MarkerLabelDrawer() = default;

void MarkerLabelDrawer::draw(NVGcontext* /*vg*/, const Map& /*map*/, const RenderView& /*view*/, const DrawingOptions& /*options*/) {
	// Legacy pill labels for Waypoints, Towns, Houses, and Zones have been replaced by standardized NanoVG tooltip cards
}

} // namespace rme::rendering
