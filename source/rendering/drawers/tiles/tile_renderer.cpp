#include "rendering/drawers/tiles/tile_renderer.h"
#include "rendering/core/sprite_batch.h"

#include "editor/editor.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/waypoints.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "game/complexitem.h"

#include "rendering/core/drawing_options.h"
#include "rendering/core/render_view.h"
#include "rendering/core/render_frame_context.h"
#include "rendering/core/graphics.h"
#include "rendering/drawers/tiles/tile_color_calculator.h"
#include "app/definitions.h"
#include "game/sprites.h"

#include "rendering/drawers/entities/item_drawer.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/drawers/entities/creature_name_drawer.h"
#include "rendering/drawers/tiles/floor_drawer.h"
#include "rendering/drawers/overlays/marker_drawer.h"
#include "rendering/utilities/pattern_calculator.h"
#include "rendering/core/sprite_preloader.h"

#include <algorithm>

TileRenderer::TileRenderer(ItemDrawer* id, SpriteDrawer* sd, CreatureDrawer* cd, CreatureNameDrawer* cnd, FloorDrawer* fd, MarkerDrawer* md, Editor* ed) :
	item_drawer(id), sprite_drawer(sd), creature_drawer(cd), floor_drawer(fd), marker_drawer(md), creature_name_drawer(cnd), editor(ed) {
}

static DrawColor invalidTileOverlayColor(InvalidOTBMItemMarkerColor markerColor, bool selected) {
	uint8_t red = 255;
	uint8_t green = 0;
	uint8_t blue = 0;

	if (markerColor == InvalidOTBMItemMarkerColor::Orange) {
		green = 165;
	}

	if (selected) {
		red = static_cast<uint8_t>(red / 2);
		green = static_cast<uint8_t>(green / 2);
		blue = static_cast<uint8_t>(blue / 2);
	}

	return DrawColor(red, green, blue, 171);
}

void TileRenderer::RenderStaticTerrain(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int draw_x, int draw_y, const Tile* tile_above) const {
	if (!location) {
		return;
	}
	Tile* tile = const_cast<Tile*>(location->get());
	if (!tile) {
		return;
	}

	const auto& view = ctx.view;
	const auto& options = ctx.options;
	const uint32_t current_house_id = ctx.current_house_id;
	const auto& position = location->getPosition();
	const int map_z = position.z;

	ItemDefinitionView ground_it;
	if (tile->ground) {
		ground_it = tile->ground->getDefinition();
	}

	const bool hidden_invalid_ground = tile->ground && tile->ground->isInvalidOTBMItem() && !options.show_invalid_tiles;
	const bool unresolved_invalid_ground = tile->ground && tile->ground->isInvalidOTBMItem() && !ground_it;

	bool as_minimap = options.show_as_minimap;
	bool only_colors = as_minimap || options.show_only_colors;

	uint8_t r = 255, g = 255, b = 255;

	// begin filters for ground tile
	if (!as_minimap && (options.hasTileColorModifiers() || location->getSpawnCount() > 0)) {
		TileColorCalculator::Calculate(tile, options, current_house_id, location->getSpawnCount(), r, g, b);
	}

	if (only_colors) {
		if (as_minimap) {
			TileColorCalculator::GetMinimapColor(tile, r, g, b);
			sprite_drawer->glBlitSquare(sprite_batch, draw_x, draw_y, DrawColor(r, g, b, 255), 0, &ctx.atlas);
		} else if (r != 255 || g != 255 || b != 255) {
			sprite_drawer->glBlitSquare(sprite_batch, draw_x, draw_y, DrawColor(r, g, b, 128), 0, &ctx.atlas);
		}
	} else {
		if (tile->ground && ground_it && !hidden_invalid_ground) {
			GameSprite* ground_sprite = ctx.gfx.getGameSprite(ground_it.clientId());
			if (ground_sprite) {
				SpritePatterns patterns;
				if (!ground_sprite->is_simple) {
					patterns = PatternCalculator::Calculate(ground_sprite, ground_it, tile->ground.get(), tile, position, ctx.elapsed_time);
				}

				// Inline preload check — skip function call when sprite is simple and loaded (95%+ case)
				if (!ground_sprite->isSimpleAndLoaded()) {
					rme::collectTileSprites(ground_sprite, patterns.x, patterns.y, patterns.z, patterns.frame);
				}

				BlitItemParams params(position, tile->ground.get(), options);
				params.tile = tile;
				params.item_definition = ground_it;
				params.sprite = ground_sprite;
				params.red = r;
				params.green = g;
				params.blue = b;
				params.patterns = &patterns;
				params.view = &view;
				params.ctx = &ctx;
				item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
			} else if (!unresolved_invalid_ground) {
				BlitItemParams params(position, tile->ground.get(), options);
				params.tile = tile;
				params.item_definition = ground_it;
				params.red = r;
				params.green = g;
				params.blue = b;
				params.view = &view;
				params.ctx = &ctx;
				item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
			}
		} else if (unresolved_invalid_ground) {
			// Missing-definition ground placeholders are represented by the tile-level invalid overlay.
		} else if (options.always_show_zones && (r != 255 || g != 255 || b != 255)) {
			item_drawer->DrawRawBrush(sprite_batch, sprite_drawer, draw_x, draw_y, SPRITE_ZONE, r, g, b, 60, &ctx);
		}

		// Static ground borders (coastlines, grass edges, sand borders, etc.)
		if (!tile->items.empty()) {
			BlitItemParams border_params(position, nullptr, options);
			border_params.tile = tile;
			border_params.ctx = &ctx;
			border_params.view = &view;
			border_params.red = r;
			border_params.green = g;
			border_params.blue = b;

			for (const auto& item : tile->items) {
				if (!item || !item->isBorder() || item->isInvalidOTBMItem()) {
					continue;
				}
				const ItemDefinitionView it = item->getDefinition();
				if (!it) {
					continue;
				}
				GameSprite* sprite = ctx.gfx.getGameSprite(it.clientId());
				if (!sprite || sprite->isAnimated()) {
					continue;
				}

				SpritePatterns patterns = PatternCalculator::Calculate(sprite, it, item.get(), tile, position, ctx.elapsed_time);
				if (!sprite->isSimpleAndLoaded()) {
					rme::collectTileSprites(sprite, patterns.x, patterns.y, patterns.z, patterns.frame);
				}

				border_params.item = item.get();
				border_params.item_definition = it;
				border_params.sprite = sprite;
				border_params.patterns = &patterns;

				item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, border_params);
			}
		}
	}

	const bool is_house_tile = tile->isHouseTile();

	// Draw helper border for selected house tiles
	// Only draw on the current floor (grid)
	if (options.show_houses && map_z == view.floor && is_house_tile && static_cast<int>(tile->getHouseID()) == current_house_id) {
		uint8_t hr, hg, hb;
		TileColorCalculator::GetHouseColor(tile->getHouseID(), hr, hg, hb);

		float intensity = 0.5f + (0.5f * options.highlight_pulse);
		int ba = static_cast<int>(intensity * 255.0f);
		sprite_drawer->glDrawBox(sprite_batch, draw_x, draw_y, 32, 32, DrawColor(hr, hg, hb, ba), &ctx.atlas);
	}
}

