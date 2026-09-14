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
#include "brushes/managers/brush_manager.h"
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
#include "ui/map_tab.h"


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
	Brush* brush = g_brush_manager.GetCurrentBrush();
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
}

void MapDrawer::Release() {
	// tooltip_drawer->clear(); // Moved to ClearTooltips(), called explicitly after UI draw
}

void MapDrawer::Draw() {
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

	if (!g_graphics.ensureAtlasManager()) {
		return;
	}
	auto* atlas = g_graphics.getAtlasManager();

	const RenderFrameContext ctx {
		*atlas,
		g_graphics,
		g_item_definitions,
		options,
		view,
		g_graphics.getElapsedTime(),
		static_cast<uint32_t>(options.current_house_id)
	};

	// Begin Batches
	sprite_batch->begin(view.projectionMatrix, *atlas);
	primitive_renderer->setProjectionMatrix(view.projectionMatrix);
	if (options.isDrawLight()) {
		light_buffer.Prepare(view);
	}

	DrawBackground();

	// Save original view bounds before DrawMap modifies them per-floor
	const ViewBounds original_bounds { view.start_x, view.start_y, view.end_x, view.end_y };

	DrawMap(ctx);

	// Flush Map for Light Pass
	sprite_batch->end(*atlas);
	primitive_renderer->flush();

	if (options.isDrawLight()) {
		DrawLight();
	}

	// Resume Batch for Overlays
	sprite_batch->begin(view.projectionMatrix, *atlas);

	if (drag_shadow_drawer) {
		drag_shadow_drawer->draw(*sprite_batch, this, item_drawer.get(), sprite_drawer.get(), creature_drawer.get(), view, options, &ctx);
	}

	live_cursor_drawer->draw(*sprite_batch, view, editor, options, *atlas);

	brush_overlay_drawer->draw(*sprite_batch, *primitive_renderer, this, item_drawer.get(), sprite_drawer.get(), creature_drawer.get(), view, options, editor, *atlas);
	selection_drawer->draw(*primitive_renderer, view, canvas, options);

	if (options.show_grid) {
		DrawGrid(original_bounds, *atlas);
	}
	if (options.show_ingame_box) {
		DrawIngameBox(original_bounds, *atlas);
	}

	// Draw Lua Overlays (sprites, lines, rects, etc.)
	lua_overlay_drawer->Draw(view, options, *atlas);

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

	BaseMap* secondary_map = nullptr;
	if (!options.ingame && canvas) {
		if (auto* map_tab = dynamic_cast<MapTab*>(canvas->GetMapWindow())) {
			if (auto* session = map_tab->GetSession()) {
				secondary_map = session->secondary_map;
			}
		}
	}

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
			DrawMapLayer(*sprite_batch, floor_ctx, map_z, live_client, true);
		}

		if (secondary_map) {
			preview_drawer->draw(*sprite_batch, canvas, secondary_map, floor_view, map_z, options, editor, item_drawer.get(), sprite_drawer.get(), creature_drawer.get(), options.current_house_id, &ctx);
		}
	}
}

void MapDrawer::DrawIngameBox(const ViewBounds& bounds, const AtlasManager& atlas) {
	grid_drawer->DrawIngameBox(*sprite_batch, view, options, bounds, atlas);
}

void MapDrawer::DrawGrid(const ViewBounds& bounds, const AtlasManager& atlas) {
	grid_drawer->DrawGrid(*sprite_batch, view, options, bounds, atlas);
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

bool MapDrawer::hasOverlays() const {
	if (options.show_creatures && creature_name_drawer && !creature_name_drawer->empty()) {
		return true;
	}
	if (options.show_tooltips && tooltip_drawer && !tooltip_drawer->empty()) {
		return true;
	}
	if (options.show_hooks && hook_indicator_drawer && !hook_indicator_drawer->empty()) {
		return true;
	}
	if (options.highlight_locked_doors && door_indicator_drawer && !door_indicator_drawer->empty()) {
		return true;
	}
	if (lua_overlay_drawer && lua_overlay_drawer->hasUIElements(view)) {
		return true;
	}
	return false;
}

void MapDrawer::DrawMapLayer(SpriteBatch& batch, const RenderFrameContext& floor_ctx, int map_z, bool live_client, bool light_collection_only) {
	map_layer_drawer->Draw(batch, map_z, live_client, floor_ctx, light_buffer, light_collection_only);
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
