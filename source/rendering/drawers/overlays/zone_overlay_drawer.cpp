#include "rendering/drawers/overlays/zone_overlay_drawer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/atlas_manager.h"
#include "rendering/indicators/zone_flags.h"
#include "rendering/indicators/zone_colors.h"
#include "rendering/indicators/technical_item_registry.h"
#include "rendering/core/render_depth.h"
#include "util/phase_profiler.h"
#include "map/map.h"
#include "map/basemap.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/spawn.h"
#include "game/waypoints.h"
#include "app/settings.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <limits>

namespace rme::rendering {

bool IsTilePathBlocking(const Tile* t) noexcept {
	if (!t || !t->isBlocking() || !t->ground) {
		return false;
	}

	if (t->ground && t->ground->isBlocking()) {
		if (!TechnicalItemRegistry::IsInvisibleWall(t->ground->getID(), t->ground->getClientID())) {
			return true;
		}
	}
	for (const auto& item : t->items) {
		if (item && item->isBlocking()) {
			if (!TechnicalItemRegistry::IsInvisibleWall(item->getID(), item->getClientID())) {
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
	visible_zone_tiles_.clear();
	alpha_zone_quads_.clear();
	mult_zone_quads_.clear();
	border_zone_quads_.clear();
	alpha_spawn_quads_.clear();
	mult_spawn_quads_.clear();
	spawn_borders_.clear();
	spawn_badges_.clear();

	if (options.ingame) {
		return;
	}

	if (!options.show_special_tiles && !options.show_spawns) {
		return;
	}

	if (view.zoom > kZoomLODCutoff) {
		return;
	}

	rme::prof::Scope zone_scope(rme::prof::Phase::ZoneOverlay);

	const AtlasRegion* white_pixel = atlas.getWhitePixel();
	if (!white_pixel) {
		return;
	}

	const ViewBounds bounds = view.getBoundsForFloor(z);
	const float floor_alpha = 1.0f;

	// 1. Special Zones Pass (Ground level)
	if (options.show_special_tiles && view.zoom <= kZoomLODCutoff) {

		const int min_x = bounds.start_x - 1;
		const int max_x = bounds.end_x + 1;
		const int row_width = max_x - min_x + 1;

		// Caller-owned cell cursors: consecutive x in a row share the containing
		// 64x64 cell and 4x4 Floor, so empty cells/floors skip 64 or 4 tiles at once.
		SpatialHashGrid::CellCursor secondary_cursor;
		SpatialHashGrid::CellCursor primary_cursor;
		auto encodeZoneMask = [](const Tile* t) noexcept -> uint8_t {
			if (!t) {
				return 0;
			}
			const uint32_t mf = t->getMapFlags();
			return static_cast<uint8_t>(
				((mf & TILESTATE_PROTECTIONZONE) ? 1u : 0u) |
				((mf & TILESTATE_NOPVP)          ? 2u : 0u) |
				((mf & TILESTATE_NOLOGOUT)       ? 4u : 0u) |
				((mf & TILESTATE_PVPZONE)        ? 8u : 0u));
		};
		auto fetchRow = [&](int ry, std::vector<uint8_t>& row) {
			row.resize(row_width);
			uint8_t* out = row.data();
			if (secondary_map) {
				for (int x = min_x; x <= max_x; ++x) {
					const Tile* t = secondary_map->getTile(x, ry, z, secondary_cursor);
					if (!t) {
						t = map.getTile(x, ry, z, primary_cursor);
					}
					out[x - min_x] = encodeZoneMask(t);
				}
				return;
			}
			const int ly = ry & 3;
			for (int x = min_x; x <= max_x;) {
				const Floor* floor = map.getFloor(x, ry, z, primary_cursor);
				if (!floor) {
					const int span_end = std::min(max_x, x | primary_cursor.empty_span_mask);
					std::fill(out + (x - min_x), out + (span_end - min_x + 1), uint8_t { 0 });
					x = span_end + 1;
					continue;
				}
				const int node_end = std::min(max_x, x | 3);
				for (; x <= node_end; ++x) {
					const Tile* t = floor->locs[((x & 3) << 2) + ly].get();
					out[x - min_x] = encodeZoneMask(t);
				}
			}
		};

		fetchRow(bounds.start_y - 1, row_prev_);
		fetchRow(bounds.start_y, row_curr_);

		for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
			fetchRow(y + 1, row_next_);

			for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
				const int idx = x - min_x;
				const uint8_t zmask = row_curr_[idx];
				if (zmask == 0) {
					continue;
				}

				uint32_t tile_zone_flags = 0;
				if (zmask & 1u) tile_zone_flags |= ZONE_FLAG_PZ;
				if (zmask & 2u) tile_zone_flags |= ZONE_FLAG_NOPVP;
				if (zmask & 4u) tile_zone_flags |= ZONE_FLAG_NOLOGOUT;
				if (zmask & 8u) tile_zone_flags |= ZONE_FLAG_PVPZONE;

				if (row_prev_[idx] == 0)     tile_zone_flags |= ZONE_FLAG_ZONE_BORDER_N;
				if (row_next_[idx] == 0)     tile_zone_flags |= ZONE_FLAG_ZONE_BORDER_S;
				if (row_curr_[idx - 1] == 0) tile_zone_flags |= ZONE_FLAG_ZONE_BORDER_W;
				if (row_curr_[idx + 1] == 0) tile_zone_flags |= ZONE_FLAG_ZONE_BORDER_E;

				visible_zone_tiles_.push_back({ x, y, zmask });

				bool is_mult = false;
				if ((zmask & 1u) && options.zone_pz_blend_mode == 1) is_mult = true;
				if ((zmask & 2u) && options.zone_nopvp_blend_mode == 1) is_mult = true;
				if ((zmask & 4u) && options.zone_nologout_blend_mode == 1) is_mult = true;
				if ((zmask & 8u) && options.zone_pvp_blend_mode == 1) is_mult = true;

				int draw_x, draw_y;
				view.getScreenPosition(x, y, z, draw_x, draw_y);
				const float depth = calculateTileDepth(x, y, RenderSublayer::GroundOverlay);

				if (is_mult) {
					tile_zone_flags |= ZONE_FLAG_MULTIPLICATIVE;
					mult_zone_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), tile_zone_flags, depth });
				} else {
					alpha_zone_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), tile_zone_flags, depth });
				}

				if (options.show_zone_borders && (tile_zone_flags & (ZONE_FLAG_ZONE_BORDER_N | ZONE_FLAG_ZONE_BORDER_S | ZONE_FLAG_ZONE_BORDER_W | ZONE_FLAG_ZONE_BORDER_E))) {
					uint32_t border_flags = (tile_zone_flags & ~ZONE_FLAG_MULTIPLICATIVE) | ZONE_FLAG_BORDER_PASS;
					border_zone_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), border_flags, depth });
				}
			}

			std::swap(row_prev_, row_curr_);
			std::swap(row_curr_, row_next_);
		}

		if (!alpha_zone_quads_.empty()) {
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			for (const auto& q : alpha_zone_quads_) {
				sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
				                  0.0f, q.flags, q.depth);
			}
		}

		if (!mult_zone_quads_.empty()) {
			sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
			for (const auto& q : mult_zone_quads_) {
				sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
				                  0.0f, q.flags, q.depth);
			}
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
		}

		if (!border_zone_quads_.empty()) {
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			for (const auto& q : border_zone_quads_) {
				sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
				                  0.0f, q.flags, q.depth);
			}
		}
	}

	// 2. Pre-calculate Cluster Badges
	if (options.show_special_tiles && view.zoom <= kZoomLODCutoff) {
		cluster_finder_.findClusters(z, bounds, visible_zone_tiles_);
	}

	// 3. Spawns Pass (Ground level)
	if (options.show_spawns && view.zoom <= kZoomLODCutoff) {
		const bool show_spawn_details = (view.zoom <= kSpawnLODDetailThreshold);
		const int max_coarse_radius = std::max(kMaxCoarseRadius, g_settings.getInteger(Config::MAX_SPAWN_RADIUS));
		const int coarse_min_x = bounds.start_x - max_coarse_radius;
		const int coarse_max_x = bounds.end_x + max_coarse_radius;
		const int coarse_min_y = bounds.start_y - max_coarse_radius;
		const int coarse_max_y = bounds.end_y + max_coarse_radius;

		// Spawns are stored in std::set<Position> sorted by (z, y, x).
		// By querying lower_bound for (INT_MIN, coarse_min_y, z), we jump directly to the
		// first candidate spawn on floor z within the visible Y range in O(log N) time.
		const Position floor_start_pos(std::numeric_limits<int>::min(), coarse_min_y, z);
		auto it = map.spawns.lower_bound(floor_start_pos);

		for (; it != map.spawns.end() && it->z == z; ++it) {
			// Because y is strictly non-decreasing across floor z in the sorted set,
			// once it->y exceeds coarse_max_y, no subsequent spawn on this floor can intersect the viewport.
			if (it->y > coarse_max_y) {
				break;
			}

			const Position& spos = *it;

			// Coarse X AABB culling: reject without MapNode lookup if spawn center is far outside X bounds
			if (spos.x < coarse_min_x || spos.x > coarse_max_x) {
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

			// Exact Viewport intersection test
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

			const float box_alpha = (st->spawn->isSelected() && options.dragging) ? (floor_alpha * 0.30f) : floor_alpha;

			const float spawn_depth = calculateTileDepth(spos.x, spos.y, RenderSublayer::GroundOverlay);

			if (options.zone_spawn_blend_mode == 1) {
				spawn_flags |= ZONE_FLAG_MULTIPLICATIVE;
				mult_spawn_quads_.push_back({ static_cast<float>(draw_x0), static_cast<float>(draw_y0),
				                              spawn_w, spawn_h, box_alpha, spawn_flags, spawn_depth });
			} else {
				alpha_spawn_quads_.push_back({ static_cast<float>(draw_x0), static_cast<float>(draw_y0),
				                              spawn_w, spawn_h, box_alpha, spawn_flags, spawn_depth });
			}

			// LOD Detail level: borders and flame badges are omitted when zoomed beyond 15% zoom
			if (show_spawn_details) {
				if (options.show_zone_borders) {
					uint32_t border_flags = (spawn_flags & ~ZONE_FLAG_MULTIPLICATIVE) | ZONE_FLAG_BORDER_PASS;
					spawn_borders_.push_back({ static_cast<float>(draw_x0), static_cast<float>(draw_y0),
					                           spawn_w, spawn_h, box_alpha, border_flags, spawn_depth });
				}

				int center_draw_x, center_draw_y;
				view.getScreenPosition(spos.x, spos.y, z, center_draw_x, center_draw_y);
				const float badge_depth = calculateTileDepth(spos.x, spos.y, RenderSublayer::Overlay);
				spawn_badges_.push_back({ static_cast<float>(center_draw_x), static_cast<float>(center_draw_y),
				                          32.0f, 32.0f, box_alpha, 0, badge_depth });
			}
		}

		// Batched draw calls: zero per-spawn state changes or flushes
		if (!alpha_spawn_quads_.empty()) {
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			for (const auto& sq : alpha_spawn_quads_) {
				sprite_batch.draw(sq.x, sq.y, sq.w, sq.h, *white_pixel, 1.0f, 1.0f, 1.0f, sq.alpha,
				                  0.0f, sq.flags, sq.depth);
			}
		}

		if (!mult_spawn_quads_.empty()) {
			sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
			for (const auto& sq : mult_spawn_quads_) {
				sprite_batch.draw(sq.x, sq.y, sq.w, sq.h, *white_pixel, 1.0f, 1.0f, 1.0f, sq.alpha,
				                  0.0f, sq.flags, sq.depth);
			}
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
		}

		if (!spawn_borders_.empty()) {
			sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
			for (const auto& sb : spawn_borders_) {
				sprite_batch.draw(sb.x, sb.y, sb.w, sb.h, *white_pixel, 1.0f, 1.0f, 1.0f, sb.alpha,
				                  0.0f, sb.flags, sb.depth);
			}
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
	if (options.ingame || !options.show_blocking || view.zoom > kZoomLODCutoff) {
		return;
	}
	rme::prof::Scope blocking_scope(rme::prof::Phase::BlockingOverlay);

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

	SpatialHashGrid::CellCursor secondary_cursor;
	SpatialHashGrid::CellCursor primary_cursor;
	auto fetchRow = [&](int ry, std::vector<uint8_t>& row) {
		row.resize(row_width);
		uint8_t* out = row.data();
		if (secondary_map) {
			for (int x = min_x; x <= max_x; ++x) {
				const Tile* t = secondary_map->getTile(x, ry, z, secondary_cursor);
				if (!t) {
					t = map.getTile(x, ry, z, primary_cursor);
				}
				out[x - min_x] = (t && IsTilePathBlocking(t)) ? 1 : 0;
			}
			return;
		}
		const int ly = ry & 3;
		for (int x = min_x; x <= max_x;) {
			const Floor* floor = map.getFloor(x, ry, z, primary_cursor);
			if (!floor) {
				const int span_end = std::min(max_x, x | primary_cursor.empty_span_mask);
				std::fill(out + (x - min_x), out + (span_end - min_x + 1), uint8_t { 0 });
				x = span_end + 1;
				continue;
			}
			const int node_end = std::min(max_x, x | 3);
			for (; x <= node_end; ++x) {
				const Tile* t = floor->locs[((x & 3) << 2) + ly].get();
				out[x - min_x] = (t && IsTilePathBlocking(t)) ? 1 : 0;
			}
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
			const float depth = calculateTileDepth(x, y, RenderSublayer::GroundOverlay);

			sprite_batch.draw(fx, fy, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
			                  0.0f, tile_zone_flags, depth);

			if (options.show_zone_borders && (bN || bS || bW || bE)) {
				uint32_t border_flags = (tile_zone_flags & ~ZONE_FLAG_MULTIPLICATIVE) | ZONE_FLAG_BORDER_PASS;
				blocking_border_quads_.push_back({ fx, fy, border_flags, depth });
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
			                  0.0f, q.flags, q.depth);
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
	// This pass only emits house border quads, and those only when zone borders are
	// enabled (house tints come from the sprite batch shader). Without borders it
	// would sweep the whole viewport with a getTile per tile for no output at all.
	if (options.ingame || !options.show_houses || !options.show_zone_borders || view.zoom > kZoomLODCutoff) {
		return;
	}
	rme::prof::Scope houses_scope(rme::prof::Phase::HouseOverlay);

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

	SpatialHashGrid::CellCursor secondary_cursor;
	SpatialHashGrid::CellCursor primary_cursor;
	auto fetchRow = [&](int ry, std::vector<uint32_t>& row) {
		row.resize(row_width);
		uint32_t* out = row.data();
		if (secondary_map) {
			for (int x = min_x; x <= max_x; ++x) {
				const Tile* t = secondary_map->getTile(x, ry, z, secondary_cursor);
				if (!t) {
					t = map.getTile(x, ry, z, primary_cursor);
				}
				out[x - min_x] = (t && t->isHouseTile()) ? static_cast<uint32_t>(t->getHouseID()) : 0u;
			}
			return;
		}
		const int ly = ry & 3;
		for (int x = min_x; x <= max_x;) {
			const Floor* floor = map.getFloor(x, ry, z, primary_cursor);
			if (!floor) {
				const int span_end = std::min(max_x, x | primary_cursor.empty_span_mask);
				std::fill(out + (x - min_x), out + (span_end - min_x + 1), uint32_t { 0 });
				x = span_end + 1;
				continue;
			}
			const int node_end = std::min(max_x, x | 3);
			for (; x <= node_end; ++x) {
				const Tile* t = floor->locs[((x & 3) << 2) + ly].get();
				out[x - min_x] = (t && t->isHouseTile()) ? static_cast<uint32_t>(t->getHouseID()) : 0u;
			}
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
				const float depth = calculateTileDepth(x, y, RenderSublayer::GroundOverlay);
				house_border_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), border_flags, depth });
			}
		}

		std::swap(house_row_prev_, house_row_curr_);
		std::swap(house_row_curr_, house_row_next_);
	}

	if (!house_border_quads_.empty()) {
		sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
		for (const auto& q : house_border_quads_) {
			sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
			                  0.0f, q.flags, q.depth);
		}
	}
}

