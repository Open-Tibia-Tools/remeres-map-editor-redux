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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>

#include "rendering/map_drawer.h"
#include "rendering/core/hardware_profile.h"
#include "editor/editor.h"
#include "brushes/managers/brush_manager.h"
#include "brushes/brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "game/sprites.h"
#include "rendering/core/graphics.h"
#include "rendering/core/render_frame_context.h"
#include "rendering/drawers/overlays/map_overlay_collector.h"
#include "rendering/io/screen_capture.h"
#include "rendering/core/gl_resources.h"

MapDrawer::MapDrawer(Editor& editor) :
	editor(editor),
	tile_renderer(&item_drawer, &sprite_drawer, &creature_drawer, &creature_name_drawer, &floor_drawer, &marker_drawer, &editor),
	map_layer_drawer(&tile_renderer, &grid_drawer, editor.map),
	lua_overlay_drawer(editor) {

	options.Update();
	settings_observer_id_ = g_settings.addObserver([this](uint32_t key) {
		options.MarkSettingDirty(key);
	});
}

MapDrawer::~MapDrawer() {
	if (settings_observer_id_ != 0) {
		g_settings.removeObserver(settings_observer_id_);
		settings_observer_id_ = 0;
	}
	if (renderers_initialized) {
		chunk_cache_manager.release();
		renderers_initialized = false;
	}
}

void MapDrawer::SetupVars(const ViewportParameters& vp) {
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
	const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()
	).count();
	const double speed = 0.005;
	options.highlight_pulse = static_cast<float>((std::sin(static_cast<double>(now_ms) * speed) + 1.0) * 0.5);

	view.Setup(vp, options, &editor.map);
}

void MapDrawer::SetupGL() {
	view.SetupGL();

	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	// Ensure renderers are initialized
	if (!renderers_initialized) {
		sprite_batch.initialize();
		primitive_renderer.initialize();
		chunk_cache_manager.initialize();
		renderers_initialized = true;
	}
}

void MapDrawer::Release() {
	// End of frame hook: persistent GPU state and chunk cache must remain alive across frames
}

