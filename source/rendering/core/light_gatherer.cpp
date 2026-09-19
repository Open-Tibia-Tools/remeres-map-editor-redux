//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/light_gatherer.h"
#include "app/definitions.h"
#include "map/basemap.h"
#include "map/map_region.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "game/creature.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/graphics.h"
#include "rendering/core/game_sprite.h"
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

LightGatherer::LightGatherer() {
	lights_.reserve(128);
}

LightGatherer::~LightGatherer() = default;

void LightGatherer::gatherForChunk(
	const BaseMap& map,
	int32_t cx, int32_t cy, int32_t target_z,
	int32_t start_floor, int32_t superend_floor,
	GraphicManager& gfx
) {
	lights_.clear();

	for (int map_z = start_floor; map_z >= superend_floor; --map_z) {
		int offset = 0;
		if (map_z <= GROUND_LAYER) {
			offset = GROUND_LAYER - map_z;
		} else if (map_z < target_z) {
			offset = target_z - map_z;
		}

		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				const int n_cx = cx + dx;
				const int n_cy = cy + dy;
				const int tile_start_x = n_cx * rme::lighting::CHUNK_SIZE;
				const int tile_start_y = n_cy * rme::lighting::CHUNK_SIZE;

				map.visitLeaves(tile_start_x, tile_start_y, tile_start_x + rme::lighting::CHUNK_SIZE, tile_start_y + rme::lighting::CHUNK_SIZE,
					[&](const MapNode* nd, int, int) {
						const Floor* floor = nd->getFloor(map_z);
						if (!floor) return;

						const Floor* floor_above = (map_z == GROUND_LAYER + 1) ? nd->getFloor(GROUND_LAYER) : nullptr;

						for (int idx = 0; idx < SpatialHashGrid::TILES_PER_NODE; ++idx) {
							const TileLocation* loc = &floor->locs[idx];
							const Tile* tile = loc->get();
							if (!tile) continue;

							const Position& pos = loc->getPosition();
							const int proj_x = pos.x - offset;
							const int proj_y = pos.y - offset;

							// 1. Ground item light
							if (tile->ground && tile->ground->hasLight()) {
								const auto l = tile->ground->getLight();
								if (l.intensity > 0) {
									lights_.push_back(rme::lighting::LightSource{
										.x = proj_x,
										.y = proj_y,
										.floor = map_z,
										.color = static_cast<uint8_t>(l.color),
										.intensity = static_cast<uint8_t>(l.intensity)
									});
								}
							}

							// 2. Translucent sunlight from floor 7 to 8
							if (map_z == GROUND_LAYER + 1) {
								const Tile* above = floor_above ? floor_above->locs[idx].get() : nullptr;
								if (tileCarriesTranslucentLight(above)) {
									lights_.push_back(rme::lighting::LightSource{
										.x = proj_x,
										.y = proj_y,
										.floor = map_z,
										.color = 215,
										.intensity = 1
									});
								}
							}

							// 3. Static item lights
							for (const auto& item : tile->items) {
								if (item && item->hasLight()) {
									const auto l = item->getLight();
									if (l.intensity > 0) {
										lights_.push_back(rme::lighting::LightSource{
											.x = proj_x,
											.y = proj_y,
											.floor = map_z,
											.color = static_cast<uint8_t>(l.color),
											.intensity = static_cast<uint8_t>(l.intensity)
										});
									}
								}
							}

							// 4. Creature lights
							if (tile->creature) {
								GameSprite* spr = gfx.getCreatureSprite(tile->creature->getLookType().lookType);
								if (spr && spr->hasLight()) {
									const auto l = spr->getLight();
									if (l.intensity > 0) {
										lights_.push_back(rme::lighting::LightSource{
											.x = proj_x,
											.y = proj_y,
											.floor = map_z,
											.color = static_cast<uint8_t>(l.color),
											.intensity = static_cast<uint8_t>(l.intensity)
										});
									}
								}
							}
						}
					}
				);
			}
		}
	}
}

void LightGatherer::Gather(
	const BaseMap& map,
	const RenderView& view,
	const DrawingOptions& options,
	LightBuffer& light_buffer
) {
	if (!options.isDrawLight()) {
		return;
	}

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
