#include "rendering/drawers/tiles/tile_renderer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/primitive_renderer.h"

#include "editor/editor.h"
#include "map/tile.h"
#include "map/map_region.h"
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
#include "rendering/ui/tooltip_drawer.h"
#include "rendering/core/light_buffer.h"
#include "rendering/core/sprite_preloader.h"
#include "rendering/utilities/pattern_calculator.h"

#include <ranges>

TileRenderer::TileRenderer(ItemDrawer* id, SpriteDrawer* sd, CreatureDrawer* cd, CreatureNameDrawer* cnd, FloorDrawer* fd, MarkerDrawer* md, TooltipDrawer* td, Editor* ed) :
	item_drawer(id), sprite_drawer(sd), creature_drawer(cd), floor_drawer(fd), marker_drawer(md), tooltip_drawer(td), creature_name_drawer(cnd), editor(ed) {
}

namespace {
	[[nodiscard]] int projectedFloorOffsetTiles(const RenderView& view, int map_z) {
		if (map_z <= GROUND_LAYER) {
			return GROUND_LAYER - map_z;
		}
		return view.floor - map_z;
	}

	[[nodiscard]] std::pair<int, int> projectedTilePosition(const RenderView& view, const Position& position) {
		const int offset_tiles = projectedFloorOffsetTiles(view, position.z);
		return { position.x - offset_tiles, position.y - offset_tiles };
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

// Helper function to populate tooltip data from an item (in-place)
static bool FillItemTooltipData(TooltipData& data, Item* item, const ItemDefinitionView& it, const Position& pos, bool isHouseTile, float zoom) {
	if (!item) {
		return false;
	}

	const uint16_t id = item->getID();
	if (id < 100) {
		return false;
	}

	uint16_t unique = 0;
	uint16_t action = 0;
	std::string_view text;
	std::string_view description;
	uint8_t doorId = 0;
	Position destination;
	bool hasContent = false;

	bool is_complex = item->isComplex();
	// Early exit for simple items
	// isTooltipable is cached (isContainer || isDoor || isTeleport)
	if (!is_complex && !it.isTooltipable()) {
		return false;
	}

	bool is_container = it.isContainer();
	bool is_door = isHouseTile && item->isDoor();
	bool is_teleport = item->isTeleport();

	if (is_complex) {
		unique = item->getUniqueID();
		action = item->getActionID();
		text = item->getText();
		description = item->getDescription();
	}

	// Check if it's a door
	if (is_door) {
		if (const Door* door = item->asDoor()) {
			if (door->isRealDoor()) {
				doorId = door->getDoorID();
			}
		}
	}

	// Check if it's a teleport
	if (is_teleport) {
		Teleport* tp = static_cast<Teleport*>(item);
		if (tp->hasDestination()) {
			destination = tp->getDestination();
		}
	}

	// Check if container has content
	if (is_container) {
		if (const Container* container = item->asContainer()) {
			hasContent = container->getItemCount() > 0;
		}
	}

	// Only create tooltip if there's something to show
	if (unique == 0 && action == 0 && doorId == 0 && text.empty() && description.empty() && destination.x == 0 && !hasContent) {
		return false;
	}

	// Get item name from database
	std::string_view itemName = it.name();
	if (itemName.empty()) {
		itemName = "Item";
	}

	data.pos = pos;
	data.itemId = id;
	data.itemName = itemName; // Assign string_view to string_view (no copy)

	data.actionId = action;
	data.uniqueId = unique;
	data.doorId = doorId;
	data.text = text;
	data.description = description;
	data.destination = destination;

	// Populate container items
	if (it.isContainer() && zoom <= 1.5f) {
		if (const Container* container = item->asContainer()) {
			// Set capacity for rendering empty slots
			data.containerCapacity = static_cast<uint8_t>(container->getVolume());

			const auto& items = container->getVector();
			data.containerItems.clear();
			// Reserve only what we need (capped at 32)
			data.containerItems.reserve(std::min(items.size(), size_t(32)));
			for (const auto& subItem : items) {
				if (subItem) {
					ContainerItem ci;
					ci.id = subItem->getID();
					ci.subtype = subItem->getSubtype();
					ci.count = subItem->getCount();
					// Sanity check for count
					if (ci.count == 0) {
						ci.count = 1;
					}

					data.containerItems.push_back(ci);

					// Limit preview items to avoid massive tooltips
					if (data.containerItems.size() >= 32) {
						break;
					}
				}
			}
		}
	}

	data.updateCategory();

	return true;
}

void TileRenderer::RegisterGroundLightOcclusion(const TileLocation* location, const RenderView& view, LightBuffer& light_buffer, uint32_t floor_light_start) const {
	if (!location) {
		return;
	}

	const Tile* tile = location->get();
	if (!tile || !tile->ground || !tile->ground->blocksLightFromBelow()) {
		return;
	}

	const auto [tile_x, tile_y] = projectedTilePosition(view, location->getPosition());
	light_buffer.SetFieldBrightness(tile_x, tile_y, floor_light_start);
}

void TileRenderer::DrawTile(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int in_draw_x, int in_draw_y, LightBuffer* light_buffer, bool light_collection_only, const Tile* tile_above) const {
	if (!location) {
		return;
	}
	Tile* tile = const_cast<Tile*>(location->get());

	if (!tile) {
		return;
	}

	const auto& view = ctx.view;
	const auto& options = ctx.options;
	uint32_t current_house_id = ctx.current_house_id;

	if (options.show_only_modified && !tile->isModified()) {
		return;
	}

	const auto& position = location->getPosition();
	const int map_x = position.x;
	const int map_y = position.y;
	const int map_z = position.z;

	int draw_x, draw_y;
	if (in_draw_x != -1 && in_draw_y != -1) {
		draw_x = in_draw_x;
		draw_y = in_draw_y;
	} else {
		// Early viewport culling - skip tiles that are completely off-screen
		if (!view.IsTileVisible(map_x, map_y, map_z, draw_x, draw_y)) {
			return;
		}
	}

	const int tile_draw_x = draw_x;
	const int tile_draw_y = draw_y;

	if (light_buffer) {
		const auto [projected_tile_x, projected_tile_y] = projectedTilePosition(view, position);

		if (!light_collection_only && tile->ground && tile->ground->blocksLightFromBelow()) {
			light_buffer->SetFieldBrightness(projected_tile_x, projected_tile_y, light_buffer->current_floor_light_start);
		}

		// Translucent light: when on floor 8 (GROUND_LAYER + 1), check if floor 7 above has translucent items
		// OTClient: light seeps through translucent ground (grates, windows) from floor 7 to floor 8
		if (position.z == GROUND_LAYER + 1) {
			const Tile* above_tile = tile_above;
			if (!above_tile && editor) {
				Position above_position = position;
				--above_position.z;
				above_tile = editor->map.getTile(above_position);
			}
			if (tileCarriesTranslucentLight(above_tile)) {
				// Emit faint warm white light (intensity=1, color=215) matching OTClient
				light_buffer->AddTileLight(projected_tile_x, projected_tile_y, SpriteLight {
					.intensity = 1,
					.color = 215
				});
			}
		}
	}

	ItemDefinitionView ground_it;
	if (tile->ground) {
		ground_it = tile->ground->getDefinition();
	}

	const bool hidden_invalid_ground = tile->ground && tile->ground->isInvalidOTBMItem() && !options.show_invalid_tiles;
	const bool unresolved_invalid_ground = tile->ground && tile->ground->isInvalidOTBMItem() && !ground_it;

	if (light_collection_only) {
		if (light_buffer && tile->ground && ground_it && !hidden_invalid_ground && !unresolved_invalid_ground && tile->ground->hasLight()) {
			int ground_draw_x = draw_x;
			int ground_draw_y = draw_y;
			BlitItemParams params(position, tile->ground.get(), options);
			params.tile = tile;
			params.item_definition = ground_it;
			params.light_collection_only = true;
			params.light_buffer = light_buffer;
			params.view = &view;
			params.ctx = &ctx;
			item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, ground_draw_x, ground_draw_y, params);
		}

		if (light_buffer && !tile->items.empty()) {
			BlitItemParams params(position, nullptr, options);
			params.tile = tile;
			params.light_collection_only = true;
			params.light_buffer = light_buffer;
			params.view = &view;
			params.ctx = &ctx;

			for (const auto& item : tile->items) {
				if (!item->hasLight()) {
					continue;
				}
				const ItemDefinitionView it = item->getDefinition();
				if (item->isInvalidOTBMItem() && (!options.show_invalid_tiles || !it)) {
					continue;
				}

				int item_draw_x = draw_x;
				int item_draw_y = draw_y;
				params.item = item.get();
				params.item_definition = it;
				item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, item_draw_x, item_draw_y, params);
			}
		}

		if (light_buffer && tile->creature && options.show_creatures) {
			creature_drawer->BlitCreature(sprite_batch, sprite_drawer, draw_x, draw_y, tile->creature.get(), CreatureDrawOptions {
				.map_pos = position,
				.transient_selection_bounds = options.transient_selection_bounds,
				.light_buffer = light_buffer,
				.view = &view,
				.light_collection_only = true,
				.ctx = &ctx
			});
		}

		return;
	}

