//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_DRAWERS_MINIMAP_DRAWER_H_
#define RME_RENDERING_DRAWERS_MINIMAP_DRAWER_H_

#include "rendering/ui/minimap_viewport.h"
#include "rendering/drawers/minimap_renderer.h"
#include "rendering/core/floor_visibility_mode.h"

#include <memory>

class Editor;
class MapCanvas;
class PrimitiveRenderer;

struct MinimapDrawOptions {
	bool drawCameraBox = true;
	bool drawBoundsBorder = true;
	FloorVisibilityMode floor_visibility_mode = FloorVisibilityMode::ClientVisible;
};

class MinimapDrawer {
public:
	MinimapDrawer();
	~MinimapDrawer();

	void Draw(const wxSize& size, Editor& editor, MapCanvas& canvas, const MinimapViewportState& viewport_state, MinimapDrawOptions options = {});
	void ReleaseGL();

	void ScreenToMap(int screen_x, int screen_y, int& map_x, int& map_y);

private:
	struct LastViewportMetrics {
		double start_x = 0.0;
		double start_y = 0.0;
		double map_width = 1.0;
		double map_height = 1.0;
		int screen_width = 1;
		int screen_height = 1;
		int map_limit_x = 0;
		int map_limit_y = 0;
		bool valid = false;
	};

	struct VisibleWorldRect {
		double start_x = 0.0;
		double start_y = 0.0;
		double width = 1.0;
		double height = 1.0;
	};

	VisibleWorldRect BuildVisibleWorldRect(const wxSize& size, Editor& editor, const MinimapViewportState& viewport_state);
	void DrawFloorShade(const glm::mat4& projection, const wxSize& size);
	void DrawMainCameraBox(const glm::mat4& projection, const wxSize& size, MapCanvas& canvas, const VisibleWorldRect& visible_rect);
	void DrawMapBoundsBorder(const glm::mat4& projection, const wxSize& size, const Editor& editor, const VisibleWorldRect& visible_rect);

	std::unique_ptr<MinimapRenderer> renderer;
	std::unique_ptr<PrimitiveRenderer> primitive_renderer;
	LastViewportMetrics last_viewport_;
	bool initialized_ = false;
};

#endif
