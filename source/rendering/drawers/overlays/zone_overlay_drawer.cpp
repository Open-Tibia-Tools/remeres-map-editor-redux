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

	visible_zone_tiles_.clear();
	alpha_zone_quads_.clear();
	mult_zone_quads_.clear();
	border_zone_quads_.clear();
	spawn_borders_.clear();

	// 1. Special Zones Pass (Ground level)
	if (options.show_special_tiles && view.zoom <= 10.0f) {

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

		fetchRow(bounds.start_y - 1, row_prev_);
		fetchRow(bounds.start_y, row_curr_);

		for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
			fetchRow(y + 1, row_next_);

			for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
				const int idx = x - min_x;
				const CachedRowTile& ct = row_curr_[idx];
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

					checkNeighbor(row_prev_[idx],     ZONE_FLAG_ZONE_BORDER_N);
					checkNeighbor(row_next_[idx],     ZONE_FLAG_ZONE_BORDER_S);
					checkNeighbor(row_curr_[idx - 1], ZONE_FLAG_ZONE_BORDER_W);
					checkNeighbor(row_curr_[idx + 1], ZONE_FLAG_ZONE_BORDER_E);

					uint8_t zmask = (ct.is_pz ? 1 : 0) | (ct.is_nopvp ? 2 : 0) | (ct.is_nolog ? 4 : 0) | (ct.is_pvp ? 8 : 0);
					visible_zone_tiles_.push_back({ x, y, zmask });

					bool is_mult = false;
					if (ct.is_pz && options.zone_pz_blend_mode == 1) is_mult = true;
					if (ct.is_nopvp && options.zone_nopvp_blend_mode == 1) is_mult = true;
					if (ct.is_nolog && options.zone_nologout_blend_mode == 1) is_mult = true;
					if (ct.is_pvp && options.zone_pvp_blend_mode == 1) is_mult = true;

					int draw_x, draw_y;
					view.getScreenPosition(x, y, z, draw_x, draw_y);

					if (is_mult) {
						tile_zone_flags |= ZONE_FLAG_MULTIPLICATIVE;
						mult_zone_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), tile_zone_flags });
					} else {
						alpha_zone_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), tile_zone_flags });
					}

					if (options.show_zone_borders && (tile_zone_flags & (ZONE_FLAG_ZONE_BORDER_N | ZONE_FLAG_ZONE_BORDER_S | ZONE_FLAG_ZONE_BORDER_W | ZONE_FLAG_ZONE_BORDER_E))) {
						uint32_t border_flags = (tile_zone_flags & ~ZONE_FLAG_MULTIPLICATIVE) | ZONE_FLAG_BORDER_PASS;
						border_zone_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), border_flags });
					}
				}
			}

			std::swap(row_prev_, row_curr_);
			std::swap(row_curr_, row_next_);
		}

		if (!alpha_zone_quads_.empty()) {
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			for (const auto& q : alpha_zone_quads_) {
				sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
				                  0.0f, q.flags);
			}
		}

		if (!mult_zone_quads_.empty()) {
			sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
			for (const auto& q : mult_zone_quads_) {
				sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
				                  0.0f, q.flags);
			}
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
		}

		if (!border_zone_quads_.empty()) {
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			for (const auto& q : border_zone_quads_) {
				sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
				                  0.0f, q.flags);
			}
		}
	}

	// 2. Pre-calculate Cluster Badges
	if (options.show_special_tiles && !visible_zone_tiles_.empty() && view.zoom <= 10.0f) {
		cluster_finder_.findClusters(z, bounds, visible_zone_tiles_);
	}

	// 3. Spawns Perimeter Box Pass (Ground level)
	if (options.show_spawns) {
		spawn_borders_.clear();

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

			if (options.show_zone_borders) {
				uint32_t border_flags = (spawn_flags & ~ZONE_FLAG_MULTIPLICATIVE) | ZONE_FLAG_BORDER_PASS;
				spawn_borders_.push_back({ static_cast<float>(draw_x0), static_cast<float>(draw_y0),
				                           spawn_w, spawn_h, box_alpha, border_flags });
			}
		}

		if (!spawn_borders_.empty()) {
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			for (const auto& sb : spawn_borders_) {
				sprite_batch.draw(sb.x, sb.y, sb.w, sb.h, *white_pixel, 1.0f, 1.0f, 1.0f, sb.alpha,
				                  0.0f, sb.flags);
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

	blocking_border_quads_.clear();

	auto fetchRow = [&](int ry, std::vector<uint8_t>& row) {
		row.resize(row_width);
		for (int x = min_x; x <= max_x; ++x) {
			const Tile* t = secondary_map ? secondary_map->getTile(x, ry, z) : nullptr;
			if (!t) {
				t = map.getTile(x, ry, z);
			}
			row[x - min_x] = (t && IsTilePathBlocking(t)) ? 1 : 0;
		}
	};

	fetchRow(bounds.start_y - 1, blocking_row_prev_);
	fetchRow(bounds.start_y, blocking_row_curr_);

	for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
		fetchRow(y + 1, blocking_row_next_);

		for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
			const int idx = x - min_x;
			if (!blocking_row_curr_[idx]) {
				continue;
			}

			uint32_t tile_zone_flags = ZONE_FLAG_BLOCKING;
			if (options.zone_blocking_blend_mode == 1) {
				tile_zone_flags |= ZONE_FLAG_MULTIPLICATIVE;
			}
			const bool bN = !blocking_row_prev_[idx];
			const bool bS = !blocking_row_next_[idx];
			const bool bW = !blocking_row_curr_[idx - 1];
			const bool bE = !blocking_row_curr_[idx + 1];

			if (bN) tile_zone_flags |= ZONE_FLAG_BLOCK_BORDER_N;
			if (bS) tile_zone_flags |= ZONE_FLAG_BLOCK_BORDER_S;
			if (bW) tile_zone_flags |= ZONE_FLAG_BLOCK_BORDER_W;
			if (bE) tile_zone_flags |= ZONE_FLAG_BLOCK_BORDER_E;

			int draw_x, draw_y;
			view.getScreenPosition(x, y, z, draw_x, draw_y);
			const float fx = static_cast<float>(draw_x);
			const float fy = static_cast<float>(draw_y);

			sprite_batch.draw(fx, fy, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
			                  0.0f, tile_zone_flags);

			if (options.show_zone_borders && (bN || bS || bW || bE)) {
				uint32_t border_flags = (tile_zone_flags & ~ZONE_FLAG_MULTIPLICATIVE) | ZONE_FLAG_BORDER_PASS;
				blocking_border_quads_.push_back({ fx, fy, border_flags });
			}
		}

		std::swap(blocking_row_prev_, blocking_row_curr_);
		std::swap(blocking_row_curr_, blocking_row_next_);
	}

	if (options.zone_blocking_blend_mode == 1) {
		sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
	}

	if (!blocking_border_quads_.empty()) {
		sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
		for (const auto& q : blocking_border_quads_) {
			sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
			                  0.0f, q.flags);
		}
	}
}

