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
	if (!t || !t->isBlocking() || !t->ground) {
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

	if (!options.show_special_tiles && !options.show_spawns) {
		return;
	}

	const AtlasRegion* white_pixel = atlas.getWhitePixel();
	if (!white_pixel) {
		return;
	}

	const ViewBounds bounds = view.getBoundsForFloor(z);
	const float floor_alpha = 1.0f;

	struct CachedRowTile {
		const Tile* tile = nullptr;
		bool is_pz = false;
		bool is_nopvp = false;
		bool is_nolog = false;
		bool is_pvp = false;
	};

	std::vector<CachedRowTile> row_prev;
	std::vector<CachedRowTile> row_curr;
	std::vector<CachedRowTile> row_next;
	std::vector<VisibleZoneTile> visible_zone_tiles;

	// 1. Special Zones Pass (Ground level)
	if (options.show_special_tiles && view.zoom <= 10.0f) {
		struct PendingZoneQuad {
			float x, y;
			uint32_t flags;
		};
		std::vector<PendingZoneQuad> alpha_zone_quads;
		std::vector<PendingZoneQuad> mult_zone_quads;

		const int min_x = bounds.start_x - 1;
		const int max_x = bounds.end_x + 1;
		const int row_width = max_x - min_x + 1;

		auto fetchRow = [&](int ry, std::vector<CachedRowTile>& row) {
			row.resize(row_width);
			for (int x = min_x; x <= max_x; ++x) {
				const Tile* t = secondary_map ? secondary_map->getTile(x, ry, z) : nullptr;
				if (!t) {
					t = map.getTile(x, ry, z);
				}
				CachedRowTile& ct = row[x - min_x];
				ct.tile = t;
				if (t) {
					ct.is_pz = t->isPZ();
					const uint32_t mf = t->getMapFlags();
					ct.is_nopvp = (mf & TILESTATE_NOPVP) != 0;
					ct.is_nolog = (mf & TILESTATE_NOLOGOUT) != 0;
					ct.is_pvp = (mf & TILESTATE_PVPZONE) != 0;
				} else {
					ct.is_pz = false;
					ct.is_nopvp = false;
					ct.is_nolog = false;
					ct.is_pvp = false;
				}
			}
		};

		fetchRow(bounds.start_y - 1, row_prev);
		fetchRow(bounds.start_y, row_curr);

		for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
			fetchRow(y + 1, row_next);

			for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
				const int idx = x - min_x;
				const CachedRowTile& ct = row_curr[idx];
				if (!ct.tile) {
					continue;
				}

				// Special Zones (PZ, No-PvP, No-Logout, PvP Zone)
				if (ct.is_pz || ct.is_nopvp || ct.is_nolog || ct.is_pvp) {
					uint32_t tile_zone_flags = 0;
					if (ct.is_pz)    tile_zone_flags |= ZONE_FLAG_PZ;
					if (ct.is_nopvp) tile_zone_flags |= ZONE_FLAG_NOPVP;
					if (ct.is_nolog) tile_zone_flags |= ZONE_FLAG_NOLOGOUT;
					if (ct.is_pvp)   tile_zone_flags |= ZONE_FLAG_PVPZONE;

					auto checkNeighbor = [&](const CachedRowTile& n, uint32_t border_bit) {
						const bool n_has_any = n.is_pz || n.is_nopvp || n.is_nolog || n.is_pvp;
						if (!n_has_any) {
							tile_zone_flags |= border_bit;
						}
					};

					checkNeighbor(row_prev[idx],     ZONE_FLAG_ZONE_BORDER_N);
					checkNeighbor(row_next[idx],     ZONE_FLAG_ZONE_BORDER_S);
					checkNeighbor(row_curr[idx - 1], ZONE_FLAG_ZONE_BORDER_W);
					checkNeighbor(row_curr[idx + 1], ZONE_FLAG_ZONE_BORDER_E);

					uint8_t zmask = (ct.is_pz ? 1 : 0) | (ct.is_nopvp ? 2 : 0) | (ct.is_nolog ? 4 : 0) | (ct.is_pvp ? 8 : 0);
					visible_zone_tiles.push_back({ x, y, zmask });

					bool is_mult = false;
					if (ct.is_pz && options.zone_pz_blend_mode == 1) is_mult = true;
					if (ct.is_nopvp && options.zone_nopvp_blend_mode == 1) is_mult = true;
					if (ct.is_nolog && options.zone_nologout_blend_mode == 1) is_mult = true;
					if (ct.is_pvp && options.zone_pvp_blend_mode == 1) is_mult = true;

					int draw_x, draw_y;
					view.getScreenPosition(x, y, z, draw_x, draw_y);

					if (is_mult) {
						tile_zone_flags |= ZONE_FLAG_MULTIPLICATIVE;
						mult_zone_quads.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), tile_zone_flags });
					} else {
						alpha_zone_quads.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), tile_zone_flags });
					}
				}
			}

			std::swap(row_prev, row_curr);
			std::swap(row_curr, row_next);
		}

		if (!alpha_zone_quads.empty()) {
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			for (const auto& q : alpha_zone_quads) {
				sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
				                  0.0f, q.flags);
			}
		}

		if (!mult_zone_quads.empty()) {
			sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
			for (const auto& q : mult_zone_quads) {
				sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
				                  0.0f, q.flags);
			}
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
		}
	}

	// 2. Pre-calculate Cluster Badges
	if (options.show_special_tiles && !visible_zone_tiles.empty() && view.zoom <= 10.0f) {
		cluster_finder_.findClusters(z, bounds, visible_zone_tiles);
	}

	// 3. Spawns Perimeter Box Pass (Ground level)
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

			uint32_t spawn_flags = ZONE_FLAG_SPAWN |
			                       ZONE_FLAG_SPAWN_BORDER_N |
			                       ZONE_FLAG_SPAWN_BORDER_S |
			                       ZONE_FLAG_SPAWN_BORDER_W |
			                       ZONE_FLAG_SPAWN_BORDER_E;

			if (options.zone_spawn_blend_mode == 1) {
				spawn_flags |= ZONE_FLAG_MULTIPLICATIVE;
				sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
			} else {
				sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			}

			const float box_alpha = (st->spawn->isSelected() && options.dragging) ? (floor_alpha * 0.30f) : floor_alpha;
			sprite_batch.draw(static_cast<float>(draw_x0), static_cast<float>(draw_y0),
			                  spawn_w, spawn_h, *white_pixel, 1.0f, 1.0f, 1.0f, box_alpha,
			                  0.0f, spawn_flags);

			if (options.zone_spawn_blend_mode == 1) {
				sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			}
		}
	}

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

			if (sx1 < bounds.start_x || sx0 > bounds.end_x ||
			    sy1 < bounds.start_y || sy0 > bounds.end_y) {
				continue;
			}

			const float box_alpha = (st->spawn->isSelected() && options.dragging) ? (floor_alpha * 0.30f) : floor_alpha;
			int center_draw_x, center_draw_y;
			view.getScreenPosition(spos.x, spos.y, z, center_draw_x, center_draw_y);
			sprite_batch.draw(static_cast<float>(center_draw_x), static_cast<float>(center_draw_y),
			                  32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, box_alpha,
			                  INDICATOR_SPAWN_BASE, 0.0f);
		}
	}
}