void TileRenderer::RenderStaticItems(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, TileElevationState& elevation) const {
	if (!location) {
		return;
	}
	Tile* tile = const_cast<Tile*>(location->get());
	if (!tile || tile->items.empty()) {
		return;
	}

	const auto& view = ctx.view;
	const auto& options = ctx.options;
	const auto& position = location->getPosition();
	const bool as_minimap = options.show_as_minimap;
	const bool only_colors = as_minimap || options.show_only_colors;
	if (only_colors) {
		return;
	}

	const bool is_house_tile = tile->isHouseTile();
	uint8_t default_ir = 255, default_ig = 255, default_ib = 255;
	bool calculate_house_color = options.extended_house_shader && options.show_houses && is_house_tile;
	if (calculate_house_color) {
		uint8_t house_r = 255, house_g = 255, house_b = 255;
		TileColorCalculator::GetHouseColor(tile->getHouseID(), house_r, house_g, house_b);
		default_ir = house_r;
		default_ig = house_g;
		default_ib = house_b;
		if ((static_cast<int>(tile->getHouseID()) == ctx.current_house_id) && (options.highlight_pulse > 0.0f)) {
			float boost = options.highlight_pulse * 0.6f;
			default_ir = static_cast<uint8_t>(std::min(255, static_cast<int>(default_ir + (255 - default_ir) * boost)));
			default_ig = static_cast<uint8_t>(std::min(255, static_cast<int>(default_ig + (255 - default_ig) * boost)));
			default_ib = static_cast<uint8_t>(std::min(255, static_cast<int>(default_ib + (255 - default_ib) * boost)));
		}
	}

	uint8_t r = 255, g = 255, b = 255;
	if (options.hasTileColorModifiers() || location->getSpawnCount() > 0) {
		TileColorCalculator::Calculate(tile, options, ctx.current_house_id, location->getSpawnCount(), r, g, b);
	}

	BlitItemParams item_params(position, nullptr, options);
	item_params.tile = tile;
	item_params.ctx = &ctx;
	item_params.view = &view;

	for (const auto& item : tile->items) {
		if (item->isBorder()) {
			continue;
		}
		const ItemDefinitionView it = item->getDefinition();
		if (item->isInvalidOTBMItem() && (!options.show_invalid_tiles || !it)) {
			continue;
		}

		GameSprite* sprite = it ? ctx.gfx.getGameSprite(it.clientId()) : nullptr;
		if (sprite && sprite->isAnimated()) {
			if (sprite->hasElevation()) {
				elevation.current_draw_x -= sprite->draw_height;
				elevation.current_draw_y -= sprite->draw_height;
			}
			continue; // Processed in RenderAnimatedItems
		}

		if (sprite) {
			SpritePatterns patterns = PatternCalculator::Calculate(sprite, it, item.get(), tile, position, ctx.elapsed_time);

			if (!sprite->isSimpleAndLoaded()) {
				rme::collectTileSprites(sprite, patterns.x, patterns.y, patterns.z, patterns.frame);
			}

			item_params.item = item.get();
			item_params.item_definition = it;
			item_params.sprite = sprite;
			item_params.patterns = &patterns;
			item_params.red = default_ir;
			item_params.green = default_ig;
			item_params.blue = default_ib;

			item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, elevation.current_draw_x, elevation.current_draw_y, item_params);
		}
	}
}