void ZoneOverlayDrawer::drawFloorHouses(SpriteBatch& sprite_batch,
                                        int z,
                                        const RenderView& view,
                                        const Map& map,
                                        const BaseMap* secondary_map,
                                        const DrawingOptions& options,
                                        const AtlasManager& atlas) {
	if (options.ingame || !options.show_houses || view.zoom > 10.0f) {
		return;
	}

	const AtlasRegion* white_pixel = atlas.getWhitePixel();
	if (!white_pixel) {
		return;
	}

	const ViewBounds bounds = view.getBoundsForFloor(z);
	const float floor_alpha = 1.0f;

	const int min_x = bounds.start_x - 1;
	const int max_x = bounds.end_x + 1;
	const int row_width = max_x - min_x + 1;

	house_border_quads_.clear();

	auto fetchRow = [&](int ry, std::vector<uint32_t>& row) {
		row.resize(row_width);
		for (int x = min_x; x <= max_x; ++x) {
			const Tile* t = secondary_map ? secondary_map->getTile(x, ry, z) : nullptr;
			if (!t) {
				t = map.getTile(x, ry, z);
			}
			row[x - min_x] = (t && t->isHouseTile()) ? static_cast<uint32_t>(t->getHouseID()) : 0u;
		}
	};

	fetchRow(bounds.start_y - 1, house_row_prev_);
	fetchRow(bounds.start_y, house_row_curr_);

	for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
		fetchRow(y + 1, house_row_next_);

		for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
			const int idx = x - min_x;
			const uint32_t house_id = house_row_curr_[idx];
			if (house_id == 0) {
				continue;
			}

			const bool bN = (house_row_prev_[idx] != house_id);
			const bool bS = (house_row_next_[idx] != house_id);
			const bool bW = (house_row_curr_[idx - 1] != house_id);
			const bool bE = (house_row_curr_[idx + 1] != house_id);

			if (options.show_zone_borders && (bN || bS || bW || bE)) {
				uint32_t border_flags = ZONE_FLAG_BORDER_PASS | ZONE_FLAG_HOUSE;
				if (bN) border_flags |= ZONE_FLAG_ZONE_BORDER_N;
				if (bS) border_flags |= ZONE_FLAG_ZONE_BORDER_S;
				if (bW) border_flags |= ZONE_FLAG_ZONE_BORDER_W;
				if (bE) border_flags |= ZONE_FLAG_ZONE_BORDER_E;

				int draw_x, draw_y;
				view.getScreenPosition(x, y, z, draw_x, draw_y);
				house_border_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), border_flags });
			}
		}

		std::swap(house_row_prev_, house_row_curr_);
		std::swap(house_row_curr_, house_row_next_);
	}

	if (!house_border_quads_.empty()) {
		sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
		for (const auto& q : house_border_quads_) {
			sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
			                  0.0f, q.flags);
		}
	}
}

void ZoneOverlayDrawer::drawFloorHighlightItems(SpriteBatch& sprite_batch,
                                                int z,
                                                const RenderView& view,
                                                const Map& map,
                                                const BaseMap* secondary_map,
                                                const DrawingOptions& options,
                                                const AtlasManager& atlas) {
	if (options.ingame || !options.highlight_items || view.zoom > 10.0f) {
		return;
	}

	const AtlasRegion* white_pixel = atlas.getWhitePixel();
	if (!white_pixel) {
		return;
	}

	const ViewBounds bounds = view.getBoundsForFloor(z);

	highlight_quads_.clear();

	for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
		for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
			const Tile* t = secondary_map ? secondary_map->getTile(x, y, z) : nullptr;
			if (!t) {
				t = map.getTile(x, y, z);
			}
			if (!t || t->items.empty() || t->items.back()->isBorder()) {
				continue;
			}

			int draw_x, draw_y;
			view.getScreenPosition(x, y, z, draw_x, draw_y);

			uint32_t flags = MakeHighlightItemsFlags();
			highlight_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), flags });
		}
	}

	if (!highlight_quads_.empty()) {
		sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
		for (const auto& q : highlight_quads_) {
			sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, q.flags);
		}
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

} // namespace rme::rendering
