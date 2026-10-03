//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include <algorithm>

#include "rendering/drawers/entities/item_drawer.h"
#include "rendering/core/graphics.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/render_view.h"
#include "rendering/utilities/pattern_calculator.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "game/sprites.h"
#include "rendering/core/graphics.h"
#include "rendering/core/render_frame_context.h"
#include "rendering/indicators/technical_item_registry.h"

namespace {
	GameSprite* resolveSprite(const ItemDefinitionView& definition, const RenderFrameContext* ctx) {
		if (!definition || !ctx) {
			return nullptr;
		}
		return ctx->gfx.getGameSprite(definition.clientId());
	}

	GameSprite* resolveSprite(ServerItemId item_id, const RenderFrameContext* ctx) {
		if (!ctx) {
			return nullptr;
		}
		return resolveSprite(ctx->item_definitions.get(item_id), ctx);
	}

	constexpr DrawColor toDrawColorFrom8Bit(int color) {
		if (color <= 0 || color >= 216) {
			return DrawColor(0, 0, 0, 255);
		}
		const uint8_t red = static_cast<uint8_t>((color / 36) % 6 * 51);
		const uint8_t green = static_cast<uint8_t>((color / 6) % 6 * 51);
		const uint8_t blue = static_cast<uint8_t>(color % 6 * 51);
		return DrawColor(red, green, blue, 255);
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
	const RenderView* view = params.view;

	const ItemDefinitionView it = params.item_definition ? params.item_definition : item->getDefinition();

	if (!options.ingame) {
		bool is_selected = item->isSelected();
		if (!is_selected && !ephemeral && options.transient_selection_bounds) {
			is_selected = options.transient_selection_bounds->contains(pos.x, pos.y);
		}
		if (is_selected) {
			red >>= 1;
			blue >>= 1;
			green >>= 1;
		}
	}

	// item sprite
	GameSprite* spr = params.sprite ? params.sprite : resolveSprite(it, params.ctx);
	const GameSprite* const original_spr = spr;

	if (item->isInvalidOTBMItem() && !options.show_invalid_tiles) {
		// Invalid OTBM placeholders are controlled exclusively by SHOW_INVALID_TILES.
		return;
	}

	const AtlasManager* atlas = params.ctx ? &params.ctx->atlas : nullptr;

	// Red invalid client id / missing item
	if (!it) {
		if (options.show_invalid_tiles && !options.ingame) {
			const AtlasRegion* white_pixel = atlas ? atlas->getWhitePixel() : nullptr;
			if (white_pixel) {
				const float rf = static_cast<float>(red) / 255.0f;
				const float gf = static_cast<float>(green) / 255.0f;
				const float bf = static_cast<float>(blue) / 255.0f;
				const float af = static_cast<float>(alpha) / 255.0f;
				const auto invalid_type = (item && item->isGroundTile()) ? rme::rendering::TileIndicatorType::InvalidGround : rme::rendering::TileIndicatorType::InvalidItem;
				sprite_batch.draw(
					static_cast<float>(draw_x), static_cast<float>(draw_y),
					32.0f, 32.0f,
					*white_pixel,
					rf, gf, bf, af,
					rme::rendering::TechnicalItemRegistry::GetMarkerId(invalid_type)
				);
			}
		}
		return;
	}

	// Display invisible items via technical item shader indicators
	if (options.show_tech_items && !options.ingame) {
		const uint16_t client_id = it.clientId();
		const uint16_t server_id = item ? item->getID() : 0;
		const auto tech_type = rme::rendering::TechnicalItemRegistry::Classify(server_id, client_id);
		if (tech_type != rme::rendering::TileIndicatorType::None) {
			const AtlasRegion* white_pixel = atlas ? atlas->getWhitePixel() : nullptr;
			if (white_pixel) {
				sprite_batch.draw(static_cast<float>(draw_x), static_cast<float>(draw_y), 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, static_cast<float>(alpha) / 255.0f, rme::rendering::TechnicalItemRegistry::GetMarkerId(tech_type));
			}
			return;
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
	if (cached_patterns && spr == original_spr) {
		patterns = *cached_patterns;
	} else {
		const long elapsed_time = params.ctx ? params.ctx->elapsed_time : -1;
		patterns = PatternCalculator::Calculate(spr, it, item, tile, pos, elapsed_time);
	}

	int subtype = patterns.subtype;
	int pattern_x = patterns.x;
	int pattern_y = patterns.y;
	int pattern_z = patterns.z;
	int frame = patterns.frame;

	const bool is_simple_sprite = (spr->width == 1 && spr->height == 1 && spr->layers == 1);

	if (options.transparent_items && !ephemeral && (!it.isGroundTile() || spr->width > 1 || spr->height > 1) && !it.isSplash() && (!it.hasFlag(ItemFlag::IsBorder) || spr->width > 1 || spr->height > 1)) {
		alpha >>= 1;
	}

	const bool is_podium = it.isPodium();
	if (is_podium) {
		Podium* podium = static_cast<Podium*>(item);
		if (!podium->hasShowPlatform() && !options.ingame) {
			alpha = options.show_tech_items ? (alpha >> 1) : 0;
		}
	}

	if (is_simple_sprite) {
		const AtlasRegion* region = nullptr;
		if (spr->is_simple && subtype == -1 && pattern_x == 0 && pattern_y == 0 && pattern_z == 0 && frame == 0) {
			region = spr->getCachedDefaultRegion();
		}
		if (!region) {
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
			sprite_drawer->glBlitAtlasQuad(sprite_batch, screenx, screeny, region, DrawColor(red, green, blue, alpha), params.house_id, params.zone_flags, params.depth);
		}
	} else {
		const auto composite_metrics = spr->getPlainLayoutMetrics(subtype, pattern_x, pattern_y, pattern_z, frame);
		int x_offset = 0;
		for (int cx = 0; cx < composite_metrics.num_columns; cx++) {
			int y_offset = 0;
			for (int cy = 0; cy < composite_metrics.num_rows; cy++) {
				for (int cf = 0; cf != spr->layers; cf++) {
					const AtlasRegion* region = spr->getAtlasRegion(cx, cy, cf, subtype, pattern_x, pattern_y, pattern_z, frame);
					if (region) {
						sprite_drawer->glBlitAtlasQuad(sprite_batch, screenx - x_offset, screeny - y_offset, region, DrawColor(red, green, blue, alpha), params.house_id, params.zone_flags, params.depth);
					}
				}
				y_offset += composite_metrics.row_heights[cy];
			}
			x_offset += composite_metrics.column_widths[cx];
		}
	}

	if (is_podium) {
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
			.view = view,
			.ctx = params.ctx,
			.depth = params.depth
		});
	}

	// draw light color indicator
	if (!options.ingame && options.show_light_str) {
		const SpriteLight& light = item->getLight();
		if (light.intensity > 0) {
			const DrawColor lightColor = toDrawColorFrom8Bit(light.color);
			const int startOffset = std::max<int>(16, 32 - light.intensity);
			const int sqSize = TILE_SIZE - startOffset;

			// We need to disable texture 2d for BlitSquare. SpriteDrawer::glBlitSquare does NOT disable texture 2d automatically?
			// SpriteDrawer::glBlitSquare internally uses BatchRenderer::DrawQuad which sets blank texture if needed.
			// So we don't need manual enable/disable here anymore.

			sprite_drawer->glBlitSquare(sprite_batch, draw_x + startOffset - 2, draw_y + startOffset - 2, DrawColor(0, 0, 0, 255), sqSize + 2, atlas, params.depth);
			sprite_drawer->glBlitSquare(sprite_batch, draw_x + startOffset - 1, draw_y + startOffset - 1, lightColor, sqSize, atlas, params.depth);
		}
	}
}

void ItemDrawer::DrawRawBrush(SpriteBatch& sprite_batch, SpriteDrawer* sprite_drawer, int screenx, int screeny, ServerItemId item_id, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha, const RenderFrameContext* ctx) {
	if (!ctx) {
		return;
	}
	const auto definition = ctx->item_definitions.get(item_id);
	uint16_t cid = definition ? definition.clientId() : 0;

	const auto tech_type = rme::rendering::TechnicalItemRegistry::Classify(item_id, cid);
	if (tech_type != rme::rendering::TileIndicatorType::None) {
		const AtlasRegion* white_pixel = ctx->atlas.getWhitePixel();
		if (white_pixel) {
			sprite_batch.draw(static_cast<float>(screenx), static_cast<float>(screeny), 32.0f, 32.0f, *white_pixel, 1.0f, 1.0f, 1.0f, static_cast<float>(alpha) / 255.0f, rme::rendering::TechnicalItemRegistry::GetMarkerId(tech_type));
		}
		return;
	}

	GameSprite* spr = resolveSprite(definition, ctx);
	if (spr) {
		sprite_drawer->BlitSprite(sprite_batch, screenx, screeny, spr, DrawColor(r, g, b, alpha), ctx);
	}
}
