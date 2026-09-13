#include "ingame_preview/ingame_preview_renderer.h"
#include "ingame_preview/floor_visibility_calculator.h"
#include "rendering/drawers/tiles/tile_renderer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/primitive_renderer.h"
#include "rendering/core/light_buffer.h"
#include "rendering/utilities/light_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/drawers/entities/creature_name_drawer.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "game/outfit.h"
#include "map/basemap.h"
#include "map/map_region.h"
#include "map/tile.h"
#include "game/creature.h"
#include "ui/gui.h"
#include "app/settings.h"
#include "rendering/core/text_renderer.h"
#include "rendering/core/render_frame_context.h"
#include "item_definitions/core/item_definition_store.h"
#include <glad/glad.h>
#include <nanovg.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstdlib>
#include <limits>

namespace IngamePreview {

	IngamePreviewRenderer::IngamePreviewRenderer(TileRenderer* tile_renderer) :
		tile_renderer(tile_renderer) {
		floor_calculator = std::make_unique<FloorVisibilityCalculator>();
		sprite_batch = std::make_unique<SpriteBatch>();
		primitive_renderer = std::make_unique<PrimitiveRenderer>();
		light_buffer = std::make_unique<LightBuffer>();
		light_drawer = std::make_shared<LightDrawer>();
		creature_drawer = std::make_unique<CreatureDrawer>();
		creature_name_drawer = std::make_unique<CreatureNameDrawer>();
		sprite_drawer = std::make_unique<SpriteDrawer>();

		sprite_batch->initialize();
		primitive_renderer->initialize();
	}

	IngamePreviewRenderer::~IngamePreviewRenderer() = default;

