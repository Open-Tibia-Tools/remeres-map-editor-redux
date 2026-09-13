//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include <algorithm>

#include "editor/editor.h"
#include "ui/gui.h"
#include "game/sprites.h"

#include "rendering/map_drawer.h"
#include "brushes/brush.h"
#include "rendering/drawers/map_layer_drawer.h"
#include "rendering/ui/map_display.h"
#include "editor/copybuffer.h"
#include "live/live_socket.h"
#include "rendering/core/graphics.h"
#include "rendering/core/render_frame_context.h"
#include "item_definitions/core/item_definition_store.h"

#include "brushes/doodad/doodad_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "rendering/utilities/light_drawer.h"
#include "rendering/ui/tooltip_drawer.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/render_view.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/primitive_renderer.h"

#include "rendering/drawers/overlays/grid_drawer.h"
#include "rendering/drawers/cursors/live_cursor_drawer.h"
#include "rendering/drawers/overlays/selection_drawer.h"
#include "rendering/drawers/cursors/brush_cursor_drawer.h"
#include "rendering/drawers/overlays/brush_overlay_drawer.h"
#include "rendering/drawers/cursors/drag_shadow_drawer.h"
#include "rendering/drawers/tiles/floor_drawer.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/drawers/entities/item_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/drawers/overlays/marker_drawer.h"
#include "rendering/drawers/overlays/hook_indicator_drawer.h"
#include "rendering/drawers/overlays/door_indicator_drawer.h"
#include "rendering/drawers/overlays/lua_overlay_drawer.h"
#include "rendering/drawers/overlays/preview_drawer.h"
#include "rendering/drawers/tiles/shade_drawer.h"
#include "rendering/drawers/tiles/tile_color_calculator.h"
#include "rendering/io/screen_capture.h"
#include "rendering/drawers/tiles/tile_renderer.h"
#include "rendering/drawers/entities/creature_name_drawer.h"
#include "rendering/core/gl_resources.h"
#include "rendering/core/shader_program.h"
#include "rendering/postprocess/post_process_manager.h"

// Shader Sources
const char* screen_vert = R"(
#version 450 core
layout(location = 0) in vec2 aPos; // -1..1
layout(location = 1) in vec2 aTexCoord; // 0..1

out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

MapDrawer::MapDrawer(MapCanvas* canvas) :
	canvas(canvas), editor(canvas->editor) {

	light_drawer = std::make_shared<LightDrawer>();
	tooltip_drawer = std::make_unique<TooltipDrawer>();

	sprite_drawer = std::make_unique<SpriteDrawer>();
	creature_drawer = std::make_unique<CreatureDrawer>();
	floor_drawer = std::make_unique<FloorDrawer>();
	item_drawer = std::make_unique<ItemDrawer>();
	marker_drawer = std::make_unique<MarkerDrawer>();

	creature_name_drawer = std::make_unique<CreatureNameDrawer>();

	tile_renderer = std::make_unique<TileRenderer>(item_drawer.get(), sprite_drawer.get(), creature_drawer.get(), creature_name_drawer.get(), floor_drawer.get(), marker_drawer.get(), tooltip_drawer.get(), &editor);

	grid_drawer = std::make_unique<GridDrawer>();
	map_layer_drawer = std::make_unique<MapLayerDrawer>(tile_renderer.get(), grid_drawer.get(), &editor); // Initialized map_layer_drawer
	live_cursor_drawer = std::make_unique<LiveCursorDrawer>();
	selection_drawer = std::make_unique<SelectionDrawer>();
	brush_cursor_drawer = std::make_unique<BrushCursorDrawer>();
	brush_overlay_drawer = std::make_unique<BrushOverlayDrawer>();
	drag_shadow_drawer = std::make_unique<DragShadowDrawer>();
	preview_drawer = std::make_unique<PreviewDrawer>();

	shade_drawer = std::make_unique<ShadeDrawer>();

	sprite_batch = std::make_unique<SpriteBatch>();
	primitive_renderer = std::make_unique<PrimitiveRenderer>();
	hook_indicator_drawer = std::make_unique<HookIndicatorDrawer>();
	door_indicator_drawer = std::make_unique<DoorIndicatorDrawer>();
	lua_overlay_drawer = std::make_unique<LuaOverlayDrawer>(this);

	item_drawer->SetHookIndicatorDrawer(hook_indicator_drawer.get());
	item_drawer->SetDoorIndicatorDrawer(door_indicator_drawer.get());

	options.Update();
	settings_observer_id_ = g_settings.addObserver([this](uint32_t) {
		options.MarkDirty();
	});
}

