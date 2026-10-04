#include "rendering/drawers/overlays/marker_drawer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/indicators/technical_item_registry.h"
#include "map/tile.h"
#include "rendering/core/atlas_manager.h"
#include "rendering/core/render_frame_context.h"

MarkerDrawer::MarkerDrawer() = default;
MarkerDrawer::~MarkerDrawer() = default;

void MarkerDrawer::draw(SpriteBatch& sprite_batch, int draw_x, int draw_y, const Tile* tile, const Waypoint* waypoint, const DrawingOptions& options, const RenderFrameContext& ctx, float depth) {
	const AtlasRegion* white_pixel = ctx.atlas.getWhitePixel();
	if (!white_pixel || !tile) {
		return;
	}

	const float fx = static_cast<float>(draw_x);
	const float fy = static_cast<float>(draw_y);

	// House entry ("ENTRY")
	if (options.show_houses && tile->isHouseExit()) {
		sprite_batch.draw(fx, fy, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f,
			rme::rendering::INDICATOR_HOUSE_ENTRY_BASE, 0u, depth);
	}

	// Town temple ("TOWN")
	if (options.show_towns && tile->isTownExit()) {
		sprite_batch.draw(fx, fy, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f,
			rme::rendering::INDICATOR_TOWN_BASE, 0u, depth);
	}

	// Waypoint ("WAYPT")
	if (!options.ingame && options.show_waypoints && waypoint) {
		sprite_batch.draw(fx, fy, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f,
			rme::rendering::INDICATOR_WAYPOINT_BASE, 0u, depth);
	}
}
