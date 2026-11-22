#ifndef RME_LASSO_OVERLAY_DRAWER_H_
#define RME_LASSO_OVERLAY_DRAWER_H_

#include "rendering/ui/lasso_gesture.h"

#include <span>

struct NVGcontext;

// Sizes in canvas pixels; the caller converts from DIPs so the outline follows DPI, not map zoom.
struct LassoOverlayStyle {
	float line_width = 2.0f;
	float handle_radius = 3.0f;
};

// Draws the active lasso in screen space with NanoVG: translucent fill (3+ points), open polyline,
// rubber band from the last point to the cursor, and vertex handles. Theme colors, no allocations.
class LassoOverlayDrawer {
public:
	void draw(NVGcontext* vg, std::span<const LassoScreenPoint> points, LassoScreenPoint cursor, const LassoOverlayStyle& style) const;
};

#endif