void TileRenderer::RenderAnimatedItems(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, TileElevationState& elevation) const {
	if (!location) {
		return;
	}
	Tile* tile = const_cast<Tile*>(location->get());
	if (!tile || tile->items.empty()) {
		return;
	}

	const auto& view = ctx.view;
	const auto& options = ctx.options;
	const auto& position = location->getPosition();
	const bool as_minimap = options.show_as_minimap;
	const bool only_colors = as_minimap || options.show_only_colors;
	if (only_colors) {
		return;
	}

	const bool is_house_tile = tile->isHouseTile();
	uint8_t default_ir = 255, default_ig = 255, default_ib = 255;
	bool calculate_house_color = options.extended_house_shader && options.show_houses && is_house_tile;
	if (calculate_house_color) {
		uint8_t house_r = 255, house_g = 255, house_b = 255;
		TileColorCalculator::GetHouseColor(tile->getHouseID(), house_r, house_g, house_b);
		default_ir = house_r;
		default_ig = house_g;
		default_ib = house_b;
		if ((static_cast<int>(tile->getHouseID()) == ctx.current_house_id) && (options.highlight_pulse > 0.0f)) {
			float boost = options.highlight_pulse * 0.6f;
			default_ir = static_cast<uint8_t>(std::min(255, static_cast<int>(default_ir + (255 - default_ir) * boost)));
			default_ig = static_cast<uint8_t>(std::min(255, static_cast<int>(default_ig + (255 - default_ig) * boost)));
			default_ib = static_cast<uint8_t>(std::min(255, static_cast<int>(default_ib + (255 - default_ib) * boost)));
		}
	}

	uint8_t r = 255, g = 255, b = 255;
	if (options.hasTileColorModifiers() || location->getSpawnCount() > 0) {
		TileColorCalculator::Calculate(tile, options, ctx.current_house_id, location->getSpawnCount(), r, g, b);
	}

	BlitItemParams item_params(position, nullptr, options);
	item_params.tile = tile;
	item_params.ctx = &ctx;
	item_params.view = &view;

	for (const auto& item : tile->items) {
		const ItemDefinitionView it = item->getDefinition();
		if (item->isInvalidOTBMItem() && (!options.show_invalid_tiles || !it)) {
			continue;
		}

		GameSprite* sprite = it ? ctx.gfx.getGameSprite(it.clientId()) : nullptr;
		if (!sprite || !sprite->isAnimated()) {
			if (sprite && sprite->hasElevation()) {
				elevation.current_draw_x -= sprite->draw_height;
				elevation.current_draw_y -= sprite->draw_height;
			}
			continue; // Only animated items in this pass
		}

		SpritePatterns patterns = PatternCalculator::Calculate(sprite, it, item.get(), tile, position, ctx.elapsed_time);

		if (!sprite->isSimpleAndLoaded()) {
			rme::collectTileSprites(sprite, patterns.x, patterns.y, patterns.z, patterns.frame);
		}

		item_params.item = item.get();
		item_params.item_definition = it;
		item_params.sprite = sprite;
		item_params.patterns = &patterns;

		if (item->isBorder()) {
			item_params.red = r;
			item_params.green = g;
			item_params.blue = b;
		} else {
			item_params.red = default_ir;
			item_params.green = default_ig;
			item_params.blue = default_ib;
		}

		item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, elevation.current_draw_x, elevation.current_draw_y, item_params);
	}
}

