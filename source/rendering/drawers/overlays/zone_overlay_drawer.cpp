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

	const ViewBounds bounds = view.getBoundsForFloor(z);
	const float floor_alpha = (z == view.floor) ? 1.0f : std::max(0.25f, 1.0f - static_cast<float>(view.floor - z) * 0.20f);

	struct CachedRowTile {
		const Tile* tile = nullptr;
		bool is_blocking = false;
		bool is_pz = false;
		bool is_nopvp = false;
		bool is_nolog = false;
		bool is_pvp = false;
	};

	std::vector<CachedRowTile> row_prev;
	std::vector<CachedRowTile> row_curr;
	std::vector<CachedRowTile> row_next;
	std::vector<VisibleZoneTile> visible_zone_tiles;

	// 1. Special Zones & Pathing / Blocking Pass
	if ((options.show_special_tiles || options.show_blocking) && view.zoom <= 10.0f) {
		perimeter_nodes_.clear();
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
					ct.is_blocking = options.show_blocking && IsTilePathBlocking(t);
					if (options.show_special_tiles) {
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
				} else {
					ct.is_blocking = false;
					ct.is_pz = false;
					ct.is_nopvp = false;
					ct.is_nolog = false;
					ct.is_pvp = false;
				}
			}
		};

		auto evaluateVertexRow = [&](int vy, const std::vector<CachedRowTile>& r_north, const std::vector<CachedRowTile>& r_south) {
			if (!options.show_special_tiles) {
				return;
			}
			for (int vx = bounds.start_x; vx <= bounds.end_x + 1; ++vx) {
				const int idx = vx - min_x;
				const CachedRowTile& nw = r_north[idx - 1];
				const CachedRowTile& ne = r_north[idx];
				const CachedRowTile& sw = r_south[idx - 1];
				const CachedRowTile& se = r_south[idx];

				const int pz_count  = (nw.is_pz ? 1 : 0) + (ne.is_pz ? 1 : 0) + (sw.is_pz ? 1 : 0) + (se.is_pz ? 1 : 0);
				const int np_count  = (nw.is_nopvp ? 1 : 0) + (ne.is_nopvp ? 1 : 0) + (sw.is_nopvp ? 1 : 0) + (se.is_nopvp ? 1 : 0);
				const int nl_count  = (nw.is_nolog ? 1 : 0) + (ne.is_nolog ? 1 : 0) + (sw.is_nolog ? 1 : 0) + (se.is_nolog ? 1 : 0);
				const int pvp_count = (nw.is_pvp ? 1 : 0) + (ne.is_pvp ? 1 : 0) + (sw.is_pvp ? 1 : 0) + (se.is_pvp ? 1 : 0);

				uint32_t node_zone_flags = 0;
				if (pz_count >= 1 && pz_count <= 3)   node_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PZ);
				if (np_count >= 1 && np_count <= 3)   node_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOPVP);
				if (nl_count >= 1 && nl_count <= 3)   node_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOLOGOUT);
				if (pvp_count >= 1 && pvp_count <= 3) node_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PVPZONE);

				if (node_zone_flags != 0) {
					int vx_screen, vy_screen;
					view.getScreenPosition(vx, vy, z, vx_screen, vy_screen);
					perimeter_nodes_.push_back({
						static_cast<float>(vx_screen) - 3.0f,
						static_cast<float>(vy_screen) - 3.0f,
						static_cast<float>(static_cast<uint32_t>(ZONE_FLAG_PERIMETER_NODE) | node_zone_flags)
					});
				}
			}
		};

		fetchRow(bounds.start_y - 1, row_prev);
		fetchRow(bounds.start_y, row_curr);

		for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
			fetchRow(y + 1, row_next);

			evaluateVertexRow(y, row_prev, row_curr);

			for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
				const int idx = x - min_x;
				const CachedRowTile& ct = row_curr[idx];
				if (!ct.tile) {
					continue;
				}

				uint32_t tile_zone_flags = 0;

				// Pathing / Blocking
				if (ct.is_blocking) {
					tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCKING);
					if (!row_prev[idx].is_blocking)     tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_N);
					if (!row_next[idx].is_blocking)     tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_S);
					if (!row_curr[idx - 1].is_blocking) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_W);
					if (!row_curr[idx + 1].is_blocking) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_BLOCK_BORDER_E);
				}

				// Special Zones (PZ, No-PvP, No-Logout, PvP Zone)
				if (ct.is_pz || ct.is_nopvp || ct.is_nolog || ct.is_pvp) {
					if (ct.is_pz)    tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PZ);
					if (ct.is_nopvp) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOPVP);
					if (ct.is_nolog) tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_NOLOGOUT);
					if (ct.is_pvp)   tile_zone_flags |= static_cast<uint32_t>(ZONE_FLAG_PVPZONE);

					auto checkNeighbor = [&](const CachedRowTile& n, uint32_t global_bit, uint32_t internal_bit) {
						const bool n_has_any = n.is_pz || n.is_nopvp || n.is_nolog || n.is_pvp;
						if (!n_has_any) {
							tile_zone_flags |= global_bit;
						} else if (n.is_pz != ct.is_pz || n.is_nopvp != ct.is_nopvp ||
						           n.is_nolog != ct.is_nolog || n.is_pvp != ct.is_pvp) {
							tile_zone_flags |= internal_bit;
						}
					};

					checkNeighbor(row_prev[idx],     static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_N), static_cast<uint32_t>(ZONE_FLAG_ZONE_INTERNAL_N));
					checkNeighbor(row_next[idx],     static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_S), static_cast<uint32_t>(ZONE_FLAG_ZONE_INTERNAL_S));
					checkNeighbor(row_curr[idx - 1], static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_W), static_cast<uint32_t>(ZONE_FLAG_ZONE_INTERNAL_W));
					checkNeighbor(row_curr[idx + 1], static_cast<uint32_t>(ZONE_FLAG_ZONE_BORDER_E), static_cast<uint32_t>(ZONE_FLAG_ZONE_INTERNAL_E));

					uint8_t zmask = (ct.is_pz ? 1 : 0) | (ct.is_nopvp ? 2 : 0) | (ct.is_nolog ? 4 : 0) | (ct.is_pvp ? 8 : 0);
					visible_zone_tiles.push_back({ static_cast<int16_t>(x), static_cast<int16_t>(y), zmask });
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

		evaluateVertexRow(bounds.end_y + 1, row_prev, row_curr);

		// Draw perimeter connection nodes on top of tile borders
		if (options.show_special_tiles) {
			for (const auto& node : perimeter_nodes_) {
				sprite_batch.draw(node.x, node.y,
				                  6.0f, 6.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
				                  0.0f, node.flags);
			}
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

			const uint32_t spawn_flags = static_cast<uint32_t>(ZONE_FLAG_SPAWN) |
			                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_N) |
			                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_S) |
			                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_W) |
			                             static_cast<uint32_t>(ZONE_FLAG_SPAWN_BORDER_E);

			const float box_alpha = (st->spawn->isSelected() && options.dragging) ? (floor_alpha * 0.30f) : floor_alpha;
			sprite_batch.draw(static_cast<float>(draw_x0), static_cast<float>(draw_y0),
			                  spawn_w, spawn_h, *white_pixel, 1.0f, 1.0f, 1.0f, box_alpha,
			                  0.0f, static_cast<float>(spawn_flags));
		}
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
	const float floor_alpha = (z == view.floor) ? 1.0f : std::max(0.25f, 1.0f - static_cast<float>(view.floor - z) * 0.20f);

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
			                  0.0f, static_cast<float>(static_cast<uint32_t>(ZONE_FLAG_CLUSTER_BADGE) | badge.zone_flag));
		}
	}

	// 2. Spawn Center Badges (Rendered on top of items)
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

			int center_draw_x, center_draw_y;
			view.getScreenPosition(spos.x, spos.y, z, center_draw_x, center_draw_y);
			const float box_alpha = (st->spawn->isSelected() && options.dragging) ? (floor_alpha * 0.30f) : floor_alpha;
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
		drawFloorBadges(sprite_batch, z, view, map, secondary_map, options, atlas);
	}
}

} // namespace rme::rendering
