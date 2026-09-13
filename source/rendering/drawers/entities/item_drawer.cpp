//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include <algorithm>
#undef min
#undef max

#include "rendering/drawers/entities/item_drawer.h"
#include "rendering/drawers/overlays/hook_indicator_drawer.h"
#include "rendering/drawers/overlays/door_indicator_drawer.h"
#include "rendering/core/graphics.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/light_buffer.h"
#include "rendering/core/render_view.h"
#include "rendering/utilities/pattern_calculator.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "game/sprites.h"
#include "ui/gui.h"
#include "rendering/core/render_frame_context.h"

namespace {
	GameSprite* resolveSprite(const ItemDefinitionView& definition, const RenderFrameContext* ctx = nullptr) {
		if (!definition) {
			return nullptr;
		}
		if (ctx) {
			return ctx->gfx.getGameSprite(definition.clientId());
		}
		return g_gui.gfx.getGameSprite(definition.clientId());
	}

	GameSprite* resolveSprite(ServerItemId item_id, const RenderFrameContext* ctx = nullptr) {
		if (ctx) {
			return resolveSprite(ctx->item_definitions.get(item_id), ctx);
		}
		return resolveSprite(g_item_definitions.get(item_id), ctx);
	}

	void registerSpriteLight(LightBuffer& light_buffer, const RenderView& view, int screen_x, int screen_y, const GameSprite::SpriteLayoutMetrics& metrics, const SpriteLight& light) {
		light_buffer.AddScreenLight(
			screen_x - metrics.left_offset + metrics.total_width / 2,
			screen_y - metrics.top_offset + metrics.total_height / 2,
			view,
			light
		);
	}
}

BlitItemParams::BlitItemParams(const Tile* t, Item* i, const DrawingOptions& o) : tile(t), item(i), options(&o) {
	if (t) {
		pos = t->getPosition();
	}
}

BlitItemParams::BlitItemParams(const Position& p, Item* i, const DrawingOptions& o) : pos(p), item(i), options(&o) {
}

ItemDrawer::ItemDrawer() {
}

ItemDrawer::~ItemDrawer() {
}