void MapDrawer::Draw(const InteractionRenderState& interaction) {
	static uint64_t s_frame_heartbeat = 0;
	if (++s_frame_heartbeat % 120 == 0) {
		auto* atlas_ptr = g_graphics.getAtlasManager();
		const int atlas_layers = atlas_ptr ? atlas_ptr->getLayerCount() : 0;
		const int atlas_allocated = atlas_ptr ? atlas_ptr->getAllocatedLayers() : 0;
		const int atlas_sprites = atlas_ptr ? atlas_ptr->getTotalSpriteCount() : 0;
		spdlog::info("[RenderHeartbeat] Frame #{}: Floor={} | Zoom={:.1f}% | Chunks Cached={}/{} (~{:.1f} MB) | Atlas: {}/{} layers ({} sprites) | Profile: {} ({}) | Light: {}",
			s_frame_heartbeat,
			view.floor,
			view.zoom * 100.0f,
			chunk_cache_manager.getCachedChunkCount(),
			chunk_cache_manager.getMaxCachedChunks(),
			(chunk_cache_manager.getCachedChunkCount() * 3.5) / 1024.0,
			atlas_layers,
			atlas_allocated,
			atlas_sprites,
			HardwareProfileManager::getModeName(HardwareProfileManager::get().getProfileMode()),
			HardwareProfileManager::getTierName(HardwareProfileManager::get().getActiveTier()),
			options.isDrawLight() ? "ON" : "OFF"
		);
	}

	const auto& active_budget = HardwareProfileManager::get().getActiveBudget();
	if (active_budget.max_cached_chunks != chunk_cache_manager.getMaxCachedChunks()) {
		chunk_cache_manager.applyBudget(active_budget);
	}

	if (options.isChunkBakeDirty()) {
		spdlog::info("[MapDrawer] options.isChunkBakeDirty() triggered chunk_cache_manager.invalidateAll()");
		chunk_cache_manager.invalidateAll();
		InvalidateOverlays();
		options.clearChunkBakeDirty();
	}
	if (options.isLightingDirty()) {
		spdlog::info("[MapDrawer] options.isLightingDirty() triggered light_drawer.invalidateAll()");
		light_drawer.invalidateAll();
		options.clearLightingDirty();
	}
	if (options.isDirty() || options.isVisualDirty()) {
		InvalidateOverlays();
	}
	options.clearVisualDirty();
	options.clearDirty();
	chunk_cache_manager.advanceFrame(view.floor);
	chunk_cache_manager.updateDirtyState(editor.map.getChangeTracker());
	light_drawer.updateDirtyState(editor.map.getChangeTracker());

	creature_name_drawer.clear();
	options.transient_selection_bounds = std::nullopt;

	if (options.boundbox_selection) {
		options.transient_selection_bounds = interaction.selection_bounds;
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
	sprite_batch.begin(view.projectionMatrix, *atlas);
	primitive_renderer.setProjectionMatrix(view.projectionMatrix);

	DrawBackground();

	// Save original view bounds before DrawMap modifies them per-floor
	const ViewBounds original_bounds { view.start_x, view.start_y, view.end_x, view.end_y };

	DrawMap(ctx, interaction);

	// Flush Map for Light Pass
	sprite_batch.end(*atlas);
	primitive_renderer.flush();

	if (options.isDrawLight()) {
		DrawLight();
	}

	// Resume Batch for Overlays
	sprite_batch.begin(view.projectionMatrix, *atlas);

	drag_shadow_drawer.draw(sprite_batch, editor, interaction.drag_start_position, &item_drawer, &sprite_drawer, &creature_drawer, view, options, &ctx);

	live_cursor_drawer.draw(sprite_batch, view, editor, options, *atlas);
	brush_overlay_drawer.draw(sprite_batch, primitive_renderer, &brush_cursor_drawer, interaction.brush_drag_state, &item_drawer, &sprite_drawer, &creature_drawer, view, options, editor, *atlas, ctx);
	selection_drawer.draw(primitive_renderer, view, options);

	if (options.show_grid) {
		DrawGrid(original_bounds, *atlas);
	}
	if (options.show_ingame_box) {
		DrawIngameBox(original_bounds, *atlas);
	}

	// Draw Lua Overlays (sprites, lines, rects, etc.)
	lua_overlay_drawer.Draw(sprite_batch, primitive_renderer, view, options, *atlas);

	// End Batches and Flush
	sprite_batch.end(*atlas);
	primitive_renderer.flush();

	// Pre-collect indicators and tooltips so hasOverlays() accurately reflects whether any NanoVG geometry exists
	CollectOverlays();
}

void MapDrawer::DrawBackground() {
	view.Clear();
}

void MapDrawer::DrawMap(const RenderFrameContext& ctx, const InteractionRenderState& interaction) {
	bool live_client = editor.live_manager.IsClient();

	BaseMap* secondary_map = (!options.ingame) ? interaction.secondary_map : nullptr;

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

		if (map_z == view.end_z && view.start_z != view.end_z) {
			shade_drawer.draw(sprite_batch, floor_view, options, ctx.atlas);
		}

		if (view.draw_all_visited_floors || map_z >= view.end_z) {
			DrawMapLayer(sprite_batch, floor_ctx, map_z, live_client);
		}

		if (secondary_map) {
			Brush* current_brush = interaction.current_brush ? interaction.current_brush : g_brush_manager.GetCurrentBrush();
			preview_drawer.draw(sprite_batch, interaction.is_pasting, secondary_map, floor_view, map_z, options, editor, &item_drawer, &sprite_drawer, &creature_drawer, options.current_house_id, current_brush, &ctx);
		}
	}
}

void MapDrawer::DrawIngameBox(const ViewBounds& bounds, const AtlasManager& atlas) {
	grid_drawer.DrawIngameBox(sprite_batch, view, options, bounds, atlas);
}

void MapDrawer::DrawGrid(const ViewBounds& bounds, const AtlasManager& atlas) {
	grid_drawer.DrawGrid(sprite_batch, view, options, bounds, atlas);
}

void MapDrawer::DrawTooltips(NVGcontext* vg) {
	if (options.show_tooltips && !tooltip_drawer.empty() && view.zoom <= 10.0f) {
		tooltip_drawer.draw(vg, view);
	}
}