MapDrawer::~MapDrawer() {
	if (settings_observer_id_ != 0) {
		g_settings.removeObserver(settings_observer_id_);
		settings_observer_id_ = 0;
	}
	Release();
}

void MapDrawer::SetupVars() {
	options.current_house_id = 0;
	Brush* brush = g_gui.GetCurrentBrush();
	if (brush) {
		if (brush->is<HouseBrush>()) {
			options.current_house_id = brush->as<HouseBrush>()->getHouseID();
		} else if (brush->is<HouseExitBrush>()) {
			options.current_house_id = brush->as<HouseExitBrush>()->getHouseID();
		}
	}

	// Calculate pulse for house highlighting
	// Period is 1 second (1000ms)
	// Range is [0.0, 1.0]
	// Using a sine wave for smooth transition
	// (sin(t) + 1) / 2
	double now = wxGetLocalTimeMillis().ToDouble();
	const double speed = 0.005;
	options.highlight_pulse = (float)((sin(now * speed) + 1.0) / 2.0);

	view.Setup(canvas, options);
}

void MapDrawer::SetupGL() {
	view.SetupGL();

	// Ensure renderers are initialized
	if (!renderers_initialized) {

		sprite_batch->initialize();
		primitive_renderer->initialize();
		renderers_initialized = true;
	}

	InitPostProcess();
}

