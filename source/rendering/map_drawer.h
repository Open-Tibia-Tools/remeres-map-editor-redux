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

#ifndef RME_MAP_DRAWER_H_
#define RME_MAP_DRAWER_H_

#include <cstdint>

#include "app/definitions.h"
#include "app/settings.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/chunk_cache_manager.h"
#include "rendering/core/primitive_renderer.h"
#include "rendering/core/render_interaction_state.h"
#include "rendering/core/render_view.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/drawers/cursors/brush_cursor_drawer.h"
#include "rendering/drawers/cursors/drag_shadow_drawer.h"
#include "rendering/drawers/cursors/live_cursor_drawer.h"
#include "rendering/drawers/entities/creature_drawer.h"
#include "rendering/drawers/entities/creature_name_drawer.h"
#include "rendering/drawers/entities/item_drawer.h"
#include "rendering/drawers/entities/sprite_drawer.h"
#include "rendering/drawers/map_layer_drawer.h"
#include "rendering/drawers/overlays/brush_overlay_drawer.h"
#include "rendering/drawers/overlays/door_indicator_drawer.h"
#include "rendering/drawers/overlays/grid_drawer.h"
#include "rendering/drawers/overlays/hook_indicator_drawer.h"
#include "rendering/drawers/overlays/lua_overlay_drawer.h"
#include "rendering/drawers/overlays/marker_drawer.h"
#include "rendering/drawers/overlays/preview_drawer.h"
#include "rendering/drawers/overlays/selection_drawer.h"
#include "rendering/drawers/tiles/floor_drawer.h"
#include "rendering/drawers/tiles/shade_drawer.h"
#include "rendering/drawers/tiles/tile_renderer.h"
#include "rendering/ui/tooltip_drawer.h"
#include "rendering/utilities/light_drawer.h"

class Editor;
struct NVGcontext;
struct RenderFrameContext;

class MapDrawer {
	Editor& editor;
	DrawingOptions options;
	RenderView view;
	SpriteBatch sprite_batch;
	PrimitiveRenderer primitive_renderer;
	LightDrawer light_drawer;
	TooltipDrawer tooltip_drawer;
	GridDrawer grid_drawer;
	LiveCursorDrawer live_cursor_drawer;
	SelectionDrawer selection_drawer;
	BrushCursorDrawer brush_cursor_drawer;
	BrushOverlayDrawer brush_overlay_drawer;
	DragShadowDrawer drag_shadow_drawer;
	FloorDrawer floor_drawer;
	SpriteDrawer sprite_drawer;
	CreatureDrawer creature_drawer;
	CreatureNameDrawer creature_name_drawer;
	HookIndicatorDrawer hook_indicator_drawer;
	DoorIndicatorDrawer door_indicator_drawer;
	ItemDrawer item_drawer;
	MarkerDrawer marker_drawer;
	PreviewDrawer preview_drawer;
	ShadeDrawer shade_drawer;
	TileRenderer tile_renderer;
	MapLayerDrawer map_layer_drawer;
	LuaOverlayDrawer lua_overlay_drawer;
	ChunkCacheManager chunk_cache_manager;

public:
	explicit MapDrawer(Editor& editor);
	~MapDrawer();

	void SetupVars(const ViewportParameters& vp);
	void SetupGL();
	void Release();

	void Draw(const InteractionRenderState& interaction);
	void DrawBackground();
	void DrawMap(const RenderFrameContext& ctx, const InteractionRenderState& interaction);
	void DrawIngameBox(const ViewBounds& bounds, const AtlasManager& atlas);

	void DrawGrid(const ViewBounds& bounds, const AtlasManager& atlas);
	void DrawTooltips(NVGcontext* vg);
	void DrawHookIndicators(NVGcontext* vg);
	void DrawDoorIndicators(NVGcontext* vg);
	void DrawUIOverlays(NVGcontext* vg);
	void ClearFrameOverlays();
	void DrawCreatureNames(NVGcontext* vg);
	bool hasOverlays();

	void DrawLight();

	void TakeScreenshot(uint8_t* screenshot_buffer);

	DrawingOptions& getOptions() {
		return options;
	}

	Editor& getEditor() {
		return editor;
	}

	SpriteBatch* getSpriteBatch() {
		return &sprite_batch;
	}
	PrimitiveRenderer* getPrimitiveRenderer() {
		return &primitive_renderer;
	}
	TileRenderer* getTileRenderer() {
		return &tile_renderer;
	}
	DoorIndicatorDrawer* getDoorIndicatorDrawer() {
		return &door_indicator_drawer;
	}
	LuaOverlayDrawer* getLuaOverlayDrawer() {
		return &lua_overlay_drawer;
	}
	TooltipDrawer* getTooltipDrawer() {
		return &tooltip_drawer;
	}
	HookIndicatorDrawer* getHookIndicatorDrawer() {
		return &hook_indicator_drawer;
	}
	ChunkCacheManager& getChunkCacheManager() {
		return chunk_cache_manager;
	}
	const ChunkCacheManager& getChunkCacheManager() const {
		return chunk_cache_manager;
	}
	const RenderView& getView() const {
		return view;
	}

	void InvalidateOverlays() noexcept {
		overlay_cache.invalidate();
	}

private:
	struct OverlayCacheState {
		int floor = -1;
		float zoom = -1.0f;
		ViewBounds bounds{};
		bool show_tooltips = false;
		bool show_hooks = false;
		bool highlight_locked_doors = false;
		uint64_t map_generation = 0;
		bool valid = false;

		[[nodiscard]] bool isValid(
			int cur_floor,
			float cur_zoom,
			const ViewBounds& cur_bounds,
			bool opt_tooltips,
			bool opt_hooks,
			bool opt_doors,
			uint64_t cur_gen
		) const noexcept {
			if (!valid) {
				return false;
			}
			if (floor != cur_floor || zoom != cur_zoom) {
				return false;
			}
			if (show_tooltips != opt_tooltips || show_hooks != opt_hooks || highlight_locked_doors != opt_doors) {
				return false;
			}
			if (map_generation != cur_gen) {
				return false;
			}
			if (cur_bounds.start_x < bounds.start_x || cur_bounds.end_x > bounds.end_x ||
				cur_bounds.start_y < bounds.start_y || cur_bounds.end_y > bounds.end_y) {
				return false;
			}
			return true;
		}

		void invalidate() noexcept {
			valid = false;
		}
	};

	void DrawMapLayer(SpriteBatch& batch, const RenderFrameContext& floor_ctx, int map_z, bool live_client);
	void CollectOverlays();
	bool renderers_initialized = false;
	OverlayCacheState overlay_cache;
	Settings::ObserverId settings_observer_id_ = 0;
};

#endif