namespace {
} // namespace

void ZoneOverlayDrawer::drawFloorZones(SpriteBatch& sprite_batch,
                                       int z,
                                       const RenderView& view,
                                       const Map& map,
                                       const BaseMap* secondary_map,
                                       const DrawingOptions& options,
                                       const AtlasManager& atlas) {
	if (!map.supportsZones() || options.ingame || !options.show_zones || view.zoom > kZoomLODCutoff) {
		return;
	}

	if (map.zone_index.totalTileCount() == 0 && (!secondary_map || !secondary_map->supportsZones())) {
		return;
	}

	const ViewBounds bounds = view.getBoundsForFloor(z);

	if (!map.zone_index.hasFloor(z) && (!secondary_map || !secondary_map->supportsZones())) {
		return;
	}

	const int min_x = bounds.start_x - 1;
	const int max_x = bounds.end_x + 1;
	const int row_width = max_x - min_x + 1;
	const uint16_t active_zone = options.current_zone_id;

	auto fetchRow = [&](int ry, std::vector<ZoneRowCell>& row) {
		row.resize(row_width);
		for (int x = min_x; x <= max_x; ++x) {
			const Tile* t = secondary_map ? secondary_map->getTile(x, ry, z) : nullptr;
			if (!t) {
				t = map.getTile(x, ry, z);
			}
			if (!t || !t->hasZones()) {
				row[x - min_x] = ZoneRowCell{ 0, 0 };
				continue;
			}
			const auto ids = t->getZoneIds();
			uint16_t prim = (active_zone != 0 && std::ranges::binary_search(ids, active_zone)) ? active_zone : ids.front();
			uint16_t sec = 0;
			if (ids.size() > 1) {
				sec = (ids.front() == prim) ? ids[1] : ids.front();
			}
			row[x - min_x] = ZoneRowCell{ prim, sec };
		}
	};

	fetchRow(bounds.start_y - 1, zone_row_prev_);
	fetchRow(bounds.start_y, zone_row_curr_);

	sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
	constexpr float kBorder = 2.0f;
	for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
		fetchRow(y + 1, zone_row_next_);

		for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
			const int idx = x - min_x;
			const ZoneRowCell cell = zone_row_curr_[idx];
			const uint16_t zone_id = cell.primary;
			if (zone_id == 0) {
				continue;
			}

			const bool active = (zone_id == active_zone);
			const glm::vec3 rgb = getZoneColor(zone_id);
			int draw_x, draw_y;
			view.getScreenPosition(x, y, z, draw_x, draw_y);
			const float fx = static_cast<float>(draw_x);
			const float fy = static_cast<float>(draw_y);
			const float depth = calculateTileDepth(x, y, RenderSublayer::GroundOverlay);

			if (cell.secondary != 0) {
				const glm::vec3 sec_rgb = getZoneColor(cell.secondary);
				const bool sec_active = (cell.secondary == active_zone);
				sprite_batch.drawRect(fx, fy, 32.0f, 16.0f, glm::vec4(rgb, active ? 0.45f : 0.25f), atlas, depth);
				sprite_batch.drawRect(fx, fy + 16.0f, 32.0f, 16.0f, glm::vec4(sec_rgb, sec_active ? 0.45f : 0.25f), atlas, depth);
			} else {
				sprite_batch.drawRect(fx, fy, 32.0f, 32.0f, glm::vec4(rgb, active ? 0.45f : 0.25f), atlas, depth);
			}

			if (options.show_zone_borders) {
				const glm::vec4 edge(rgb, 0.9f);
				if (zone_row_prev_[idx].primary != zone_id) {
					sprite_batch.drawRect(fx, fy, 32.0f, kBorder, edge, atlas, depth);
				}
				if (zone_row_next_[idx].primary != zone_id) {
					sprite_batch.drawRect(fx, fy + 32.0f - kBorder, 32.0f, kBorder, edge, atlas, depth);
				}
				if (zone_row_curr_[idx - 1].primary != zone_id) {
					sprite_batch.drawRect(fx, fy, kBorder, 32.0f, edge, atlas, depth);
				}
				if (zone_row_curr_[idx + 1].primary != zone_id) {
					sprite_batch.drawRect(fx + 32.0f - kBorder, fy, kBorder, 32.0f, edge, atlas, depth);
				}
			}
		}

		std::swap(zone_row_prev_, zone_row_curr_);
		std::swap(zone_row_curr_, zone_row_next_);
	}
}