void ItemDrawer::BlitItem(SpriteBatch& sprite_batch, SpriteDrawer* sprite_drawer, CreatureDrawer* creature_drawer, int& draw_x, int& draw_y, const BlitItemParams& params) {
	const Position& pos = params.pos;
	Item* item = params.item;
	const Tile* tile = params.tile;
	const DrawingOptions& options = *params.options;
	bool ephemeral = params.ephemeral;
	int red = params.red;
	int green = params.green;
	int blue = params.blue;
	int alpha = params.alpha;
	const SpritePatterns* cached_patterns = params.patterns;
	LightBuffer* light_buffer = params.light_buffer;
	const RenderView* view = params.view;
	const bool draw_visuals = !params.light_collection_only;

	const ItemDefinitionView it = params.item_definition ? params.item_definition : item->getDefinition();

	// Locked door indicator
	if (!options.ingame && options.highlight_locked_doors && it.isDoor()) {
		bool locked = item->isLocked();

		// Door orientation: horizontal wall -> West border (south=true), vertical wall -> North border (east=true)
		if (static_cast<BorderType>(it.attribute(ItemAttributeKey::BorderAlignment)) == WALL_HORIZONTAL) {
			DrawDoorIndicator(locked, pos, true, false);
		} else if (static_cast<BorderType>(it.attribute(ItemAttributeKey::BorderAlignment)) == WALL_VERTICAL) {
			DrawDoorIndicator(locked, pos, false, true);
		} else {
			// Center case for non-aligned doors
			DrawDoorIndicator(locked, pos, false, false);
		}
	}

	bool is_transient_selected = !ephemeral && options.transient_selection_bounds && options.transient_selection_bounds->contains(pos.x, pos.y);
	if (!options.ingame && (item->isSelected() || is_transient_selected)) {
		red /= 2;
		blue /= 2;
		green /= 2;
	}

	// item sprite
	GameSprite* spr = resolveSprite(it, params.ctx);

	if (item->isInvalidOTBMItem() && !options.show_invalid_tiles) {
		// Invalid OTBM placeholders are controlled exclusively by SHOW_INVALID_TILES.
		return;
	}

	const AtlasManager* atlas = params.ctx ? &params.ctx->atlas : nullptr;

	// Display invisible and invalid items
	// Ugly hacks. :)
	if (!options.ingame && options.show_tech_items) {
		// Red invalid client id
		if (!it) {
			sprite_drawer->glBlitSquare(sprite_batch, draw_x, draw_y, DrawColor(red, 0, 0, alpha), 0, atlas);
			return;
		}

		switch (it.clientId()) {
			// Yellow invisible stairs tile (459)
			case 469:
				sprite_drawer->glBlitSquare(sprite_batch, draw_x, draw_y, DrawColor(red, green, 0, (alpha * 171) >> 8), 0, atlas);
				return;

			// Red invisible walkable tile (460)
			case 470:
			case 17970:
			case 20028:
			case 34168:
				sprite_drawer->glBlitSquare(sprite_batch, draw_x, draw_y, DrawColor(red, 0, 0, (alpha * 171) >> 8), 0, atlas);
				return;

			// Cyan invisible wall (1548)
			case 2187:
				sprite_drawer->glBlitSquare(sprite_batch, draw_x, draw_y, DrawColor(0, green, blue, 80), 0, atlas);
				return;

			default:
				break;
		}

		// primal light
		if (it.clientId() >= 39092 && it.clientId() <= 39100 || it.clientId() == 39236 || it.clientId() == 39367 || it.clientId() == 39368) {
			spr = resolveSprite(SPRITE_LIGHTSOURCE, params.ctx);
			red = 0;
			alpha = 180;
		}
	}

	// metaItem, sprite not found or not hidden
	if (it.isMetaItem() || spr == nullptr || !ephemeral && it.hasFlag(ItemFlag::Pickupable) && !options.show_items) {
		return;
	}

	const auto [draw_offset_x, draw_offset_y] = spr->getDrawOffset();
	int screenx = draw_x - draw_offset_x;
	int screeny = draw_y - draw_offset_y;

	// Set the newd drawing height accordingly
	draw_x -= spr->draw_height;
	draw_y -= spr->draw_height;

	SpritePatterns patterns;
	if (cached_patterns && spr == resolveSprite(it, params.ctx)) {
		patterns = *cached_patterns;
	} else {
		patterns = PatternCalculator::Calculate(spr, it, item, tile, pos);
	}

	int subtype = patterns.subtype;
	int pattern_x = patterns.x;
	int pattern_y = patterns.y;
	int pattern_z = patterns.z;
	int frame = patterns.frame;
		const auto composite_metrics = spr->getPlainLayoutMetrics(subtype, pattern_x, pattern_y, pattern_z, frame);

	if (light_buffer && view && item->hasLight()) {
		registerSpriteLight(*light_buffer, *view, screenx, screeny, composite_metrics, item->getLight());
	}

	if (!ephemeral && options.transparent_items && (!it.isGroundTile() || spr->width > 1 || spr->height > 1) && !it.isSplash() && (!it.hasFlag(ItemFlag::IsBorder) || spr->width > 1 || spr->height > 1)) {
		alpha /= 2;
	}

	if (it.isPodium()) {
		Podium* podium = static_cast<Podium*>(item);
		if (!podium->hasShowPlatform() && !options.ingame) {
			if (options.show_tech_items) {
				alpha /= 2;
			} else {
				alpha = 0;
			}
		}
	}

	if (draw_visuals) {
		// Atlas-only rendering
		// g_gui.gfx.ensureAtlasManager();
		// BatchRenderer::SetAtlasManager(g_gui.gfx.getAtlasManager());

		if (spr->width == 1 && spr->height == 1 && spr->layers == 1) {
			const AtlasRegion* region;
			if (subtype == -1 && pattern_x == 0 && pattern_y == 0 && pattern_z == 0 && frame == 0) {
				region = spr->getAtlasRegion(0, 0, 0, -1, 0, 0, 0, 0);
			} else {
				region = spr->getAtlasRegion(0, 0, 0, subtype, pattern_x, pattern_y, pattern_z, frame);
			}

			if (region) {
#ifdef DEBUG
				// DEBUG: Check for mismatch on Item 369 using PRECISE sub-sprite ID
				if (item->getID() == 369) {
					// Use 0,0 as pattern coordinates for 1x1 items
					uint32_t precise_expected_id = spr->getSpriteId(frame, 0, 0);
					if (region->debug_sprite_id != 0 && precise_expected_id != 0 && region->debug_sprite_id != precise_expected_id) {
						spdlog::error("SPRITE MISMATCH DETECTED: Item 369 (Expected Sprite ID {}, Actual Region Owner {})", precise_expected_id, region->debug_sprite_id);
					}
				}
#endif
				sprite_drawer->glBlitAtlasQuad(sprite_batch, screenx, screeny, region, DrawColor(red, green, blue, alpha));
			}
		} else {
			int x_offset = 0;
			for (int cx = 0; cx != spr->width; cx++) {
				int y_offset = 0;
				for (int cy = 0; cy != spr->height; cy++) {
					for (int cf = 0; cf != spr->layers; cf++) {
						const AtlasRegion* region = spr->getAtlasRegion(cx, cy, cf, subtype, pattern_x, pattern_y, pattern_z, frame);
						if (region) {
							sprite_drawer->glBlitAtlasQuad(sprite_batch, screenx - x_offset, screeny - y_offset, region, DrawColor(red, green, blue, alpha));
						}
					}
					y_offset += composite_metrics.row_heights[cy];
				}
				x_offset += composite_metrics.column_widths[cx];
			}
		}
	}

	if (it.isPodium()) {
		Podium* podium = static_cast<Podium*>(item);
		Outfit outfit = podium->getOutfit();
		if (!podium->hasShowOutfit()) {
			if (podium->hasShowMount()) {
				outfit.lookType = outfit.lookMount;
				outfit.lookHead = outfit.lookMountHead;
				outfit.lookBody = outfit.lookMountBody;
				outfit.lookLegs = outfit.lookMountLegs;
				outfit.lookFeet = outfit.lookMountFeet;
				outfit.lookAddon = 0;
				outfit.lookMount = 0;
			} else {
				outfit.lookType = 0;
			}
		}
		if (!podium->hasShowMount()) {
			outfit.lookMount = 0;
		}

		creature_drawer->BlitCreature(sprite_batch, sprite_drawer, draw_x, draw_y, outfit, static_cast<Direction>(podium->getDirection()), CreatureDrawOptions {
			.color = DrawColor(red, green, blue, alpha),
			.light_buffer = light_buffer,
			.view = view,
			.light_collection_only = params.light_collection_only
		});
	}

	// draw wall hook
	if (draw_visuals && !options.ingame && options.show_hooks && (it.hasFlag(ItemFlag::HookSouth) || it.hasFlag(ItemFlag::HookEast))) {
		DrawHookIndicator(it, pos);
	}

	// draw light color indicator
	if (draw_visuals && !options.ingame && options.show_light_str) {
		const SpriteLight& light = item->getLight();
		if (light.intensity > 0) {
			wxColor lightColor = colorFromEightBit(light.color);
			uint8_t byteR = lightColor.Red();
			uint8_t byteG = lightColor.Green();
			uint8_t byteB = lightColor.Blue();
			uint8_t byteA = 255;

			int startOffset = std::max<int>(16, 32 - light.intensity);
			int sqSize = TILE_SIZE - startOffset;

			// We need to disable texture 2d for BlitSquare. SpriteDrawer::glBlitSquare does NOT disable texture 2d automatically?
			// SpriteDrawer::glBlitSquare internally uses BatchRenderer::DrawQuad which sets blank texture if needed.
			// So we don't need manual enable/disable here anymore.

			sprite_drawer->glBlitSquare(sprite_batch, draw_x + startOffset - 2, draw_y + startOffset - 2, DrawColor(0, 0, 0, byteA), sqSize + 2);
			sprite_drawer->glBlitSquare(sprite_batch, draw_x + startOffset - 1, draw_y + startOffset - 1, DrawColor(byteR, byteG, byteB, byteA), sqSize);
		}
	}
}

