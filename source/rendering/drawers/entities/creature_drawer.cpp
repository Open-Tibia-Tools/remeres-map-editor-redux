//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

// glut include removed

#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "game/creature.h"
#include "ui/gui.h"
#include "game/sprites.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/animator.h"
#include "rendering/core/light_buffer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/render_frame_context.h"
#include "item_definitions/core/item_definition_store.h"
#include <spdlog/spdlog.h>

CreatureDrawer::CreatureDrawer() {
}

CreatureDrawer::~CreatureDrawer() {
}

namespace {
	void registerCreatureSpriteLight(LightBuffer& light_buffer, const RenderView& view, const GameSprite& sprite, int screen_x, int screen_y, SpriteLight light, bool preview_local_player) {
		if (preview_local_player) {
			light.intensity = std::max<uint8_t>(light.intensity, 2);
			if (light.color == 0 || light.color > 215) {
				light.color = 215;
			}
		}

		if (light.intensity == 0) {
			return;
		}

		const auto draw_offset = sprite.getDrawOffset();
		const wxSize composite_size = sprite.GetSize();
		const int left = screen_x - draw_offset.first;
		const int top = screen_y - draw_offset.second;
		const int width = std::max(1, composite_size.GetWidth());
		const int height = std::max(1, composite_size.GetHeight());
		light_buffer.AddScreenLight(left + width / 2, top + height / 2, view, light);
	}

	void registerCreatureSpriteLight(LightBuffer& light_buffer, const RenderView& view, int screen_x, int screen_y, const std::pair<int, int>& draw_offset, const GameSprite::SpriteLayoutMetrics& metrics, SpriteLight light, bool preview_local_player) {
		if (preview_local_player) {
			light.intensity = std::max<uint8_t>(light.intensity, 2);
			if (light.color == 0 || light.color > 215) {
				light.color = 215;
			}
		}

		if (light.intensity == 0) {
			return;
		}

		light_buffer.AddScreenLight(
			screen_x - draw_offset.first - metrics.left_offset + metrics.total_width / 2,
			screen_y - draw_offset.second - metrics.top_offset + metrics.total_height / 2,
			view,
			light
		);
	}

	void registerCreatureCenterLight(LightBuffer& light_buffer, const RenderView& view, int screen_x, int screen_y, const GameSprite* displacement_sprite, SpriteLight light, bool preview_local_player) {
		if (preview_local_player) {
			light.intensity = std::max<uint8_t>(light.intensity, 2);
			if (light.color == 0 || light.color > 215) {
				light.color = 215;
			}
		}

		if (light.intensity == 0) {
			return;
		}

		const int displacement_x = displacement_sprite ? displacement_sprite->getDrawOffset().first : 0;
		const int displacement_y = displacement_sprite ? displacement_sprite->getDrawOffset().second : 0;
		light_buffer.AddScreenLight(screen_x - displacement_x + TILE_SIZE / 2, screen_y - displacement_y + TILE_SIZE / 2, view, light);
	}
}

void CreatureDrawer::BlitCreature(SpriteBatch& sprite_batch, SpriteDrawer* sprite_drawer, int screenx, int screeny, const Creature* c, const CreatureDrawOptions& options) {
	CreatureDrawOptions local_opts = options;
	if (!local_opts.ingame && (c->isSelected() || (local_opts.transient_selection_bounds.has_value() && local_opts.transient_selection_bounds->contains(local_opts.map_pos.x, local_opts.map_pos.y)))) {
		local_opts.color.r /= 2;
		local_opts.color.g /= 2;
		local_opts.color.b /= 2;
	}
	BlitCreature(sprite_batch, sprite_drawer, screenx, screeny, c->getLookType(), c->getDirection(), local_opts);
}

