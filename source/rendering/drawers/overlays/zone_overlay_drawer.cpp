#include "rendering/drawers/overlays/zone_overlay_drawer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/atlas_manager.h"
#include "rendering/indicators/zone_flags.h"
#include "rendering/indicators/technical_item_registry.h"
#include "map/map.h"
#include "map/basemap.h"
#include "map/tile.h"
#include "game/spawn.h"
#include <algorithm>
#include <vector>

namespace rme::rendering {

void ZoneOverlayDrawer::draw(SpriteBatch& sprite_batch,
                             const RenderView& view,
                             const Map& map,
                             const BaseMap* secondary_map,
                             const DrawingOptions& options,
                             const AtlasManager& atlas) {
	if (options.ingame) {
		return;
	}

	if (!options.show_special_tiles && !options.show_blocking && !options.show_spawns) {
		return;
	}

	const AtlasRegion* white_pixel = atlas.getWhitePixel();
	if (!white_pixel) {
		return;
	}

	auto isPathBlocking = [](const Tile* t) -> bool {
		return t && t->isBlocking() && (t->ground != nullptr || !t->items.empty());
	};

	auto sameZone = [](const Tile* t, bool pz, bool nopvp, bool nolog, bool pvp) -> bool {
		if (!t) return false;
		if (pz) return t->isPZ();
		if (nopvp) return (t->getMapFlags() & TILESTATE_NOPVP) != 0;
		if (nolog) return (t->getMapFlags() & TILESTATE_NOLOGOUT) != 0;
		if (pvp) return (t->getMapFlags() & TILESTATE_PVPZONE) != 0;
		return false;
	};

	const int start_z = options.transparent_floors ? view.start_z : view.floor;
	const int end_z = options.transparent_floors ? view.superend_z : view.floor;

	for (int z = start_z; z >= end_z; --z) {
		const ViewBounds bounds = view.getBoundsForFloor(z);
		const float floor_alpha = (z == view.floor) ? 1.0f : std::max(0.25f, 1.0f - static_cast<float>(view.floor - z) * 0.20f);

		// 1. Special Zones & Pathing / Blocking Pass
		if (options.show_special_tiles || options.show_blocking) {
			const int min_x = bounds.start_x - 1;
			const int max_x = bounds.end_x + 1;
			const int row_width = max_x - min_x + 1;

			auto fetchRow = [&](int ry, std::vector<const Tile*>& row) {
				row.resize(row_width);
				for (int x = min_x; x <= max_x; ++x) {
					const Tile* t = nullptr;
					if (secondary_map) {
						t = secondary_map->getTile(x, ry, z);
					}
					if (!t) {
						t = map.getTile(x, ry, z);
					}
					row[x - min_x] = t;
				}
			};

			std::vector<const Tile*> row_prev;
			std::vector<const Tile*> row_curr;
			std::vector<const Tile*> row_next;

			fetchRow(bounds.start_y - 1, row_prev);
			fetchRow(bounds.start_y, row_curr);

			for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
				fetchRow(y + 1, row_next);

				for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
					const int idx = x - min_x;
					const Tile* tile = row_curr[idx];
					if (!tile) {
						continue;
					}

					uint32_t tile_zone_flags = 0;

					// Pathing / Blocking
					if (options.show_blocking && isPathBlocking(tile)) {
						tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCKING);
						if (!isPathBlocking(row_prev[idx]))     tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_N);
						if (!isPathBlocking(row_next[idx]))     tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_S);
						if (!isPathBlocking(row_curr[idx - 1])) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_W);
						if (!isPathBlocking(row_curr[idx + 1])) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_E);
					}

					// Special Zones (PZ, No-PvP, No-Logout, PvP Zone)
					if (options.show_special_tiles) {
						const bool has_pz = tile->isPZ();
						const bool has_nopvp = (tile->getMapFlags() & TILESTATE_NOPVP) != 0;
						const bool has_nolog = (tile->getMapFlags() & TILESTATE_NOLOGOUT) != 0;
						const bool has_pvp = (tile->getMapFlags() & TILESTATE_PVPZONE) != 0;

						if (has_pz) {
							tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PZ);
						} else if (has_nopvp) {
							tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOPVP);
						} else if (has_nolog) {
							tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOLOGOUT);
						} else if (has_pvp) {
							tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PVPZONE);
						}

						if (has_pz || has_nopvp || has_nolog || has_pvp) {
							if (!sameZone(row_prev[idx], has_pz, has_nopvp, has_nolog, has_pvp))     tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_N);
							if (!sameZone(row_next[idx], has_pz, has_nopvp, has_nolog, has_pvp))     tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_S);
							if (!sameZone(row_curr[idx - 1], has_pz, has_nopvp, has_nolog, has_pvp)) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_W);
							if (!sameZone(row_curr[idx + 1], has_pz, has_nopvp, has_nolog, has_pvp)) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_E);
						}
					}

					if (tile_zone_flags > 0) {
						int draw_x, draw_y;
						view.getScreenPosition(x, y, z, draw_x, draw_y);
						sprite_batch.draw(static_cast<float>(draw_x), static_cast<float>(draw_y),
						                  32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
						                  0.0f, static_cast<float>(tile_zone_flags));
					}
				}

				row_prev = std::move(row_curr);
				row_curr = std::move(row_next);
			}
		}

		// 2. Spawns Pass
		if (options.show_spawns) {
			for (const Position& spos : map.spawns) {
				if (spos.z != z) {
					continue;
				}
				const Tile* st = map.getTile(spos);
				if (!st || !st->spawn) {
					continue;
				}
				const int radius = st->spawn->getSize();
				const int sx0 = spos.x - radius;
				const int sx1 = spos.x + radius;
				const int sy0 = spos.y - radius;
				const int sy1 = spos.y + radius;

				// Viewport intersection test
				if (sx1 < bounds.start_x || sx0 > bounds.end_x ||
				    sy1 < bounds.start_y || sy0 > bounds.end_y) {
					continue;
				}

				// Calculate screen bounds for spawn rectangle
				int draw_x0, draw_y0;
				view.getScreenPosition(sx0, sy0, z, draw_x0, draw_y0);
				const float spawn_w = static_cast<float>((sx1 - sx0 + 1) * 32);
				const float spawn_h = static_cast<float>((sy1 - sy0 + 1) * 32);

				const uint32_t spawn_flags = static_cast<uint32_t>(ZONE_FLAG_SPAWN) |
				                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_N) |
				                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_S) |
				                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_W) |
				                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_E);

				const float box_alpha = (st->spawn->isSelected() && options.dragging) ? (floor_alpha * 0.30f) : floor_alpha;
				sprite_batch.draw(static_cast<float>(draw_x0), static_cast<float>(draw_y0),
				                  spawn_w, spawn_h, *white_pixel, 1.0f, 1.0f, 1.0f, box_alpha,
				                  0.0f, static_cast<float>(spawn_flags));

				// Emit spawn center badge (32x32) with "SPAWN" text
				int center_draw_x, center_draw_y;
				view.getScreenPosition(spos.x, spos.y, z, center_draw_x, center_draw_y);
				sprite_batch.draw(static_cast<float>(center_draw_x), static_cast<float>(center_draw_y),
				                  32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, box_alpha,
				                  INDICATOR_SPAWN_BASE, 0.0f);
			}
		}
	}
}

} // namespace rme::rendering
