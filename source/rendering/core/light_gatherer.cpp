//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/light_gatherer.h"
#include "app/definitions.h"
#include "map/map.h"
#include "map/map_region.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "game/creature.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include <algorithm>
#include <ranges>

namespace {
	[[nodiscard]] int projectedFloorOffsetTiles(const RenderView& view, int map_z) noexcept {
		if (map_z <= GROUND_LAYER) {
			return GROUND_LAYER - map_z;
		}
		return view.floor - map_z;
	}

	[[nodiscard]] bool tileCarriesTranslucentLight(const Tile* tile) {
		if (!tile) {
			return false;
		}
		if (tile->ground && (tile->ground->isTranslucent() || tile->ground->hasLensHelp())) {
			return true;
		}
		return std::ranges::any_of(tile->items, [](const std::unique_ptr<Item>& item) {
			return item && (item->isTranslucent() || item->hasLensHelp());
		});
	}
}

void LightGatherer::Gather(
	const Map& map,
	const RenderView& view,
	const DrawingOptions& options,
	LightBuffer& light_buffer
) {
	if (!options.isDrawLight()) {
		return;
	}

	// Maximum light radius in Tibia is ~10 tiles, add safety margin
	constexpr int kLightMarginTiles = 12;

	for (int map_z = view.start_z; map_z >= view.superend_z; --map_z) {
		light_buffer.SetFloorLightStart();

		const ViewBounds floor_bounds = view.getBoundsForFloor(map_z);
		const int offset_tiles = projectedFloorOffsetTiles(view, map_z);

		const int safe_start_x = floor_bounds.start_x - kLightMarginTiles;
		const int safe_start_y = floor_bounds.start_y - kLightMarginTiles;
		const int safe_end_x = floor_bounds.end_x + kLightMarginTiles;
		const int safe_end_y = floor_bounds.end_y + kLightMarginTiles;

		map.visitLeaves(safe_start_x, safe_start_y, safe_end_x, safe_end_y, [&](const MapNode* nd, int, int) {
			const Floor* floor = nd->getFloor(map_z);
			if (!floor) {
				return;
			}

			const Floor* floor_above = (map_z == GROUND_LAYER + 1) ? nd->getFloor(GROUND_LAYER) : nullptr;

			for (int idx = 0; idx < SpatialHashGrid::TILES_PER_NODE; ++idx) {
				const TileLocation* location = &floor->locs[idx];
				const Tile* tile = location->get();
				if (!tile) {
					continue;
				}

				const Position& pos = location->getPosition();
				const int proj_x = pos.x - offset_tiles;
				const int proj_y = pos.y - offset_tiles;

				// Ground occlusion & light
				if (tile->ground) {
					if (tile->ground->blocksLightFromBelow()) {
						light_buffer.SetFieldBrightness(proj_x, proj_y, light_buffer.current_floor_light_start);
					}
					if (tile->ground->hasLight()) {
						light_buffer.AddTileLight(proj_x, proj_y, tile->ground->getLight());
					}
				}

				// Translucent light from floor above (floor 7 to 8)
				if (map_z == GROUND_LAYER + 1) {
					const Tile* above_tile = floor_above ? floor_above->locs[idx].get() : nullptr;
					if (tileCarriesTranslucentLight(above_tile)) {
						light_buffer.AddTileLight(proj_x, proj_y, SpriteLight {
							.intensity = 1,
							.color = 215
						});
					}
				}

				// Item lights
				for (const auto& item : tile->items) {
					if (item && item->hasLight()) {
						light_buffer.AddTileLight(proj_x, proj_y, item->getLight());
					}
				}
			}
		});
	}
}
