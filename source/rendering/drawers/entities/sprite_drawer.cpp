#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/core/graphics.h"
#include "game/sprites.h"
#include "item_definitions/core/item_definition_store.h"
#include <spdlog/spdlog.h>
#include "rendering/core/sprite_batch.h"
#include "rendering/core/atlas_manager.h"

#include <array>

namespace {
	constexpr auto make_color_lut() {
		std::array<float, 256> lut {};
		for (int i = 0; i < 256; ++i) {
			lut[i] = static_cast<float>(i) * (1.0f / 255.0f);
		}
		return lut;
	}
	constexpr auto COLOR_LUT = make_color_lut();
}

SpriteDrawer::SpriteDrawer() {
}

SpriteDrawer::~SpriteDrawer() {
}

void SpriteDrawer::glBlitAtlasQuad(SpriteBatch& sprite_batch, int sx, int sy, const AtlasRegion* region, DrawColor color) {
	if (region) {
		sprite_batch.draw(
			static_cast<float>(sx), static_cast<float>(sy),
			static_cast<float>(region->pixel_width), static_cast<float>(region->pixel_height),
			*region,
			COLOR_LUT[color.r], COLOR_LUT[color.g], COLOR_LUT[color.b], COLOR_LUT[color.a]
		);
	}
}

#include "rendering/core/render_frame_context.h"

void SpriteDrawer::glBlitSquare(SpriteBatch& sprite_batch, int sx, int sy, DrawColor color, int size, const AtlasManager* atlas) {
	if (size == 0) {
		size = TILE_SIZE;
	}

	const AtlasManager* atlas_mgr = atlas;
	if (!atlas_mgr && g_graphics.hasAtlasManager()) {
		atlas_mgr = g_graphics.getAtlasManager();
	}
	if (atlas_mgr) {
		sprite_batch.drawRect(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(size), static_cast<float>(size), glm::vec4(COLOR_LUT[color.r], COLOR_LUT[color.g], COLOR_LUT[color.b], COLOR_LUT[color.a]), *atlas_mgr);
	}
}

void SpriteDrawer::glDrawBox(SpriteBatch& sprite_batch, int sx, int sy, int width, int height, DrawColor color, const AtlasManager* atlas) {
	const AtlasManager* atlas_mgr = atlas;
	if (!atlas_mgr && g_graphics.hasAtlasManager()) {
		atlas_mgr = g_graphics.getAtlasManager();
	}
	if (atlas_mgr) {
		sprite_batch.drawRectLines(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(width), static_cast<float>(height), glm::vec4(COLOR_LUT[color.r], COLOR_LUT[color.g], COLOR_LUT[color.b], COLOR_LUT[color.a]), *atlas_mgr);
	}
}

void SpriteDrawer::BlitSprite(SpriteBatch& sprite_batch, int screenx, int screeny, ServerItemId server_item_id, DrawColor color, const RenderFrameContext* ctx) {
	GameSprite* spr = nullptr;
	if (ctx) {
		const auto definition = ctx->item_definitions.get(server_item_id);
		spr = definition ? ctx->gfx.getGameSprite(definition.clientId()) : nullptr;
	} else {
		const auto definition = g_item_definitions.get(server_item_id);
		spr = definition ? g_graphics.getGameSprite(definition.clientId()) : nullptr;
	}
	if (spr == nullptr) {
		return;
	}
	// Call the pointer overload
	BlitSprite(sprite_batch, screenx, screeny, spr, color, ctx);
}

void SpriteDrawer::BlitSprite(SpriteBatch& sprite_batch, int screenx, int screeny, GameSprite* spr, DrawColor color, const RenderFrameContext* ctx) {
	if (spr == nullptr) {
		return;
	}
	const auto draw_offset = spr->getDrawOffset();
	screenx -= draw_offset.first;
	screeny -= draw_offset.second;

	const long elapsed_time = ctx ? ctx->elapsed_time : -1;
	const int tme = spr->animator ? spr->animator->getFrame(elapsed_time) : 0;

	// Fast path for single 1x1x1 sprites (waypoints, markers, simple items) — skip metric cache
	if (spr->width == 1 && spr->height == 1 && spr->layers == 1) {
		const AtlasRegion* region = spr->getAtlasRegion(0, 0, 0, -1, 0, 0, 0, tme);
		if (region) {
			glBlitAtlasQuad(sprite_batch, screenx, screeny, region, color);
		}
		return;
	}

	const auto layout_metrics = spr->getPlainLayoutMetrics(-1, 0, 0, 0, tme);
	int x_offset = 0;
	for (int cx = 0; cx < layout_metrics.num_columns; ++cx) {
		int y_offset = 0;
		for (int cy = 0; cy < layout_metrics.num_rows; ++cy) {
			for (int cf = 0; cf != spr->layers; ++cf) {
				const AtlasRegion* region = spr->getAtlasRegion(cx, cy, cf, -1, 0, 0, 0, tme);
				if (region) {
					glBlitAtlasQuad(sprite_batch, screenx - x_offset, screeny - y_offset, region, color);
				}
				// No fallback - if region is null, sprite failed to load
			}
			y_offset += layout_metrics.row_heights[cy];
		}
		x_offset += layout_metrics.column_widths[cx];
	}
}
