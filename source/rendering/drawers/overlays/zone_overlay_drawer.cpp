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
#include "game/item.h"
#include "game/spawn.h"
#include <algorithm>
#include <vector>

namespace rme::rendering {

bool IsTilePathBlocking(const Tile* t) noexcept {
	if (!t || !t->isBlocking() || (!t->ground && t->items.empty())) {
		return false;
	}

	auto isInvisibleWall = [](uint32_t sid, uint32_t cid) noexcept -> bool {
		if (sid == 1548 || cid == 2187) {
			return true;
		}
		return TechnicalItemRegistry::Classify(sid, cid) == TileIndicatorType::TechInvisibleWall;
	};

	if (t->ground && t->ground->isBlocking()) {
		if (!isInvisibleWall(t->ground->getID(), t->ground->getClientID())) {
			return true;
		}
	}
	for (const auto& item : t->items) {
		if (item && item->isBlocking()) {
			if (!isInvisibleWall(item->getID(), item->getClientID())) {
				return true;
			}
		}
	}
	return false;
}

void ZoneOverlayDrawer::drawFloor(SpriteBatch& sprite_batch,
                                  int z,
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

	auto sameZone = [](const Tile* t, bool pz, bool nopvp, bool nolog, bool pvp) -> bool {
		if (!t) return false;
		const bool t_pz = t->isPZ();
		const bool t_nopvp = (t->getMapFlags() & TILESTATE_NOPVP) != 0;
		const bool t_nolog = (t->getMapFlags() & TILESTATE_NOLOGOUT) != 0;
		const bool t_pvp = (t->getMapFlags() & TILESTATE_PVPZONE) != 0;
		return (t_pz == pz) && (t_nopvp == nopvp) && (t_nolog == nolog) && (t_pvp == pvp);
	};

	const ViewBounds bounds = view.getBoundsForFloor(z);
	const float floor_alpha = (z == view.floor) ? 1.0f : std::max(0.25f, 1.0f - static_cast<float>(view.floor - z) * 0.20f);

	std::vector<const Tile*> row_prev;
	std::vector<const Tile*> row_curr;
	std::vector<const Tile*> row_next;

	// 1. Special Zones & Pathing / Blocking Pass
	if ((options.show_special_tiles || options.show_blocking) && view.zoom <= 10.0f) {
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
				if (options.show_blocking && IsTilePathBlocking(tile)) {
					tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCKING);
					if (!IsTilePathBlocking(row_prev[idx]))     tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_N);
					if (!IsTilePathBlocking(row_next[idx]))     tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_S);
					if (!IsTilePathBlocking(row_curr[idx - 1])) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_W);
					if (!IsTilePathBlocking(row_curr[idx + 1])) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_E);
				}

				// Special Zones (PZ, No-PvP, No-Logout, PvP Zone)
				if (options.show_special_tiles) {
					const bool has_pz = tile->isPZ();
					const bool has_nopvp = (tile->getMapFlags() & TILESTATE_NOPVP) != 0;
					const bool has_nolog = (tile->getMapFlags() & TILESTATE_NOLOGOUT) != 0;
					const bool has_pvp = (tile->getMapFlags() & TILESTATE_PVPZONE) != 0;

					if (has_pz)    tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PZ);
					if (has_nopvp) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOPVP);
					if (has_nolog) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOLOGOUT);
					if (has_pvp)   tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PVPZONE);

					if (has_pz || has_nopvp || has_nolog || has_pvp) {
						auto checkNeighbor = [&](const Tile* neighbor, uint32_t global_bit, uint32_t internal_bit) {
							if (!neighbor || (!neighbor->isPZ() && (neighbor->getMapFlags() & (TILESTATE_NOPVP | TILESTATE_NOLOGOUT | TILESTATE_PVPZONE)) == 0)) {
								tile_zone_flags |= global_bit;
							} else if (!sameZone(neighbor, has_pz, has_nopvp, has_nolog, has_pvp)) {
								tile_zone_flags |= internal_bit;
							}
						};

						checkNeighbor(row_prev[idx],     static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_N), static_cast<uint32_t>(ZONE_FLAG_ZONE_INTERNAL_N));
						checkNeighbor(row_next[idx],     static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_S), static_cast<uint32_t>(ZONE_FLAG_ZONE_INTERNAL_S));
						checkNeighbor(row_curr[idx - 1], static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_W), static_cast<uint32_t>(ZONE_FLAG_ZONE_INTERNAL_W));
						checkNeighbor(row_curr[idx + 1], static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_E), static_cast<uint32_t>(ZONE_FLAG_ZONE_INTERNAL_E));
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

			std::swap(row_prev, row_curr);
			std::swap(row_curr, row_next);
		}
	}

	// 2. Fixed World Center Cluster Badges Pass
	if (options.show_special_tiles && view.zoom <= 10.0f) {
		const uint64_t cur_gen = map.getChangeTracker().getGeneration();
		const auto& badges = cluster_finder_.getVisibleBadges(z, bounds, map, secondary_map, cur_gen);
		for (const auto& badge : badges) {
			int draw_x, draw_y;
			view.getScreenPosition(badge.center_x, badge.center_y, z, draw_x, draw_y);
			const float px = static_cast<float>(draw_x) + 16.0f - (badge.width * 0.5f) + badge.offset_x;
			const float py = static_cast<float>(draw_y) + 16.0f - (badge.height * 0.5f) + badge.offset_y;
			sprite_batch.draw(px, py, badge.width, badge.height, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
			                  0.0f, static_cast<float>(static_cast<uint32_t>(ZONE_FLAG_CLUSTER_BADGE) | badge.zone_flag));
		}
	}

	// 3. Spawns Pass
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

void ZoneOverlayDrawer::draw(SpriteBatch& sprite_batch,
                             const RenderView& view,
                             const Map& map,
                             const BaseMap* secondary_map,
                             const DrawingOptions& options,
                             const AtlasManager& atlas) {
	if (options.ingame) {
		return;
	}

	const int start_z = options.transparent_floors ? view.start_z : view.floor;
	const int end_z = options.transparent_floors ? view.superend_z : view.floor;

	for (int z = start_z; z >= end_z; --z) {
		drawFloor(sprite_batch, z, view, map, secondary_map, options, atlas);
	}
}

} // namespace rme::rendering