void MapDrawer::CollectOverlays() {
	const bool can_read_labels = view.zoom <= 10.0f;
	if (!can_read_labels) {
		if (overlay_cache.valid) {
			tooltip_drawer.clear();
			hook_indicator_drawer.clear();
			door_indicator_drawer.clear();
			overlay_cache.invalidate();
		}
		return;
	}

	const bool need_tooltips = options.show_tooltips;
	const bool need_hooks = !options.ingame && options.show_hooks;
	const bool need_doors = !options.ingame && options.highlight_locked_doors;

	if (!need_tooltips && !need_doors && !need_hooks) {
		if (overlay_cache.valid) {
			tooltip_drawer.clear();
			hook_indicator_drawer.clear();
			door_indicator_drawer.clear();
			overlay_cache.invalidate();
		}
		return;
	}

	const int map_z = view.floor;
	const ViewBounds current_bounds = view.getBoundsForFloor(map_z);
	const uint64_t current_generation = editor.map.getChangeTracker().getGeneration();

	if (overlay_cache.isValid(map_z, view.zoom, current_bounds, need_tooltips, need_hooks, need_doors, current_generation)) {
		return;
	}

	tooltip_drawer.clear();
	hook_indicator_drawer.clear();
	door_indicator_drawer.clear();

	constexpr int EXTRA_MARGIN_TILES = 8;
	const ViewBounds collect_bounds = view.getBoundsForFloor(map_z, EXTRA_MARGIN_TILES);

	MapOverlayCollector::Collect(
		editor.map,
		view,
		collect_bounds,
		options,
		&editor,
		need_tooltips ? &tooltip_drawer : nullptr,
		need_doors ? &door_indicator_drawer : nullptr,
		need_hooks ? &hook_indicator_drawer : nullptr
	);

	overlay_cache.floor = map_z;
	overlay_cache.zoom = view.zoom;
	overlay_cache.bounds = collect_bounds;
	overlay_cache.show_tooltips = need_tooltips;
	overlay_cache.show_hooks = need_hooks;
	overlay_cache.highlight_locked_doors = need_doors;
	overlay_cache.map_generation = current_generation;
	overlay_cache.valid = true;
}

void MapDrawer::DrawHookIndicators(NVGcontext* vg) {
	if (options.show_hooks && !hook_indicator_drawer.empty() && view.zoom <= 10.0f) {
		hook_indicator_drawer.draw(vg, view);
	}
}

void MapDrawer::DrawDoorIndicators(NVGcontext* vg) {
	if (options.highlight_locked_doors && !door_indicator_drawer.empty() && view.zoom <= 10.0f) {
		door_indicator_drawer.draw(vg, view);
	}
}

void MapDrawer::DrawUIOverlays(NVGcontext* vg) {
	lua_overlay_drawer.DrawUI(vg, view, options);
}

void MapDrawer::DrawCreatureNames(NVGcontext* vg) {
	if (options.show_creatures && !creature_name_drawer.empty() && view.zoom <= 10.0f) {
		creature_name_drawer.draw(vg, view);
	}
}

bool MapDrawer::hasOverlays() {
	const bool can_read_labels = view.zoom <= 10.0f;
	if (options.show_creatures && !creature_name_drawer.empty() && can_read_labels) {
		return true;
	}
	if (options.show_tooltips && !tooltip_drawer.empty() && can_read_labels) {
		return true;
	}
	if (options.show_hooks && !hook_indicator_drawer.empty() && can_read_labels) {
		return true;
	}
	if (options.highlight_locked_doors && !door_indicator_drawer.empty() && can_read_labels) {
		return true;
	}
	if (lua_overlay_drawer.hasUIElements(view)) {
		return true;
	}
	return false;
}

void MapDrawer::DrawMapLayer(SpriteBatch& batch, const RenderFrameContext& floor_ctx, int map_z, bool live_client) {
	LiveClient* live_client_service = live_client ? editor.live_manager.GetClient() : nullptr;
	map_layer_drawer.Draw(batch, map_z, live_client_service, floor_ctx, &chunk_cache_manager);
}

void MapDrawer::DrawLight() {
	light_drawer.render(view, editor.map, g_graphics, options);
}

void MapDrawer::TakeScreenshot(uint8_t* screenshot_buffer) {
	ScreenCapture::Capture(view.screensize_x, view.screensize_y, screenshot_buffer);
}

void MapDrawer::ClearFrameOverlays() {
	// Overlay data (tooltips, hooks, doors) are cached across frames until invalidated
	// (e.g. camera moves outside bounds, zoom change, map edit, or options toggle).
	// Transient per-frame overlays (like creature names) are cleared during their render pass.
}