	const bool need_waypoint = (options.show_tooltips && map_z == view.floor) || (view.zoom < 10.0 && !options.ingame && options.show_waypoints);
	const Waypoint* waypoint = nullptr;
	if (need_waypoint && location->getWaypointCount() > 0) {
		waypoint = editor->map.waypoints.getWaypoint(location);
	}

	// Waypoint tooltip (one per waypoint)
	if (options.show_tooltips && waypoint && map_z == view.floor) {
		tooltip_drawer->addWaypointTooltip(position, waypoint->name);
	}

	bool as_minimap = options.show_as_minimap;
	bool only_colors = as_minimap || options.show_only_colors;

	uint8_t r = 255, g = 255, b = 255;

	// begin filters for ground tile
	if (!as_minimap && (options.hasTileColorModifiers() || location->getSpawnCount() > 0)) {
		TileColorCalculator::Calculate(tile, options, current_house_id, location->getSpawnCount(), r, g, b);
	}

	InvalidOTBMItemMarkerColor invalid_tile_marker_color = InvalidOTBMItemMarkerColor::None;
	bool has_selected_invalid_item = false;

	if (options.show_invalid_tiles && tile->ground && tile->ground->isInvalidOTBMItem()) {
		invalid_tile_marker_color = tile->ground->invalidOTBMMarkerColor();
		has_selected_invalid_item = tile->ground->isSelected();
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
				params.light_buffer = light_buffer;
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
				params.light_buffer = light_buffer;
				params.view = &view;
				params.ctx = &ctx;
				item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, params);
			}
		} else if (unresolved_invalid_ground) {
			// Missing-definition ground placeholders are represented by the tile-level invalid overlay.
		} else if (options.always_show_zones && (r != 255 || g != 255 || b != 255)) {
			item_drawer->DrawRawBrush(sprite_batch, sprite_drawer, draw_x, draw_y, SPRITE_ZONE, r, g, b, 60, &ctx);
		}
	}

	// Cache isHouseTile â€” used multiple times below
	const bool is_house_tile = tile->isHouseTile();

	// Ground tooltip (one per item)
	if (options.show_tooltips && map_z == view.floor && tile->ground && ground_it) {
		TooltipData& groundData = tooltip_drawer->requestTooltipData();
		if (FillItemTooltipData(groundData, tile->ground.get(), ground_it, position, is_house_tile, view.zoom)) {
			if (groundData.hasVisibleFields()) {
				tooltip_drawer->commitTooltip();
			}
		}
	}

	// end filters for ground tile

	// Draw helper border for selected house tiles
	// Only draw on the current floor (grid)
	if (options.show_houses && map_z == view.floor && is_house_tile && static_cast<int>(tile->getHouseID()) == current_house_id) {

		uint8_t hr, hg, hb;
		TileColorCalculator::GetHouseColor(tile->getHouseID(), hr, hg, hb);

		float intensity = 0.5f + (0.5f * options.highlight_pulse);
		// Optimization: Use integer math for border color to avoid vec4 construction and casting
		int ba = static_cast<int>(intensity * 255.0f);
		// hr, hg, hb are already uint8_t
		sprite_drawer->glDrawBox(sprite_batch, draw_x, draw_y, 32, 32, DrawColor(hr, hg, hb, ba), &ctx.atlas);
	}

	if (!only_colors) {
		if (view.zoom < 10.0 || !options.hide_items_when_zoomed) {
			// Hoist house color calculation out of item loop
			uint8_t default_ir = 255, default_ig = 255, default_ib = 255;
			bool calculate_house_color = options.extended_house_shader && options.show_houses && is_house_tile;
			if (calculate_house_color) {
				uint8_t house_r = 255, house_g = 255, house_b = 255;
				TileColorCalculator::GetHouseColor(tile->getHouseID(), house_r, house_g, house_b);
				default_ir = house_r;
				default_ig = house_g;
				default_ib = house_b;
				if ((static_cast<int>(tile->getHouseID()) == current_house_id) && (options.highlight_pulse > 0.0f)) {
					float boost = options.highlight_pulse * 0.6f;
					default_ir = static_cast<uint8_t>(std::min(255, static_cast<int>(default_ir + (255 - default_ir) * boost)));
					default_ig = static_cast<uint8_t>(std::min(255, static_cast<int>(default_ig + (255 - default_ig) * boost)));
					default_ib = static_cast<uint8_t>(std::min(255, static_cast<int>(default_ib + (255 - default_ib) * boost)));
				}
			}

			bool process_tooltips = options.show_tooltips && map_z == view.floor;

			// items on tile
			BlitItemParams item_params(position, nullptr, options);
			item_params.tile = tile;
			item_params.ctx = &ctx;
			item_params.light_buffer = light_buffer;
			item_params.view = &view;

			for (const auto& item : tile->items) {
				if (item->isInvalidOTBMItem() && options.show_invalid_tiles) {
					if (invalid_tile_marker_color != InvalidOTBMItemMarkerColor::Red) {
						invalid_tile_marker_color = item->invalidOTBMMarkerColor();
					}
					has_selected_invalid_item = has_selected_invalid_item || item->isSelected();
				}

				const ItemDefinitionView it = item->getDefinition();
				if (item->isInvalidOTBMItem() && (!options.show_invalid_tiles || !it)) {
					// Missing-definition placeholders are represented by the tile-level invalid overlay.
					continue;
				}

				// item tooltip (one per item)
				if (process_tooltips) {
					TooltipData& itemData = tooltip_drawer->requestTooltipData();
					if (FillItemTooltipData(itemData, item.get(), it, position, is_house_tile, view.zoom)) {
						if (itemData.hasVisibleFields()) {
							tooltip_drawer->commitTooltip();
						}
					}
				}

				GameSprite* sprite = it ? ctx.gfx.getGameSprite(it.clientId()) : nullptr;
				if (sprite) {
					SpritePatterns patterns = PatternCalculator::Calculate(sprite, it, item.get(), tile, position, ctx.elapsed_time);

					// Inline preload check — skip function call when sprite is simple and loaded
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

					item_drawer->BlitItem(sprite_batch, sprite_drawer, creature_drawer, draw_x, draw_y, item_params);
				} else if (item->isInvalidOTBMItem()) {
					// Missing-definition placeholders are represented by the tile-level invalid overlay.
				}
			}
			// monster/npc on tile
			if (tile->creature && options.show_creatures) {
				creature_drawer->BlitCreature(sprite_batch, sprite_drawer, draw_x, draw_y, tile->creature.get(), CreatureDrawOptions {
					.map_pos = position,
					.transient_selection_bounds = options.transient_selection_bounds,
					.light_buffer = light_buffer,
					.view = &view,
					.ctx = &ctx
				});
				if (creature_name_drawer) {
					creature_name_drawer->addLabel(position, tile->creature->getName(), tile->creature.get());
				}
			}
		}

		if (options.show_invalid_zones && !as_minimap && tile->hasInvalidZones()) {
			sprite_drawer->glBlitSquare(sprite_batch, tile_draw_x, tile_draw_y, DrawColor(255, 0, 255, 171), 0, &ctx.atlas);
		}

		if (options.show_invalid_tiles && !as_minimap && invalid_tile_marker_color != InvalidOTBMItemMarkerColor::None) {
			const DrawColor overlay = invalidTileOverlayColor(invalid_tile_marker_color, has_selected_invalid_item);
			sprite_drawer->glBlitSquare(sprite_batch, tile_draw_x, tile_draw_y, overlay, 0, &ctx.atlas);
		}

		if (view.zoom < 10.0) {
			// markers (waypoint, house exit, town temple, spawn)
			marker_drawer->draw(sprite_batch, sprite_drawer, draw_x, draw_y, tile, waypoint, current_house_id, *editor, options, &ctx);
		}
	}
}