void ZoneOverlayDrawer::drawFloorHighlightItems(SpriteBatch& sprite_batch,
                                                int z,
                                                const RenderView& view,
                                                const Map& map,
                                                const BaseMap* secondary_map,
                                                const DrawingOptions& options,
                                                const AtlasManager& atlas) {
	if (options.ingame || !options.highlight_items || view.zoom > kZoomLODCutoff) {
		return;
	}
	rme::prof::Scope highlight_scope(rme::prof::Phase::HighlightOverlay);

	const AtlasRegion* white_pixel = atlas.getWhitePixel();
	if (!white_pixel) {
		return;
	}

	const ViewBounds bounds = view.getBoundsForFloor(z);

	highlight_quads_.clear();

	SpatialHashGrid::CellCursor secondary_cursor;
	SpatialHashGrid::CellCursor primary_cursor;
	for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
		if (secondary_map) {
			for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
				const Tile* t = secondary_map->getTile(x, y, z, secondary_cursor);
				if (!t) {
					t = map.getTile(x, y, z, primary_cursor);
				}
				if (!t || t->items.empty() || t->items.back()->isBorder()) {
					continue;
				}
				int draw_x, draw_y;
				view.getScreenPosition(x, y, z, draw_x, draw_y);
				const float depth = calculateTileDepth(x, y, RenderSublayer::GroundOverlay);
				highlight_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), MakeHighlightItemsFlags(), depth });
			}
			continue;
		}
		const int ly = y & 3;
		for (int x = bounds.start_x; x <= bounds.end_x;) {
			const Floor* floor = map.getFloor(x, y, z, primary_cursor);
			if (!floor) {
				x = std::min(bounds.end_x, x | primary_cursor.empty_span_mask) + 1;
				continue;
			}
			const int node_end = std::min(bounds.end_x, x | 3);
			for (; x <= node_end; ++x) {
				const Tile* t = floor->locs[((x & 3) << 2) + ly].get();
				if (!t || t->items.empty() || t->items.back()->isBorder()) {
					continue;
				}
				int draw_x, draw_y;
				view.getScreenPosition(x, y, z, draw_x, draw_y);
				const float depth = calculateTileDepth(x, y, RenderSublayer::GroundOverlay);
				highlight_quads_.push_back({ static_cast<float>(draw_x), static_cast<float>(draw_y), MakeHighlightItemsFlags(), depth });
			}
		}
	}

	if (!highlight_quads_.empty()) {
		sprite_batch.setBlendFunc(GL_DST_COLOR, GL_ZERO, atlas);
		for (const auto& q : highlight_quads_) {
			sprite_batch.draw(q.x, q.y, 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, q.flags, q.depth);
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
	rme::prof::Scope badges_scope(rme::prof::Phase::BadgeOverlay);

	const ViewBounds bounds = view.getBoundsForFloor(z);
	const float floor_alpha = 1.0f;

	// 1. Cluster Zone Badges Pass (Rendered on top of items, tables, walls, and statues)
	if (options.show_special_tiles && view.zoom <= kZoomLODCutoff) {
		const auto& badges = cluster_finder_.getLastBadges();
		for (const auto& badge : badges) {
			if (badge.z != z) {
				continue;
			}
			int draw_x, draw_y;
			view.getScreenPosition(badge.center_x, badge.center_y, z, draw_x, draw_y);
			const float px = static_cast<float>(draw_x) + 16.0f - (badge.width * 0.5f) + badge.offset_x;
			const float py = static_cast<float>(draw_y) + 16.0f - (badge.height * 0.5f) + badge.offset_y;
			const float badge_depth = calculateTileDepth(badge.center_x, badge.center_y, RenderSublayer::Overlay);
			sprite_batch.draw(px, py, badge.width, badge.height, *white_pixel, 1.0f, 1.0f, 1.0f, floor_alpha,
			                  0.0f, ZONE_FLAG_CLUSTER_BADGE | badge.zone_flag, badge_depth);
		}
	}

	// 2. Spawn Center Badges Pass (Rendered on top of items, tables, walls, and statues)
	if (options.show_spawns && !spawn_badges_.empty()) {
		sprite_batch.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, atlas);
		for (const auto& b : spawn_badges_) {
			sprite_batch.draw(b.x, b.y, b.w, b.h, *white_pixel, 1.0f, 1.0f, 1.0f, b.alpha,
			                  0.0f, MakeIndicatorZoneFlag(static_cast<uint8_t>(TileIndicatorType::Spawn)), b.depth);
		}
	}
}