void MapDrawer::InitPostProcess() {
	if (pp_vao) {
		return;
	}

	// Load Shaders
	// Load Shaders
	PostProcessManager::Instance().Initialize(screen_vert);

	// Setup Screen Quad
	pp_vao = std::make_unique<GLVertexArray>();
	pp_vbo = std::make_unique<GLBuffer>();
	pp_ebo = std::make_unique<GLBuffer>();

	float quadVertices[] = {
		// positions   // texCoords
		-1.0f, 1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f,
		1.0f, -1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 1.0f, 1.0f
	};

	unsigned int quadIndices[] = {
		0, 1, 2,
		0, 2, 3
	};

	glNamedBufferStorage(pp_vbo->GetID(), sizeof(quadVertices), quadVertices, 0);
	glNamedBufferStorage(pp_ebo->GetID(), sizeof(quadIndices), quadIndices, 0);

	glVertexArrayVertexBuffer(pp_vao->GetID(), 0, pp_vbo->GetID(), 0, 4 * sizeof(float));
	glVertexArrayElementBuffer(pp_vao->GetID(), pp_ebo->GetID());

	glEnableVertexArrayAttrib(pp_vao->GetID(), 0);
	glVertexArrayAttribFormat(pp_vao->GetID(), 0, 2, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(pp_vao->GetID(), 0, 0);

	glEnableVertexArrayAttrib(pp_vao->GetID(), 1);
	glVertexArrayAttribFormat(pp_vao->GetID(), 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
	glVertexArrayAttribBinding(pp_vao->GetID(), 1, 0);
}

void MapDrawer::DrawPostProcess(const RenderView& view, const DrawingOptions& options) {
	if (!scale_fbo || !pp_vao) {
		return;
	}

	ShaderProgram* shader = PostProcessManager::Instance().GetEffect(options.screen_shader_name);
	if (!shader) {
		// Manager already tries fallback to NONE, but if even that is missing:
		return;
	}

	// Only clear and bind main screen once we know we can draw the result
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(view.viewport_x, view.viewport_y, view.screensize_x, view.screensize_y);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear main screen

	shader->Use();
	shader->SetInt("u_Texture", 0);
	// Set TextureSize uniform if shader needs it
	shader->SetVec2("u_TextureSize", glm::vec2(fbo_width, fbo_height));

	glBindTextureUnit(0, scale_texture->GetID());
	glBindVertexArray(pp_vao->GetID());
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	shader->Unuse();
}

void MapDrawer::UpdateFBO(const RenderView& view, const DrawingOptions& options) {
	// Determine FBO size.
	// If upscaling (Zoom < 1.0, e.g. 0.25), we want 1 pixel = 1 map unit.
	// width_pixels = screen_width * zoom.
	float scale_factor = view.zoom < 1.0f ? view.zoom : 1.0f;
	// If zoom > 1.0 (minified), we render at screen res (or native map size?)
	// Rendering at screen res with Zoom > 1.0 means primitives are small.

	int target_w = std::max(1, static_cast<int>(view.screensize_x * scale_factor));
	int target_h = std::max(1, static_cast<int>(view.screensize_y * scale_factor));

	bool fbo_resized = false;
	if (fbo_width != target_w || fbo_height != target_h || !scale_fbo) {
		fbo_width = target_w;
		fbo_height = target_h;
		scale_fbo = std::make_unique<GLFramebuffer>();
		scale_texture = std::make_unique<GLTextureResource>(GL_TEXTURE_2D);

		glTextureStorage2D(scale_texture->GetID(), 1, GL_RGBA8, fbo_width, fbo_height);
		glTextureParameteri(scale_texture->GetID(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(scale_texture->GetID(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glNamedFramebufferTexture(scale_fbo->GetID(), GL_COLOR_ATTACHMENT0, scale_texture->GetID(), 0);
		GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
		glNamedFramebufferDrawBuffers(scale_fbo->GetID(), 1, drawBuffers);

		// Sanity check for division by zero risk in shaders
		if (fbo_width < 1 || fbo_height < 1) {
			// This should be impossible due to std::max, but good for invariant documentation
			spdlog::error("MapDrawer: FBO dimension is zero ({}, {})!", fbo_width, fbo_height);
		}
		fbo_resized = true;
	}

	// Update filtering parameters when scaling is enabled and either the FBO was resized or the AA mode changed (scale_texture && (fbo_resized || options.anti_aliasing != m_lastAaMode))
	if (scale_texture && (fbo_resized || options.anti_aliasing != m_lastAaMode)) {
		GLenum filter = options.anti_aliasing ? GL_LINEAR : GL_NEAREST;
		glTextureParameteri(scale_texture->GetID(), GL_TEXTURE_MIN_FILTER, filter);
		glTextureParameteri(scale_texture->GetID(), GL_TEXTURE_MAG_FILTER, filter);
		m_lastAaMode = options.anti_aliasing;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, scale_fbo->GetID());
	glViewport(0, 0, fbo_width, fbo_height);
}

void MapDrawer::Release() {
	// tooltip_drawer->clear(); // Moved to ClearTooltips(), called explicitly after UI draw
}

void MapDrawer::Draw() {
	//g_gui.gfx.updateTime();

	light_buffer.Clear();
	creature_name_drawer->clear();
	options.transient_selection_bounds = std::nullopt;

	if (options.boundbox_selection) {
		options.transient_selection_bounds = MapBounds {
			.x1 = std::min(canvas->last_click_map_x, canvas->last_cursor_map_x),
			.y1 = std::min(canvas->last_click_map_y, canvas->last_cursor_map_y),
			.x2 = std::max(canvas->last_click_map_x, canvas->last_cursor_map_x),
			.y2 = std::max(canvas->last_click_map_y, canvas->last_cursor_map_y)
		};
	}

	if (!g_gui.gfx.ensureAtlasManager()) {
		return;
	}
	auto* atlas = g_gui.gfx.getAtlasManager();

	const RenderFrameContext ctx {
		*atlas,
		g_gui.gfx,
		g_item_definitions,
		options,
		view,
		g_gui.gfx.getElapsedTime(),
		static_cast<uint32_t>(options.current_house_id)
	};

	// Begin Batches
	sprite_batch->begin(view.projectionMatrix, *atlas);
	primitive_renderer->setProjectionMatrix(view.projectionMatrix);
	if (options.isDrawLight()) {
		light_buffer.Prepare(view);
	}

	// Check Framebuffer Logic
	// Check Framebuffer Logic
	bool use_fbo = (options.screen_shader_name != ShaderNames::NONE) || options.anti_aliasing;
	// Use FBO if zooming IN (zoom < 1.0) for upscaling, OR if AA is requested.
	// If zooming OUT (zoom > 1.0), FBO resolution logic needs care.
	// Current logic: Always use FBO if shading enabled.

	if (use_fbo) {
		UpdateFBO(view, options);
	}

	DrawBackground(); // Clear screen (or FBO)

	// Save original view bounds before DrawMap modifies them per-floor
	const ViewBounds original_bounds { view.start_x, view.start_y, view.end_x, view.end_y };

	DrawMap(ctx);

	// Flush Map for Light Pass
	sprite_batch->end(*atlas);
	primitive_renderer->flush();

	// If using FBO, resolve to screen BEFORE compositing lightmap
	// (lightmap must composite onto the resolved scene, not an empty framebuffer)
	if (use_fbo) {
		DrawPostProcess(view, options);
		// Reset to default FBO for overlays
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(view.viewport_x, view.viewport_y, view.screensize_x, view.screensize_y);
	}

	if (options.isDrawLight()) {
		DrawLight();
	}

	// Resume Batch for Overlays
	sprite_batch->begin(view.projectionMatrix, *atlas);

	if (drag_shadow_drawer) {
		drag_shadow_drawer->draw(*sprite_batch, this, item_drawer.get(), sprite_drawer.get(), creature_drawer.get(), view, options, &ctx);
	}

	live_cursor_drawer->draw(*sprite_batch, view, editor, options);

	brush_overlay_drawer->draw(*sprite_batch, *primitive_renderer, this, item_drawer.get(), sprite_drawer.get(), creature_drawer.get(), view, options, editor);
	selection_drawer->draw(*primitive_renderer, view, canvas, options);

	if (options.show_grid) {
		DrawGrid(original_bounds);
	}
	if (options.show_ingame_box) {
		DrawIngameBox(original_bounds);
	}

	// Draw Lua Overlays (sprites, lines, rects, etc.)
	lua_overlay_drawer->Draw(view, options);

	// Draw creature names (Overlay) moved to DrawCreatureNames()

	// End Batches and Flush
	sprite_batch->end(*atlas);
	primitive_renderer->flush();

	// Tooltips are now drawn in MapCanvas::OnPaint (UI Pass)
}

void MapDrawer::DrawBackground() {
	view.Clear();
}

void MapDrawer::DrawMap(const RenderFrameContext& ctx) {
	bool live_client = editor.live_manager.IsClient();

	// Enable texture mode

	for (int map_z = view.start_z; map_z >= view.superend_z; map_z--) {
		RenderView floor_view = view;
		const ViewBounds floor_bounds = view.getBoundsForFloor(map_z);
		floor_view.start_x = floor_bounds.start_x;
		floor_view.start_y = floor_bounds.start_y;
		floor_view.end_x = floor_bounds.end_x;
		floor_view.end_y = floor_bounds.end_y;

		RenderFrameContext floor_ctx {
			ctx.atlas,
			ctx.gfx,
			ctx.item_definitions,
			ctx.options,
			floor_view,
			ctx.elapsed_time,
			ctx.current_house_id
		};

		if (options.isDrawLight() && options.draw_floor_shadow && view.end_z >= GROUND_LAYER + 1 && map_z == view.end_z) {
			sprite_batch->drawRect(0.0f, 0.0f, floor_view.screensize_x * floor_view.zoom, floor_view.screensize_y * floor_view.zoom, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f), ctx.atlas);
		}

		if (!options.isDrawLight() && map_z == view.end_z && view.start_z != view.end_z) {
			shade_drawer->draw(*sprite_batch, floor_view, options, ctx.atlas);
		}

		if (view.draw_all_visited_floors || map_z >= view.end_z) {
			DrawMapLayer(*sprite_batch, floor_ctx, map_z, live_client);
		} else if (options.isDrawLight()) {
			DrawMapLayer(hidden_floor_light_batch, floor_ctx, map_z, live_client, true);
		}

		preview_drawer->draw(*sprite_batch, canvas, floor_view, map_z, options, editor, item_drawer.get(), sprite_drawer.get(), creature_drawer.get(), options.current_house_id, &ctx);
	}
}

void MapDrawer::DrawMap() {
	if (!g_gui.gfx.ensureAtlasManager()) {
		return;
	}
	const RenderFrameContext ctx {
		*g_gui.gfx.getAtlasManager(),
		g_gui.gfx,
		g_item_definitions,
		options,
		view,
		g_gui.gfx.getElapsedTime(),
		static_cast<uint32_t>(options.current_house_id)
	};
	DrawMap(ctx);
}

void MapDrawer::DrawIngameBox(const ViewBounds& bounds) {
	grid_drawer->DrawIngameBox(*sprite_batch, view, options, bounds);
}

void MapDrawer::DrawGrid(const ViewBounds& bounds) {
	grid_drawer->DrawGrid(*sprite_batch, view, options, bounds);
}

void MapDrawer::DrawTooltips(NVGcontext* vg) {
	tooltip_drawer->draw(vg, view);
}

void MapDrawer::DrawHookIndicators(NVGcontext* vg) {
	hook_indicator_drawer->draw(vg, view);
}

void MapDrawer::DrawDoorIndicators(NVGcontext* vg) {
	if (options.highlight_locked_doors) {
		door_indicator_drawer->draw(vg, view);
	}
}

void MapDrawer::DrawCreatureNames(NVGcontext* vg) {
	creature_name_drawer->draw(vg, view);
}

void MapDrawer::DrawMapLayer(SpriteBatch& batch, const RenderFrameContext& floor_ctx, int map_z, bool live_client, bool light_collection_only) {
	map_layer_drawer->Draw(batch, map_z, live_client, floor_ctx, light_buffer, light_collection_only);
}

void MapDrawer::DrawMapLayer(SpriteBatch& batch, const RenderView& draw_view, int map_z, bool live_client, bool light_collection_only) {
	if (!g_gui.gfx.ensureAtlasManager()) {
		return;
	}
	const RenderFrameContext ctx {
		*g_gui.gfx.getAtlasManager(),
		g_gui.gfx,
		g_item_definitions,
		options,
		draw_view,
		g_gui.gfx.getElapsedTime(),
		static_cast<uint32_t>(options.current_house_id)
	};
	DrawMapLayer(batch, ctx, map_z, live_client, light_collection_only);
}

void MapDrawer::DrawLight() {
	light_drawer->draw(view, light_buffer, options);
}

void MapDrawer::TakeScreenshot(uint8_t* screenshot_buffer) {
	ScreenCapture::Capture(view.screensize_x, view.screensize_y, screenshot_buffer);
}

void MapDrawer::ClearFrameOverlays() {
	tooltip_drawer->clear();
	hook_indicator_drawer->clear();
	door_indicator_drawer->clear();
}
