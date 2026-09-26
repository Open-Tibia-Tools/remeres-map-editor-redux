#include "rendering/drawers/overlays/zone_overlay_drawer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/atlas_manager.h"
#include "rendering/indicators/zone_flags.h"
#include "rendering/indicators/technical_item_registry.h"
#include "map/map.h"
#include "map/tile.h"

namespace rme::rendering {

void ZoneOverlayDrawer::draw(SpriteBatch& sprite_batch,
                             const RenderView& view,
                             const Map& map,
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

	const int z = view.floor;
	const ViewBounds bounds = view.getBoundsForFloor(z);

	auto isPathBlocking = [](const Tile* t) -> bool {
		return t && t->isBlocking() && (t->ground != nullptr || !t->items.empty());
	};

	// 1. Special Zones & Pathing / Blocking Pass (single 32x32 quad per tile)
	if (options.show_special_tiles || options.show_blocking) {
		for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
			for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
				const Tile* tile = map.getTile(x, y, z);
				if (!tile) {
					continue;
				}

				uint32_t tile_zone_flags = 0;

				// Pathing / Blocking
				if (options.show_blocking && isPathBlocking(tile)) {
					tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCKING);
					if (!isPathBlocking(map.getTile(x, y - 1, z))) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_N);
					if (!isPathBlocking(map.getTile(x, y + 1, z))) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_S);
					if (!isPathBlocking(map.getTile(x - 1, y, z))) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_W);
					if (!isPathBlocking(map.getTile(x + 1, y, z))) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_E);
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
						const auto sameZone = [&](const Tile* other) {
							if (!other) return false;
							if (has_pz) return other->isPZ();
							if (has_nopvp) return (other->getMapFlags() & TILESTATE_NOPVP) != 0;
							if (has_nolog) return (other->getMapFlags() & TILESTATE_NOLOGOUT) != 0;
							if (has_pvp) return (other->getMapFlags() & TILESTATE_PVPZONE) != 0;
							return false;
						};
						if (!sameZone(map.getTile(x, y - 1, z))) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_N);
						if (!sameZone(map.getTile(x, y + 1, z))) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_S);
						if (!sameZone(map.getTile(x - 1, y, z))) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_W);
						if (!sameZone(map.getTile(x + 1, y, z))) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_E);
					}
				}

				if (tile_zone_flags > 0) {
					int draw_x, draw_y;
					view.getScreenPosition(x, y, z, draw_x, draw_y);
					sprite_batch.draw(static_cast<float>(draw_x), static_cast<float>(draw_y),
					                  32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f,
					                  0.0f, static_cast<float>(tile_zone_flags));
				}
			}
		}
	}

	// 2. Spawns Pass: Individual bounding rectangles with independent outer borders + center badges
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

			// Emit spawn box quad with all 4 borders enabled
			const uint32_t spawn_flags = static_cast<uint32_t>(ZONE_FLAG_SPAWN) |
			                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_N) |
			                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_S) |
			                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_W) |
			                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_E);

			sprite_batch.draw(static_cast<float>(draw_x0), static_cast<float>(draw_y0),
			                  spawn_w, spawn_h, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f,
			                  0.0f, static_cast<float>(spawn_flags));

			// Emit spawn center badge (32x32) with "SPAWN" text
			int center_draw_x, center_draw_y;
			view.getScreenPosition(spos.x, spos.y, z, center_draw_x, center_draw_y);
			sprite_batch.draw(static_cast<float>(center_draw_x), static_cast<float>(center_draw_y),
			                  32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f,
			                  INDICATOR_SPAWN_BASE, 0.0f);
		}
	}
}

} // namespace rme::rendering