void ZoneOverlayDrawer::drawFloorWaypoints(SpriteBatch& sprite_batch,
                                           int z,
                                           const RenderView& view,
                                           const Map& map,
                                           const DrawingOptions& options,
                                           const AtlasManager& atlas) {
	if (options.ingame || !options.show_waypoints || map.waypoints.empty() || view.zoom > kZoomLODCutoff) {
		return;
	}

	const AtlasRegion* white_pixel = atlas.getWhitePixel();
	if (!white_pixel) {
		return;
	}

	for (const auto& [wp_name, wp_ptr] : map.waypoints) {
		if (!wp_ptr) {
			continue;
		}
		const Position& pos = wp_ptr->pos;
		if (pos.z != z || pos == Position()) {
			continue;
		}
		int draw_x = 0;
		int draw_y = 0;
		if (!view.IsTileVisible(pos.x, pos.y, pos.z, draw_x, draw_y)) {
			continue;
		}

		sprite_batch.draw(
			static_cast<float>(draw_x), static_cast<float>(draw_y),
			32.0f, 32.0f,
			*white_pixel,
			1.0f, 1.0f, 1.0f, 1.0f,
			0.0f, MakeIndicatorZoneFlag(static_cast<uint8_t>(TileIndicatorType::Waypoint)), 0.0f
		);
	}
}

} // namespace rme::rendering