void ZoneOverlayDrawer::drawFloorBlocking(SpriteBatch& sprite_batch,
                                          int z,
                                          const RenderView& view,
                                          const Map& map,
                                          const BaseMap* secondary_map,
                                          const DrawingOptions& options,
                                          const AtlasManager& atlas) {
	if (options.ingame || !options.show_blocking || view.zoom > 10.0f) {
		return;
	}

	const AtlasRegion* white_pixel = atlas.getWhitePixel();
	if (!white_pixel) {
		return;
	}

	if (options.zone_blocking_blend_mode == 1) {
		sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
	}

	const ViewBounds bounds = view.getBoundsForFloor(z);
	const float floor_alpha = 1.0f;

	const int min_x = bounds.start_x - 1;
	const int max_x = bounds.end_x + 1;
	const int row_width = max_x - min_x + 1;

	std::vector<uint8_t> row_prev(row_width, 0);
	std::vector<uint8_t> row_curr(row_width, 0);
	std::vector<uint8_t> row_next(row_width, 0);

	auto fetchRow = [&](int ry, std::vector<uint8_t>& row) {
		for (int x = min_x; x <= max_x; ++x) {
			const Tile* t = secondary_map ? secondary_map->getTile(x, ry, z) : nullptr;
			if (!t) {
				t = map.getTile(x, ry, z);
			}
			row[x - min_x] = (t && IsTilePathBlocking(t)) ? 1 : 0;
		}
	};

	fetchRow(bounds.start_y - 1, row_prev);
	fetchRow(bounds.start_y, row_curr);

	for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
		fetchRow(y + 1, row_next);

		for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
			const int idx = x - min_x;
			if (!row_curr[idx]) {
				continue;
			}

			uint32_t tile_zone_flags = ZONE_FLAG_BLOCKING;
			if (options.zone_blocking_blend_mode == 1) {
				tile_zone_flags |= ZONE_FLAG_MULTIPLICATIVE;
			}
			if (!row_prev[idx])     tile_zone_flags |= ZONE_FLAG_BLOCK_BORDER_N;
			if (!row_next[idx])     tile_zone_flags |= ZONE_FLAG_BLOCK_BORDER_S;
			if (!row_curr[idx - 1]) tile_zone_flags |= ZONE_FLAG_BLOCK_BORDER_W;
			if (!row_curr[idx + 1]) tile_zone_flags |= ZONE_FLAG_BLOCK_BORDER_E;

			int draw_x, draw_y;
			view.getScreenPosition(x, y, z, draw_x, draw_y);
			sprite_batch.draw(static_cast<float>(draw_x), static_cast<float>(draw_y),
			                  32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
			                  0.0f, tile_zone_flags);
		}

		std::swap(row_prev, row_curr);
		std::swap(row_curr, row_next);
	}

	if (options.zone_blocking_blend_mode == 1) {
		sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
	}
}