void ItemDrawer::DrawRawBrush(SpriteBatch& sprite_batch, SpriteDrawer* sprite_drawer, int screenx, int screeny, ServerItemId item_id, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) {
	const auto definition = g_item_definitions.get(item_id);
	GameSprite* spr = resolveSprite(definition);
	uint16_t cid = definition ? definition.clientId() : 0;

	switch (cid) {
		// Yellow invisible stairs tile
		case 469:
			b = 0;
			alpha = (alpha * 171) >> 8;
			spr = resolveSprite(SPRITE_ZONE);
			break;

		// Red invisible walkable tile
		case 470:
			g = 0;
			b = 0;
			alpha = (alpha * 171) >> 8;
			spr = resolveSprite(SPRITE_ZONE);
			break;

		// Cyan invisible wall
		case 2187:
			r = 0;
			alpha = alpha / 3;
			spr = resolveSprite(SPRITE_ZONE);
			break;

		default:
			break;
	}

	// primal light
	if (cid >= 39092 && cid <= 39100 || cid == 39236 || cid == 39367 || cid == 39368) {
		spr = resolveSprite(SPRITE_LIGHTSOURCE);
		r = 0;
		alpha = (alpha * 171) >> 8;
	}

	if (spr) {
		sprite_drawer->BlitSprite(sprite_batch, screenx, screeny, spr, DrawColor(r, g, b, alpha));
	}
}

void ItemDrawer::DrawHookIndicator(const ItemDefinitionView& definition, const Position& pos) {
	if (hook_indicator_drawer) {
		hook_indicator_drawer->addHook(pos, definition.hasFlag(ItemFlag::HookSouth), definition.hasFlag(ItemFlag::HookEast));
	}
}

void ItemDrawer::DrawDoorIndicator(bool locked, const Position& pos, bool south, bool east) {
	if (door_indicator_drawer) {
		door_indicator_drawer->addDoor(pos, locked, south, east);
	}
}
