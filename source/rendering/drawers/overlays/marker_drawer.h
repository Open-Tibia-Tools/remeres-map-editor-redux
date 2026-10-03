#ifndef RME_MARKER_DRAWER_H_
#define RME_MARKER_DRAWER_H_

#include "rendering/core/drawing_options.h"
#include <cstdint>

class Tile;
class Waypoint;

class SpriteBatch;
struct RenderFrameContext;

class MarkerDrawer {
public:
	MarkerDrawer();
	~MarkerDrawer();

	void draw(SpriteBatch& sprite_batch, int draw_x, int draw_y, const Tile* tile, const Waypoint* waypoint, const DrawingOptions& options, const RenderFrameContext& ctx, float depth = 0.0f);
};

#endif
