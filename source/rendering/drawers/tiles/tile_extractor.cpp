#include "rendering/drawers/tiles/tile_extractor.h"
#include "rendering/drawers/tiles/tile_renderer.h"
#include "rendering/core/render_frame_context.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/sprite_batch.h"
#include "map/basemap.h"
#include "map/map_region.h"

namespace rme::rendering {

void TileExtractor::ExtractChunk(
	RenderChunk& chunk,
	BaseMap& map,
	const RenderFrameContext& ctx,
	const TileRenderer& tile_renderer
) {
	const int start_x = chunk.chunk_x;
	const int start_y = chunk.chunk_y;
	const int end_x = start_x + CHUNK_SIZE_TILES;
	const int end_y = start_y + CHUNK_SIZE_TILES;
	const int floor_z = chunk.floor;

	// Fast pre-check: if no map nodes exist in this chunk, mark clean and return empty
	bool has_any_leaf = false;
	for (int nd_x = start_x; nd_x < end_x; nd_x += 4) {
		for (int nd_y = start_y; nd_y < end_y; nd_y += 4) {
			if (map.getLeaf(nd_x, nd_y)) {
				has_any_leaf = true;
				break;
			}
		}
		if (has_any_leaf) {
			break;
		}
	}

	chunk.clear();

	if (!has_any_leaf) {
		chunk.dirty = false;
		return;
	}

	SpriteBatch ground_batch;
	SpriteBatch item_batch;
	ground_batch.beginExtraction(ctx.atlas);
	item_batch.beginExtraction(ctx.atlas);

	// Static quad extraction: disable tooltips and dynamic creatures in cached chunk
	DrawingOptions extraction_options = ctx.options;
	extraction_options.show_tooltips = false;
	extraction_options.show_creatures = false;

	RenderFrameContext extraction_ctx {
		.atlas = ctx.atlas,
		.gfx = ctx.gfx,
		.item_definitions = ctx.item_definitions,
		.options = extraction_options,
		.view = ctx.view,
		.elapsed_time = ctx.elapsed_time,
		.current_house_id = ctx.current_house_id
	};

	for (int nd_x = start_x; nd_x < end_x; nd_x += 4) {
		for (int nd_y = start_y; nd_y < end_y; nd_y += 4) {
			MapNode* nd = map.getLeaf(nd_x, nd_y);
			if (!nd) {
				continue;
			}

			Floor* floor = nd->getFloor(floor_z);
			if (!floor) {
				continue;
			}

			TileLocation* location = floor->locs.data();
			for (int map_x = 0; map_x < 4; ++map_x) {
				const int world_x = (nd_x + map_x) * TILE_SIZE;
				for (int map_y = 0; map_y < 4; ++map_y, ++location) {
					if (!location->get()) {
						continue;
					}
					const int world_y = (nd_y + map_y) * TILE_SIZE;

					tile_renderer.DrawTile(
						ground_batch,
						item_batch,
						location,
						extraction_ctx,
						world_x,
						world_y,
						nullptr,
						false
					);
				}
			}
		}
	}

	chunk.ground_sprites = ground_batch.takePendingSprites();
	chunk.item_sprites = item_batch.takePendingSprites();
	chunk.dirty = false;
}

} // namespace rme::rendering
