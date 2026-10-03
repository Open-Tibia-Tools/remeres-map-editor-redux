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
#include "rendering/core/atlas_manager.h"
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
#include "rendering/indicators/technical_item_registry.h"
#include "rendering/indicators/zone_flags.h"
#include "rendering/utilities/pattern_calculator.h"
#include "rendering/core/sprite_preloader.h"
#include "rendering/core/render_depth.h"

#include <algorithm>

TileRenderer::TileRenderer(ItemDrawer* id, SpriteDrawer* sd, CreatureDrawer* cd, CreatureNameDrawer* cnd, FloorDrawer* fd, MarkerDrawer* md, Editor* ed) :
	item_drawer(id), sprite_drawer(sd), creature_drawer(cd), floor_drawer(fd), marker_drawer(md), creature_name_drawer(cnd), editor(ed) {
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
	const bool is_house_tile = tile->isHouseTile();

	uint8_t r = 255, g = 255, b = 255;

	// begin filters for ground tile
	if (!as_minimap && options.hasTileColorModifiers()) {
		TileColorCalculator::Calculate(tile, options, r, g, b);
	}

	const float ground_depth = rme::rendering::calculateTileDepth(position.x, position.y, rme::rendering::RenderSublayer::Ground);
	const float border_depth = rme::rendering::calculateTileDepth(position.x, position.y, rme::rendering::RenderSublayer::Border);

	if (only_colors) {
		if (as_minimap) {
			TileColorCalculator::GetMinimapColor(tile, r, g, b);
			sprite_drawer->glBlitSquare(sprite_batch, draw_x, draw_y, DrawColor(r, g, b, 255), 0, &ctx.atlas, ground_depth);
		} else if (r != 255 || g != 255 || b != 255) {
			sprite_drawer->glBlitSquare(sprite_batch, draw_x, draw_y, DrawColor(r, g, b, 128), 0, &ctx.atlas, ground_depth);
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

				const float ground_house_id = (options.show_houses && is_house_tile) ? -static_cast<float>(tile->getHouseID()) : 0.0f;

				BlitItemParams params(position, tile->ground.get(), options);
				params.tile = tile;
				params.item_definition = ground_it;
				params.sprite = ground_sprite;
				params.red = r;
				params.green = g;
				params.blue = b;
				params.house_id = ground_house_id;
				params.zone_flags = 0;
				params.depth = ground_depth;
				params.patterns = &patterns;
				params.view = &view;
				params.ctx = &ctx;
				item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
			} else if (!unresolved_invalid_ground) {
				const float ground_house_id = (options.show_houses && is_house_tile) ? -static_cast<float>(tile->getHouseID()) : 0.0f;

				BlitItemParams params(position, tile->ground.get(), options);
				params.tile = tile;
				params.item_definition = ground_it;
				params.red = r;
				params.green = g;
				params.blue = b;
				params.house_id = ground_house_id;
				params.zone_flags = 0;
				params.depth = ground_depth;
				params.view = &view;
				params.ctx = &ctx;
				item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
			}
		} else if (unresolved_invalid_ground) {
			// Missing-definition ground placeholders are represented by the tile-level invalid overlay.
		}

		// Static ground borders (coastlines, grass edges, sand borders, etc.)
		if (!tile->items.empty()) {
			const float ground_house_id = (options.show_houses && is_house_tile) ? -static_cast<float>(tile->getHouseID()) : 0.0f;

			BlitItemParams border_params(position, nullptr, options);
			border_params.tile = tile;
			border_params.ctx = &ctx;
			border_params.view = &view;
			border_params.red = r;
			border_params.green = g;
			border_params.blue = b;
			border_params.house_id = ground_house_id;
			border_params.zone_flags = 0;
			border_params.depth = border_depth;
			for (const auto& item : tile->items) {
				if (!item || !item->isBorder() || item->isInvalidOTBMItem()) {
					continue;
				}
				const ItemDefinitionView it = item->getDefinition();
				if (!it) {
					continue;
				}

				GameSprite* sprite = ctx.gfx.getGameSprite(it.clientId());
				if (!sprite) {
					if (options.show_tech_items && !options.ingame) {
						border_params.item = item.get();
						border_params.item_definition = it;
						border_params.sprite = nullptr;
						border_params.patterns = nullptr;
						item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, border_params);
					}
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
	const float item_house_id = (options.show_houses && options.extended_house_shader && is_house_tile) ? static_cast<float>(tile->getHouseID()) : 0.0f;
	constexpr uint8_t default_ir = 255, default_ig = 255, default_ib = 255;

	uint8_t r = 255, g = 255, b = 255;
	if (options.hasTileColorModifiers()) {
		TileColorCalculator::Calculate(tile, options, r, g, b);
	}

	BlitItemParams item_params(position, nullptr, options);
	item_params.tile = tile;
	item_params.ctx = &ctx;
	item_params.view = &view;
	item_params.house_id = item_house_id;

	int elevation_step = 0;
	for (const auto& item : tile->items) {
		if (item->isBorder()) {
			continue;
		}
		const ItemDefinitionView it = item->getDefinition();
		if (item->isInvalidOTBMItem() && (!options.show_invalid_tiles || !it)) {
			continue;
		}

		rme::rendering::RenderSublayer sublayer = rme::rendering::RenderSublayer::CommonItem;
		if (item->isAlwaysOnBottom()) {
			sublayer = rme::rendering::RenderSublayer::BottomItem;
		} else if (it && it.hasFlag(ItemFlag::TopEffect)) {
			sublayer = rme::rendering::RenderSublayer::TopItem;
		}
		const float item_depth = rme::rendering::calculateTileDepth(position.x, position.y, sublayer, elevation_step);
		if (sublayer == rme::rendering::RenderSublayer::CommonItem) {
			elevation_step++;
		}
		item_params.depth = item_depth;

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
			const bool is_blocking_item = item->isBlocking() &&
				(item->getID() != 1548 && it.clientId() != 2187) &&
				(rme::rendering::TechnicalItemRegistry::Classify(item->getID(), it.clientId()) != rme::rendering::TileIndicatorType::TechInvisibleWall);
			item_params.zone_flags = (options.show_blocking && options.extended_pathing_shader && is_blocking_item)
				? rme::rendering::ZONE_FLAG_ITEM_BLOCKING
				: 0;

			item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, elevation.current_draw_x, elevation.current_draw_y, item_params);
		} else if (it && options.show_tech_items && !options.ingame) {
			item_params.item = item.get();
			item_params.item_definition = it;
			item_params.sprite = nullptr;
			item_params.patterns = nullptr;
			item_params.red = default_ir;
			item_params.green = default_ig;
			item_params.blue = default_ib;
			item_params.zone_flags = 0;

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
	const float item_house_id = (options.show_houses && options.extended_house_shader && is_house_tile) ? static_cast<float>(tile->getHouseID()) : 0.0f;
	constexpr uint8_t default_ir = 255, default_ig = 255, default_ib = 255;

	uint8_t r = 255, g = 255, b = 255;
	if (options.hasTileColorModifiers()) {
		TileColorCalculator::Calculate(tile, options, r, g, b);
	}

	BlitItemParams item_params(position, nullptr, options);
	item_params.tile = tile;
	item_params.ctx = &ctx;
	item_params.view = &view;
	item_params.house_id = item_house_id;

	int anim_elevation_step = 0;
	for (const auto& item : tile->items) {
		if (item->isBorder()) {
			continue;
		}
		const ItemDefinitionView it = item->getDefinition();
		if (item->isInvalidOTBMItem() && (!options.show_invalid_tiles || !it)) {
			continue;
		}

		rme::rendering::RenderSublayer sublayer = rme::rendering::RenderSublayer::CommonItem;
		if (item->isAlwaysOnBottom()) {
			sublayer = rme::rendering::RenderSublayer::BottomItem;
		} else if (it && it.hasFlag(ItemFlag::TopEffect)) {
			sublayer = rme::rendering::RenderSublayer::TopItem;
		}
		const float item_depth = rme::rendering::calculateTileDepth(position.x, position.y, sublayer, anim_elevation_step);
		if (sublayer == rme::rendering::RenderSublayer::CommonItem) {
			anim_elevation_step++;
		}
		item_params.depth = item_depth;

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
		item_params.red = default_ir;
		item_params.green = default_ig;
		const bool is_blocking_item = item->isBlocking() &&
			(item->getID() != 1548 && it.clientId() != 2187) &&
			(rme::rendering::TechnicalItemRegistry::Classify(item->getID(), it.clientId()) != rme::rendering::TileIndicatorType::TechInvisibleWall);
		item_params.zone_flags = (options.show_blocking && options.extended_pathing_shader && is_blocking_item)
			? rme::rendering::ZONE_FLAG_ITEM_BLOCKING
			: 0;

		item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, elevation.current_draw_x, elevation.current_draw_y, item_params);
	}
}

void TileRenderer::RenderDynamicEntities(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int draw_x, int draw_y, bool render_creature_sprites, bool render_markers) const {
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
		const float creature_depth = rme::rendering::calculateTileDepth(position.x, position.y, rme::rendering::RenderSublayer::DynamicEntity);
		const float overlay_depth = rme::rendering::calculateTileDepth(position.x, position.y, rme::rendering::RenderSublayer::Overlay);

		// monster/npc on tile
		if (tile->creature && options.show_creatures) {
			if (render_creature_sprites) {
				creature_drawer->BlitCreature(sprite_batch, sprite_drawer, draw_x, draw_y, tile->creature.get(), CreatureDrawOptions {
					.map_pos = position,
					.transient_selection_bounds = options.transient_selection_bounds,
					.view = &view,
					.ctx = &ctx,
					.depth = creature_depth
				});
			}
			if (creature_name_drawer) {
				creature_name_drawer->addLabel(position, tile->creature->getName(), tile->creature.get());
			}
		}

		const AtlasRegion* white_pixel = ctx.atlas.getWhitePixel();

		if (options.show_invalid_zones && !as_minimap && tile->hasInvalidZones() && white_pixel) {
			sprite_batch.draw(
				static_cast<float>(draw_x), static_cast<float>(draw_y),
				32.0f, 32.0f,
				*white_pixel,
				1.0f, 1.0f, 1.0f, 1.0f,
				rme::rendering::INDICATOR_INVALID_ZONE_BASE,
				0u,
				overlay_depth
			);
		}

		if (options.show_invalid_tiles && !as_minimap && white_pixel) {
			rme::rendering::TileIndicatorType invalid_indicator = rme::rendering::TileIndicatorType::None;
			bool has_selected_invalid_item = false;

			if (tile->ground && tile->ground->isInvalidOTBMItem()) {
				invalid_indicator = (tile->ground->invalidOTBMMarkerColor() == InvalidOTBMItemMarkerColor::Orange)
					? rme::rendering::TileIndicatorType::InvalidItem
					: rme::rendering::TileIndicatorType::InvalidGround;
				has_selected_invalid_item = tile->ground->isSelected();
			}

			for (const auto& item : tile->items) {
				if (item->isInvalidOTBMItem()) {
					if (invalid_indicator != rme::rendering::TileIndicatorType::InvalidGround) {
						invalid_indicator = (item->invalidOTBMMarkerColor() == InvalidOTBMItemMarkerColor::Red)
							? rme::rendering::TileIndicatorType::InvalidGround
							: rme::rendering::TileIndicatorType::InvalidItem;
					}
					has_selected_invalid_item = has_selected_invalid_item || item->isSelected();
				}
			}

			if (invalid_indicator != rme::rendering::TileIndicatorType::None) {
				if (!has_selected_invalid_item && options.transient_selection_bounds) {
					has_selected_invalid_item = options.transient_selection_bounds->contains(position.x, position.y);
				}

				const float tint = has_selected_invalid_item ? 0.5f : 1.0f;
				const float marker_id = rme::rendering::TechnicalItemRegistry::GetMarkerId(invalid_indicator);
				sprite_batch.draw(
					static_cast<float>(draw_x), static_cast<float>(draw_y),
					32.0f, 32.0f,
					*white_pixel,
					tint, tint, tint, 1.0f,
					marker_id,
					0u,
					overlay_depth
				);
			}
		}

		const bool need_waypoint = !options.ingame && options.show_waypoints;
		const Waypoint* waypoint = nullptr;
		if (need_waypoint && location->getWaypointCount() > 0 && editor) {
			waypoint = editor->map.waypoints.getWaypoint(location);
		}

		// markers (waypoint, house exit, town temple, spawn)
		if (editor && render_markers) {
			marker_drawer->draw(sprite_batch, draw_x, draw_y, tile, waypoint, options, ctx, overlay_depth);
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

	RenderDynamicEntities(sprite_batch, location, ctx, draw_x, draw_y, /*render_creature_sprites=*/true, /*render_markers=*/true);
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

	RenderDynamicEntities(sprite_batch, location, ctx, draw_x, draw_y, /*render_creature_sprites=*/false, /*render_markers=*/false);
}


