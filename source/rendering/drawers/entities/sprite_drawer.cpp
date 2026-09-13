#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/core/graphics.h"
#include "game/sprites.h"
#include "item_definitions/core/item_definition_store.h"

#include "ui/gui.h"
#include <spdlog/spdlog.h>
#include "rendering/core/sprite_batch.h"
#include "rendering/core/atlas_manager.h"

SpriteDrawer::SpriteDrawer() {
}

SpriteDrawer::~SpriteDrawer() {
}

void SpriteDrawer::glBlitAtlasQuad(SpriteBatch& sprite_batch, int sx, int sy, const AtlasRegion* region, DrawColor color) {
	if (region) {
		float normalizedR = color.r / 255.0f;
		float normalizedG = color.g / 255.0f;
		float normalizedB = color.b / 255.0f;
		float normalizedA = color.a / 255.0f;

		sprite_batch.draw(
			static_cast<float>(sx), static_cast<float>(sy),
			static_cast<float>(region->pixel_width), static_cast<float>(region->pixel_height),
			*region,
			normalizedR, normalizedG, normalizedB, normalizedA
		);
	}
}

#include "rendering/core/render_frame_context.h"

void SpriteDrawer::glBlitSquare(SpriteBatch& sprite_batch, int sx, int sy, DrawColor color, int size, const AtlasManager* atlas) {
	if (size == 0) {
		size = TILE_SIZE;
	}

	float normalizedR = color.r / 255.0f;
	float normalizedG = color.g / 255.0f;
	float normalizedB = color.b / 255.0f;
	float normalizedA = color.a / 255.0f;

	const AtlasManager* atlas_mgr = atlas;
	if (!atlas_mgr && g_gui.gfx.hasAtlasManager()) {
		atlas_mgr = g_gui.gfx.getAtlasManager();
	}
	if (atlas_mgr) {
		sprite_batch.drawRect(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(size), static_cast<float>(size), glm::vec4(normalizedR, normalizedG, normalizedB, normalizedA), *atlas_mgr);
	}
}

void SpriteDrawer::glDrawBox(SpriteBatch& sprite_batch, int sx, int sy, int width, int height, DrawColor color, const AtlasManager* atlas) {
	float normalizedR = color.r / 255.0f;
	float normalizedG = color.g / 255.0f;
	float normalizedB = color.b / 255.0f;
	float normalizedA = color.a / 255.0f;

	const AtlasManager* atlas_mgr = atlas;
	if (!atlas_mgr && g_gui.gfx.hasAtlasManager()) {
		atlas_mgr = g_gui.gfx.getAtlasManager();
	}
	if (atlas_mgr) {
		sprite_batch.drawRectLines(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(width), static_cast<float>(height), glm::vec4(normalizedR, normalizedG, normalizedB, normalizedA), *atlas_mgr);
	}
}

void SpriteDrawer::glSetColor(wxColor color) {
	// Not needed with BatchRenderer automatic color handling in DrawQuad,
	// but if used for stateful drawing elsewhere, we might need a state setter.
	// For now, ignoring as glBlitTexture/Square takes explicit color.
}

void SpriteDrawer::BlitSprite(SpriteBatch& sprite_batch, int screenx, int screeny, ServerItemId server_item_id, DrawColor color, const RenderFrameContext* ctx) {
	GameSprite* spr = nullptr;
	if (ctx) {
		const auto definition = ctx->item_definitions.get(server_item_id);
		spr = definition ? ctx->gfx.getGameSprite(definition.clientId()) : nullptr;
	} else {
		const auto definition = g_item_definitions.get(server_item_id);
		spr = definition ? g_gui.gfx.getGameSprite(definition.clientId()) : nullptr;
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
	for (int cx = 0; cx != spr->width; ++cx) {
		int y_offset = 0;
		for (int cy = 0; cy != spr->height; ++cy) {
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
