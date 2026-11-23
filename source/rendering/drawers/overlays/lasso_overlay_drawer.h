//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_LASSO_OVERLAY_DRAWER_H_
#define RME_LASSO_OVERLAY_DRAWER_H_

#include "rendering/ui/lasso_gesture.h"

#include <cstdint>
#include <span>

struct NVGcontext;

// Sizes in canvas pixels; the caller converts from DIPs so the outline follows DPI, not map zoom.
struct LassoOverlayStyle {
	float line_width = 2.0f;
	float handle_radius = 3.0f;
};

// Draws the active lasso in screen space with NanoVG: open polyline, rubber band from the last
// point to the cursor, and vertex handles. No fill: the fill falsely suggested a tile selection.
// Theme colors, no allocations.
class LassoOverlayDrawer {
public:
	// Handles are drawn only for click-placed vertices; a freehand lasso passes an empty span.
	void draw(NVGcontext* vg, std::span<const LassoScreenPoint> points, std::span<const std::uint8_t> handle_vertices, LassoScreenPoint cursor, const LassoOverlayStyle& style) const;
};

#endif