void ZoneOverlayDrawer::drawFloorBadges(SpriteBatch& sprite_batch,
                                        int z,
                                        const RenderView& view,
                                        const Map& map,
                                        const BaseMap* /*secondary_map*/,
                                        const DrawingOptions& options,
                                        const AtlasManager& atlas) {
	if (options.ingame) {
		return;
	}

	const AtlasRegion* white_pixel = atlas.getWhitePixel();
	if (!white_pixel) {
		return;
	}

	const ViewBounds bounds = view.getBoundsForFloor(z);
	const float floor_alpha = 1.0f;

	// 1. Cluster Zone Badges Pass (Rendered on top of items, tables, walls, and statues)
	if (options.show_special_tiles && view.zoom <= 10.0f) {
		const auto& badges = cluster_finder_.getLastBadges();
		for (const auto& badge : badges) {
			if (badge.z != z) {
				continue;
			}
			int draw_x, draw_y;
			view.getScreenPosition(badge.center_x, badge.center_y, z, draw_x, draw_y);
			const float px = static_cast<float>(draw_x) + 16.0f - (badge.width * 0.5f) + badge.offset_x;
			const float py = static_cast<float>(draw_y) + 16.0f - (badge.height * 0.5f) + badge.offset_y;
			sprite_batch.draw(px, py, badge.width, badge.height, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
			                  0.0f, ZONE_FLAG_CLUSTER_BADGE | badge.zone_flag);
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
		drawFloorBlocking(sprite_batch, z, view, map, secondary_map, options, atlas);
		drawFloorBadges(sprite_batch, z, view, map, secondary_map, options, atlas);
	}
}

} // namespace rme::rendering