	void IngamePreviewRenderer::Render(NVGcontext* vg, const BaseMap& map, int viewport_x, int viewport_y, int viewport_width, int viewport_height, const Position& camera_pos, float zoom, bool lighting_enabled, uint8_t client_brightness_percent, const Outfit& preview_outfit, Direction preview_direction, int animation_phase, int offset_x, int offset_y) {
		// CRITICAL: Update animation time for all sprite animations to work
		g_gui.gfx.updateTime();

		int first_visible = floor_calculator->CalcFirstVisibleFloor(map, camera_pos.x, camera_pos.y, camera_pos.z);
		int last_visible = floor_calculator->CalcLastVisibleFloor(camera_pos.z);

		// Setup RenderView and DrawingOptions
		RenderView view;
		view.zoom = zoom;
		view.tile_size = TILE_SIZE;
		view.floor = camera_pos.z;
		view.start_z = last_visible;
		view.end_z = camera_pos.z;
		view.superend_z = first_visible;
		view.screensize_x = viewport_width;
		view.screensize_y = viewport_height;
		view.camera_pos = camera_pos;
		view.viewport_x = viewport_x;
		view.viewport_y = viewport_y;

		// Initialize cached logical dimensions (required for visibility culling)
		view.logical_width = viewport_width * zoom;
		view.logical_height = viewport_height * zoom;

		// Proper coordinate alignment
		// We want camera_pos to be at the center of the viewport
		int offset = (camera_pos.z <= GROUND_LAYER) ? (GROUND_LAYER - camera_pos.z) * TILE_SIZE : 0;
		view.view_scroll_x = (camera_pos.x * TILE_SIZE) + (TILE_SIZE / 2) - offset + offset_x - static_cast<int>(viewport_width * zoom / 2.0f);
		view.view_scroll_y = (camera_pos.y * TILE_SIZE) + (TILE_SIZE / 2) - offset + offset_y - static_cast<int>(viewport_height * zoom / 2.0f);

		// Matching RME's projection (width * zoom x height * zoom)
		view.projectionMatrix = glm::ortho(0.0f, static_cast<float>(viewport_width) * zoom, static_cast<float>(viewport_height) * zoom, 0.0f, -1.0f, 1.0f);
		view.viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.375f, 0.375f, 0.0f));

		DrawingOptions options;
		options.SetIngame();
		options.show_lights = lighting_enabled;
		options.show_shade = g_settings.getBoolean(Config::SHOW_SHADE);
		options.server_light = SpriteLight {
			.intensity = light_intensity,
			.color = server_light_color
		};
		options.minimum_ambient_light = static_cast<float>(std::min<int>(client_brightness_percent, 100)) / 100.0f;
		options.draw_floor_shadow = options.show_shade;

		// Initialize GL state
		glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		primitive_renderer->setProjectionMatrix(view.projectionMatrix);
		light_buffer->Clear();
		if (lighting_enabled) {
			light_buffer->Prepare(view);
		}
		const bool draw_lights = options.isDrawLight() && view.zoom <= 10.0f;
		if (creature_name_drawer) {
			creature_name_drawer->clear(); // Clear old labels
		}

		if (!g_gui.gfx.ensureAtlasManager()) {
			return;
		}
		auto* atlas = g_gui.gfx.getAtlasManager();

		const RenderFrameContext ctx {
			.atlas = *atlas,
			.gfx = g_gui.gfx,
			.item_definitions = g_item_definitions,
			.options = options,
			.view = view,
			.elapsed_time = g_gui.gfx.getElapsedTime(),
			.current_house_id = 0
		};

		sprite_batch->begin(view.projectionMatrix, *atlas);

		for (int z = last_visible; z >= first_visible; --z) {
			if (options.isDrawLight() && options.draw_floor_shadow && camera_pos.z >= GROUND_LAYER + 1 && z == camera_pos.z) {
				sprite_batch->drawRect(0.0f, 0.0f, view.screensize_x * view.zoom, view.screensize_y * view.zoom, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f), *atlas);
			}

			// Pre-calculate view offsets for this floor
			int floor_offset = (z <= GROUND_LAYER)
				? (GROUND_LAYER - z) * TILE_SIZE
				: TILE_SIZE * (view.floor - z);
			int camera_offset = (camera_pos.z <= GROUND_LAYER)
				? (GROUND_LAYER - camera_pos.z) * TILE_SIZE
				: 0;

			// Dynamic viewport culling — adjusted per floor
			constexpr int margin = TILE_SIZE * 16;
			int max_floor_offset = std::max(
				std::abs(floor_offset - camera_offset),
				TILE_SIZE * MAP_MAX_LAYER
			);

			view.start_x = static_cast<int>(std::floor((view.view_scroll_x - margin - max_floor_offset) / static_cast<float>(TILE_SIZE)));
			view.start_y = static_cast<int>(std::floor((view.view_scroll_y - margin - max_floor_offset) / static_cast<float>(TILE_SIZE)));
			view.end_x = static_cast<int>(std::ceil((view.view_scroll_x + viewport_width * zoom + margin + max_floor_offset) / static_cast<float>(TILE_SIZE)));
			view.end_y = static_cast<int>(std::ceil((view.view_scroll_y + viewport_height * zoom + margin + max_floor_offset) / static_cast<float>(TILE_SIZE)));

			const int nd_start_x = view.start_x & ~3;
			const int nd_start_y = view.start_y & ~3;
			const int nd_end_x = (view.end_x & ~3) + 4;
			const int nd_end_y = (view.end_y & ~3) + 4;
			const int base_draw_x = -view.view_scroll_x - floor_offset;
			const int base_draw_y = -view.view_scroll_y - floor_offset;
			const int safe_margin_tiles = PAINTERS_ALGORITHM_SAFETY_MARGIN_PIXELS / TILE_SIZE;

			auto visitVisibleNodes = [&](auto&& visitor) {
				map.visitLeaves(nd_start_x - safe_margin_tiles, nd_start_y - safe_margin_tiles, nd_end_x + safe_margin_tiles, nd_end_y + safe_margin_tiles, [&](const MapNode* node, int nd_map_x, int nd_map_y) {
					const int node_draw_x = nd_map_x * TILE_SIZE + base_draw_x;
					const int node_draw_y = nd_map_y * TILE_SIZE + base_draw_y;

					if (!view.IsRectVisible(node_draw_x, node_draw_y, 4 * TILE_SIZE, 4 * TILE_SIZE, PAINTERS_ALGORITHM_SAFETY_MARGIN_PIXELS)) {
						return;
					}

					const bool fully_inside = view.IsRectFullyInside(node_draw_x, node_draw_y, 4 * TILE_SIZE, 4 * TILE_SIZE);
					const Floor* floor = node->getFloor(z);
					if (!floor) {
						return;
					}

					const TileLocation* location = floor->locs.data();
					int draw_x_base = node_draw_x;
					for (int map_x = 0; map_x < 4; ++map_x, draw_x_base += TILE_SIZE) {
						int draw_y = node_draw_y;
						for (int map_y = 0; map_y < 4; ++map_y, ++location, draw_y += TILE_SIZE) {
							if (!fully_inside && !view.IsPixelVisible(draw_x_base, draw_y, PAINTERS_ALGORITHM_SAFETY_MARGIN_PIXELS)) {
								continue;
							}
							visitor(location, draw_x_base, draw_y);
						}
					}
				});
			};

			if (draw_lights) {
				ASSERT(light_buffer->lights.size() <= std::numeric_limits<uint32_t>::max());
				const uint32_t floor_light_start = static_cast<uint32_t>(light_buffer->lights.size());
				visitVisibleNodes([&](const TileLocation* location, int, int) {
					tile_renderer->RegisterGroundLightOcclusion(location, view, *light_buffer, floor_light_start);
				});
			}

			visitVisibleNodes([&](const TileLocation* location, int draw_x, int draw_y) {
				tile_renderer->DrawTile(*sprite_batch, location, ctx, draw_x, draw_y, draw_lights ? light_buffer.get() : nullptr);

				if (creature_name_drawer && z == camera_pos.z) {
					if (const Tile* tile = location->get(); tile && tile->creature) {
						creature_name_drawer->addLabel(location->getPosition(), tile->creature->getName(), tile->creature.get());
					}
				}
			});

			if (z == camera_pos.z) {
				const int center_x = static_cast<int>((viewport_width * zoom) / 2.0f);
				const int center_y = static_cast<int>((viewport_height * zoom) / 2.0f);
				const int elevation_offset = GetTileElevationOffset(map.getTile(camera_pos));
				const int draw_x = center_x - 16;
				const int draw_y = center_y - 16 - elevation_offset;

				creature_drawer->BlitCreature(*sprite_batch, sprite_drawer.get(), draw_x, draw_y, preview_outfit, preview_direction, CreatureDrawOptions {
					.ingame = true,
					.animationPhase = animation_phase,
					.light_buffer = draw_lights ? light_buffer.get() : nullptr,
					.view = &view,
					.preview_local_player = true,
					.ctx = &ctx
				});
			}
		}

		sprite_batch->end(*atlas);

		if (options.isDrawLight() && light_drawer) {
			light_drawer->draw(view, *light_buffer, options);
		}

		// Draw Names
		if (creature_name_drawer && vg) {
			TextRenderer::BeginFrame(vg, viewport_width, viewport_height, 1.0f); // Ingame preview doesn't use scale factor yet

			// 1. Draw creatures on map
			creature_name_drawer->draw(vg, view);

			// 2. Draw our own label at precise center
			if (vg) {
				nvgSave(vg);
				float fontSize = 11.0f;
				nvgFontSize(vg, fontSize);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

				// Total elevation offset was calculated above.
				// For the label to stay synced, we should probably fetch it again or store it.
				// Since we are at the center of the screen, we can just use screen-relative coords.
				float screenCenterX = static_cast<float>(viewport_width) / 2.0f;
				float screenCenterY = static_cast<float>(viewport_height) / 2.0f;

				// Fetch elevation again to be precise
				int elevation_offset = GetTileElevationOffset(map.getTile(camera_pos));

				float labelY = screenCenterY - (16.0f + static_cast<float>(elevation_offset)) / zoom - 2.0f;

				std::string name = preview_name;
				float textBounds[4];
				nvgTextBounds(vg, 0, 0, name.c_str(), nullptr, textBounds);
				float textWidth = textBounds[2] - textBounds[0];
				float textHeight = textBounds[3] - textBounds[1];

				float paddingX = 4.0f;
				float paddingY = 2.0f;

				nvgBeginPath(vg);
				nvgRoundedRect(vg, screenCenterX - textWidth / 2.0f - paddingX, labelY - textHeight - paddingY * 2.0f, textWidth + paddingX * 2.0f, textHeight + paddingY * 2.0f, 3.0f);
				nvgFillColor(vg, nvgRGBA(0, 0, 0, 160));
				nvgFill(vg);

				nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
				nvgText(vg, screenCenterX, labelY - paddingY, name.c_str(), nullptr);
				nvgRestore(vg);
			}
			TextRenderer::EndFrame(vg);
		}
	}

	int IngamePreviewRenderer::GetTileElevationOffset(const Tile* tile) const {
		int elevation_offset = 0;
		if (tile) {
			for (const auto& item : tile->items) {
				elevation_offset += item->getHeight();
			}
			if (tile->ground) {
				elevation_offset += tile->ground->getHeight();
			}
			if (elevation_offset > 24) {
				elevation_offset = 24;
			}
		}
		return elevation_offset;
	}

} // namespace IngamePreview
