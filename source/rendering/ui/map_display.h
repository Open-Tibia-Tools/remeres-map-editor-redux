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

#ifndef RME_DISPLAY_WINDOW_H_
#define RME_DISPLAY_WINDOW_H_

#include "editor/action.h"
#include "map/tile.h"
#include "game/creature.h"
#include "rendering/utilities/fps_counter.h"

#include "ui/map_popup_menu.h"
#include "game/animation_timer.h"
#include "rendering/core/graphics.h"
#include <chrono>
#include <memory>
#include <optional>

struct NVGcontext;
struct DrawingOptions;

class Item;
class Creature;
class MapWindow;
class MapDrawer;
class SelectionController;
class DrawingController;
class ScreenshotController;
class MapMenuHandler;

class MapCanvas : public wxGLCanvas {
private:
	std::unique_ptr<wxGLContext> m_glContext;
	std::unique_ptr<NVGcontext, NVGDeleter> m_nvg;

public:
	MapCanvas(wxWindow* parent, Editor& editor, int* attriblist);
	~MapCanvas() override;
	virtual void Reset();

	// All events
	void OnPaint(wxPaintEvent& event);
	void OnEraseBackground(wxEraseEvent& event) { }
	void OnIdle(wxIdleEvent& event);

	void OnMouseMove(wxMouseEvent& event);
	void OnMouseLeftRelease(wxMouseEvent& event);
	void OnMouseLeftClick(wxMouseEvent& event);
	void OnMouseLeftDoubleClick(wxMouseEvent& event);
	void OnMouseCenterClick(wxMouseEvent& event);
	void OnMouseCenterRelease(wxMouseEvent& event);
	void OnMouseRightClick(wxMouseEvent& event);
	void OnMouseRightRelease(wxMouseEvent& event);

	void OnKeyDown(wxKeyEvent& event);
	void OnKeyUp(wxKeyEvent& event);
	void OnWheel(wxMouseEvent& event);
	void OnGainMouse(wxMouseEvent& event);
	void OnLoseMouse(wxMouseEvent& event);

	// Mouse events handlers (called by the above)
	void OnMouseActionRelease(wxMouseEvent& event);
	void OnMouseActionClick(wxMouseEvent& event);
	void OnMouseCameraClick(wxMouseEvent& event);
	void OnMouseCameraRelease(wxMouseEvent& event);
	void OnMousePropertiesClick(wxMouseEvent& event);
	void OnMousePropertiesRelease(wxMouseEvent& event);

	void RequestLocalRefresh(bool immediate = false);
	void RequestSharedMapRefresh(bool immediate = false);
	void RequestAnimationRepaint();
	void SetHoverPreviewActive(bool active);

	virtual void ScreenToMap(int screen_x, int screen_y, int* map_x, int* map_y);
	void MouseToMap(int* map_x, int* map_y) {
		ScreenToMap(cursor_x, cursor_y, map_x, map_y);
	}
	virtual void GetScreenCenter(int* map_x, int* map_y);

	virtual void StartPasting();
	virtual void EndPasting();
	virtual void EnterSelectionMode();
	virtual void EnterDrawingMode();

	virtual void UpdatePositionStatus(int x = -1, int y = -1);
	virtual void UpdateZoomStatus();
	void SyncCursorHoverState();

	virtual void ChangeFloor(int new_floor);
	int GetFloor() const {
		return floor;
	}
	double GetZoom() const {
		return zoom;
	}
	virtual void SetZoom(double value);
	virtual void GetViewBox(int* view_scroll_x, int* view_scroll_y, int* screensize_x, int* screensize_y) const;

	Position GetCursorPosition() const;
	void SetLightVisibilityOrigin(const Position& pos);
	void ClearLightVisibilityOrigin();
	std::optional<Position> GetLightVisibilityOrigin() const;

	void TakeScreenshot(wxFileName path, wxString format);

	enum {
		BLOCK_SIZE = 100
	};

	inline int getFillIndex(int x, int y) const {
		return x + BLOCK_SIZE * y;
	}

	static bool processed[BLOCK_SIZE * BLOCK_SIZE];
	Editor& editor;
	std::unique_ptr<MapDrawer> drawer;
	int keyCode;

	// View related
	int floor;
	double zoom;
	int cursor_x;
	int cursor_y;

	bool dragging;
	bool boundbox_selection;
	bool screendragging;
	bool isPasting() const;

	std::unique_ptr<ScreenshotController> screenshot_controller;

	int last_cursor_map_x;
	int last_cursor_map_y;
	int last_cursor_map_z;

	int last_click_map_x;
	int last_click_map_y;
	int last_click_map_z;
	int last_click_abs_x;
	int last_click_abs_y;
	int last_click_x;
	int last_click_y;

	int last_mmb_click_x;
	int last_mmb_click_y;

	int view_scroll_x;
	int view_scroll_y;
	std::optional<Position> light_visibility_origin_override_;

	uint32_t current_house_id;

	wxStopWatch refresh_watch;
	std::unique_ptr<MapPopupMenu> popup_menu;
	std::unique_ptr<AnimationTimer> animation_timer;

	FPSCounter fps_counter;

	friend class MapDrawer;
	friend class SelectionDrawer;
	friend class BrushOverlayDrawer;
	friend class DragShadowDrawer;
	friend class PreviewDrawer;
	friend class SelectionController;
	friend class DrawingController;

	std::unique_ptr<SelectionController> selection_controller;
	std::unique_ptr<DrawingController> drawing_controller;
	std::unique_ptr<MapMenuHandler> menu_handler;

private:
	bool IsAnimationEnabled() const;
	int GetAnimationRefreshIntervalMs() const noexcept;
	void QueueNativeRefresh(bool immediate);
	void EnsureNanoVG();
	void DrawOverlays(NVGcontext* vg, const DrawingOptions& options);
	void PerformGarbageCollection();

	MapWindow* GetMapWindow() const;
	bool renderer_initialized = false;
	long m_last_gc_time = 0;
	bool hover_preview_active_ = false;
	std::chrono::steady_clock::time_point last_animation_refresh_time_{};
};

#endif