void TileRenderer::RenderDynamicEntities(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int draw_x, int draw_y) const {
	if (!location) {
		return;
	}
	Tile* tile = const_cast<Tile*>(location->get());
	if (!tile) {
		return;
	}

	const auto& view = ctx.view;
	const auto& options = ctx.options;
	const auto& position = location->getPosition();

	const bool as_minimap = options.show_as_minimap;
	const bool only_colors = as_minimap || options.show_only_colors;

	if (!only_colors) {
		// monster/npc on tile
		if (tile->creature && options.show_creatures) {
			creature_drawer->BlitCreature(sprite_batch, sprite_drawer, draw_x, draw_y, tile->creature.get(), CreatureDrawOptions {
				.map_pos = position,
				.transient_selection_bounds = options.transient_selection_bounds,
				.view = &view,
				.ctx = &ctx
			});
			if (creature_name_drawer) {
				creature_name_drawer->addLabel(position, tile->creature->getName(), tile->creature.get());
			}
		}

		if (options.show_invalid_zones && !as_minimap && tile->hasInvalidZones()) {
			sprite_drawer->glBlitSquare(sprite_batch, draw_x, draw_y, DrawColor(255, 0, 255, 171), 0, &ctx.atlas);
		}

		InvalidOTBMItemMarkerColor invalid_tile_marker_color = InvalidOTBMItemMarkerColor::None;
		bool has_selected_invalid_item = false;
		if (options.show_invalid_tiles && tile->ground && tile->ground->isInvalidOTBMItem()) {
			invalid_tile_marker_color = tile->ground->invalidOTBMMarkerColor();
			has_selected_invalid_item = tile->ground->isSelected();
		}
		if (options.show_invalid_tiles) {
			for (const auto& item : tile->items) {
				if (item->isInvalidOTBMItem()) {
					if (invalid_tile_marker_color != InvalidOTBMItemMarkerColor::Red) {
						invalid_tile_marker_color = item->invalidOTBMMarkerColor();
					}
					has_selected_invalid_item = has_selected_invalid_item || item->isSelected();
				}
			}
		}

		if (options.show_invalid_tiles && !as_minimap && invalid_tile_marker_color != InvalidOTBMItemMarkerColor::None) {
			const DrawColor overlay = invalidTileOverlayColor(invalid_tile_marker_color, has_selected_invalid_item);
			sprite_drawer->glBlitSquare(sprite_batch, draw_x, draw_y, overlay, 0, &ctx.atlas);
		}

		const bool need_waypoint = !options.ingame && options.show_waypoints;
		const Waypoint* waypoint = nullptr;
		if (need_waypoint && location->getWaypointCount() > 0 && editor) {
			waypoint = editor->map.waypoints.getWaypoint(location);
		}

		// markers (waypoint, house exit, town temple, spawn)
		if (editor) {
			marker_drawer->draw(sprite_batch, sprite_drawer, draw_x, draw_y, tile, waypoint, ctx.current_house_id, editor->map, options, ctx);
		}
	}
}

void TileRenderer::DrawTile(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int in_draw_x, int in_draw_y, const Tile* tile_above) const {
	if (!location) {
		return;
	}
	Tile* tile = const_cast<Tile*>(location->get());
	if (!tile) {
		return;
	}

	const auto& view = ctx.view;
	const auto& options = ctx.options;

	if (options.show_only_modified && !tile->isModified()) {
		return;
	}

	const auto& position = location->getPosition();
	int draw_x, draw_y;
	if (in_draw_x != -1 && in_draw_y != -1) {
		draw_x = in_draw_x;
		draw_y = in_draw_y;
	} else {
		// Early viewport culling - skip tiles that are completely off-screen
		if (!view.IsTileVisible(position.x, position.y, position.z, draw_x, draw_y)) {
			return;
		}
	}

	RenderStaticTerrain(sprite_batch, location, ctx, draw_x, draw_y, tile_above);

	TileElevationState static_elevation { draw_x, draw_y };
	RenderStaticItems(sprite_batch, location, ctx, static_elevation);

	TileElevationState animated_elevation { draw_x, draw_y };
	RenderAnimatedItems(sprite_batch, location, ctx, animated_elevation);

	RenderDynamicEntities(sprite_batch, location, ctx, draw_x, draw_y);
}

void TileRenderer::RenderDynamicPasses(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int draw_x, int draw_y, const Tile* tile_above) const {
	if (!location) {
		return;
	}
	Tile* tile = const_cast<Tile*>(location->get());
	if (!tile) {
		return;
	}

	const auto& options = ctx.options;
	if (options.show_only_modified && !tile->isModified()) {
		return;
	}

	TileElevationState animated_elevation { draw_x, draw_y };
	RenderAnimatedItems(sprite_batch, location, ctx, animated_elevation);
	RenderDynamicEntities(sprite_batch, location, ctx, draw_x, draw_y);
}