void CreatureDrawer::BlitCreature(SpriteBatch& sprite_batch, SpriteDrawer* sprite_drawer, int screenx, int screeny, const Outfit& outfit, Direction dir, const CreatureDrawOptions& options) {
	const bool draw_visuals = !options.light_collection_only;
	GraphicManager& gfx = options.ctx ? options.ctx->gfx : g_gui.gfx;
	const ItemDefinitionStore& item_defs = options.ctx ? options.ctx->item_definitions : g_item_definitions;

	if (outfit.lookItem != 0) {
		const auto definition = item_defs.get(outfit.lookItem);
		if (definition) {
			GameSprite* spr = gfx.getGameSprite(definition.clientId());
			if (spr && options.light_buffer && options.view && spr->hasLight()) {
				registerCreatureSpriteLight(*options.light_buffer, *options.view, *spr, screenx, screeny, spr->getLight(), false);
			}
			if (draw_visuals) {
				sprite_drawer->BlitSprite(sprite_batch, screenx, screeny, spr, options.color, options.ctx);
			}
			if (spr && options.light_buffer && options.view && options.preview_local_player) {
				registerCreatureCenterLight(*options.light_buffer, *options.view, screenx, screeny, spr, spr->hasLight() ? spr->getLight() : SpriteLight {}, options.preview_local_player);
			}
		}
	} else {
		// get outfit sprite
		const Outfit* drawOutfit = &outfit;
		if (drawOutfit->lookType == 0) {
			drawOutfit = &DEFAULT_UNKNOWN_CREATURE_OUTFIT;
		}
		GameSprite* spr = gfx.getCreatureSprite(drawOutfit->lookType);
		if (!spr && drawOutfit->lookType != DEFAULT_UNKNOWN_CREATURE_OUTFIT.lookType) {
			drawOutfit = &DEFAULT_UNKNOWN_CREATURE_OUTFIT;
			spr = gfx.getCreatureSprite(DEFAULT_UNKNOWN_CREATURE_OUTFIT.lookType);
		}
		if (!spr) {
			return;
		}

		if (options.light_buffer && options.view && spr->hasLight()) {
			registerCreatureSpriteLight(*options.light_buffer, *options.view, *spr, screenx, screeny, spr->getLight(), false);
		}

		// Resolve animation frame for walk animation
		// For in-game preview: animationPhase controls walk animation
		// - When > 0: walking (use the provided animation phase)
		// - When == 0: standing idle (ALWAYS use frame 0, NOT the global animator)
		// The global animator is for idle creatures on the map, NOT for the player
		int resolvedFrame = options.animationPhase > 0 ? options.animationPhase : 0;

		// mount and addon drawing thanks to otc code
		// mount colors by Zbizu
		int pattern_z = 0;
		GameSprite* mountSpr = nullptr;
		if (drawOutfit->lookMount != 0) {
			if ((mountSpr = gfx.getCreatureSprite(drawOutfit->lookMount))) {
				// Generate mount colors and metrics once so rendering and light placement stay aligned.
				Outfit mountOutfit;
				mountOutfit.lookType = drawOutfit->lookMount;
				mountOutfit.lookMount = 0;
				mountOutfit.lookHead = drawOutfit->lookMountHead;
				mountOutfit.lookBody = drawOutfit->lookMountBody;
				mountOutfit.lookLegs = drawOutfit->lookMountLegs;
				mountOutfit.lookFeet = drawOutfit->lookMountFeet;
				const auto mount_draw_offset = mountSpr->getDrawOffset();
				const bool is_simple_mount = (mountSpr->width == 1 && mountSpr->height == 1);
				GameSprite::SpriteLayoutMetrics mount_metrics {};
				bool has_mount_metrics = false;

				if (options.light_buffer && options.view && mountSpr->hasLight()) {
					mount_metrics = mountSpr->getOutfitLayoutMetrics(static_cast<int>(dir), 0, 0, resolvedFrame);
					has_mount_metrics = true;
					registerCreatureSpriteLight(*options.light_buffer, *options.view, screenx, screeny, mount_draw_offset, mount_metrics, mountSpr->getLight(), false);
				}

				if (draw_visuals) {
					const int mount_base_x = screenx - mount_draw_offset.first;
					const int mount_base_y = screeny - mount_draw_offset.second;
					if (is_simple_mount) {
						const AtlasRegion* region = mountSpr->getAtlasRegion(0, 0, static_cast<int>(dir), 0, 0, mountOutfit, resolvedFrame);
						if (region) {
							sprite_drawer->glBlitAtlasQuad(
								sprite_batch,
								mount_base_x,
								mount_base_y,
								region,
								options.color
							);
						}
					} else {
						if (!has_mount_metrics) {
							mount_metrics = mountSpr->getOutfitLayoutMetrics(static_cast<int>(dir), 0, 0, resolvedFrame);
						}
						int mount_x_offset = 0;
						for (int cx = 0; cx < mount_metrics.num_columns; ++cx) {
							int mount_y_offset = 0;
							for (int cy = 0; cy < mount_metrics.num_rows; ++cy) {
								const AtlasRegion* region = mountSpr->getAtlasRegion(cx, cy, static_cast<int>(dir), 0, 0, mountOutfit, resolvedFrame);
								if (region) {
									sprite_drawer->glBlitAtlasQuad(
										sprite_batch,
										mount_base_x - mount_x_offset,
										mount_base_y - mount_y_offset,
										region,
										options.color
									);
								}
								mount_y_offset += mount_metrics.row_heights[cy];
							}
							mount_x_offset += mount_metrics.column_widths[cx];
						}
					}
				}

				pattern_z = std::clamp(spr->pattern_z - 1, 0, 1);
			}
		}

		// pattern_y => creature addon
		if (draw_visuals) {
			const auto sprite_draw_offset = spr->getDrawOffset();
			const int base_x = screenx - sprite_draw_offset.first;
			const int base_y = screeny - sprite_draw_offset.second;
			for (int pattern_y = 0; pattern_y < spr->pattern_y; pattern_y++) {

				// continue if we dont have this addon
				if (pattern_y > 0) {
					if ((pattern_y - 1 >= 31) || !(drawOutfit->lookAddon & (1 << (pattern_y - 1)))) {
						continue;
					}
				}

				if (spr->width == 1 && spr->height == 1) {
					const AtlasRegion* region = spr->getAtlasRegion(0, 0, static_cast<int>(dir), pattern_y, pattern_z, *drawOutfit, resolvedFrame);
					if (region) {
						sprite_drawer->glBlitAtlasQuad(
							sprite_batch,
							base_x,
							base_y,
							region,
							options.color
						);
					}
					continue;
				}

				const auto sprite_metrics = spr->getOutfitLayoutMetrics(static_cast<int>(dir), pattern_y, pattern_z, resolvedFrame);

				int sprite_x_offset = 0;
				for (int cx = 0; cx < sprite_metrics.num_columns; ++cx) {
					int sprite_y_offset = 0;
					for (int cy = 0; cy < sprite_metrics.num_rows; ++cy) {
						const AtlasRegion* region = spr->getAtlasRegion(cx, cy, static_cast<int>(dir), pattern_y, pattern_z, *drawOutfit, resolvedFrame);
						if (region) {
							sprite_drawer->glBlitAtlasQuad(
								sprite_batch,
								base_x - sprite_x_offset,
								base_y - sprite_y_offset,
								region,
								options.color
							);
						}
						sprite_y_offset += sprite_metrics.row_heights[cy];
					}
					sprite_x_offset += sprite_metrics.column_widths[cx];
				}
			}
		}

		if (options.light_buffer && options.view && options.preview_local_player) {
			registerCreatureCenterLight(*options.light_buffer, *options.view, screenx, screeny, mountSpr ? mountSpr : spr, spr->hasLight() ? spr->getLight() : SpriteLight {}, options.preview_local_player);
		}
	}
}
