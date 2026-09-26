#include "rendering/drawers/overlays/marker_drawer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/indicators/technical_item_registry.h"
#include "map/tile.h"
#include "game/spawn.h"
#include "rendering/core/atlas_manager.h"
#include "rendering/core/render_frame_context.h"

MarkerDrawer::MarkerDrawer() = default;
MarkerDrawer::~MarkerDrawer() = default;

void MarkerDrawer::draw(SpriteBatch& sprite_batch, SpriteDrawer* /*drawer*/, int draw_x, int draw_y, const Tile* tile, const Waypoint* waypoint, uint32_t current_house_id, Map& map, const DrawingOptions& options, const RenderFrameContext& ctx) {
	const AtlasRegion* white_pixel = ctx.atlas.getWhitePixel();
	if (!white_pixel || !tile) {
		return;
	}

	const float fx = static_cast<float>(draw_x);
	const float fy = static_cast<float>(draw_y);

	// House entry ("ENTRY")
	if (options.show_houses && tile->isHouseExit()) {
		const HouseExitList* exits = tile->getHouseExits();
		const uint32_t exit_house_id = (exits && !exits->empty()) ? exits->front() : 1;
		sprite_batch.draw(fx, fy, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f,
			rme::rendering::INDICATOR_HOUSE_ENTRY_BASE + static_cast<float>(exit_house_id));
	}

	// Town temple ("TOWN")
	if (options.show_towns && tile->isTownExit()) {
		sprite_batch.draw(fx, fy, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f,
			rme::rendering::INDICATOR_TOWN_BASE);
	}

	// Waypoint ("WAYPT")
	if (!options.ingame && options.show_waypoints && waypoint) {
		sprite_batch.draw(fx, fy, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f,
			rme::rendering::INDICATOR_WAYPOINT_BASE);
	}
}
