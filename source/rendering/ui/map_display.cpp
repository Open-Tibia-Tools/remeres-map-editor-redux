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

#include <format>
#include <sstream>
#include <time.h>
#include <thread>
#include <chrono>
#include <wx/wfstream.h>
#include <spdlog/spdlog.h>

#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "brushes/brush.h"
#include "game/sprites.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "ui/properties/old_properties_window.h"
#include "ui/properties/properties_window.h"
#include "palette/palette_window.h"
#include "rendering/ui/screenshot_controller.h"
#include "rendering/utilities/tile_describer.h"
#include "rendering/core/coordinate_mapper.h"
#include "rendering/ui/map_display.h"
#include "rendering/ui/map_status_updater.h"
#include "rendering/map_drawer.h"
#include "rendering/core/text_renderer.h"
#include <glad/glad.h>
#include <nanovg.h>
#include <nanovg_gl.h>
#include "app/application.h"
#include "live/live_server.h"
#include "live/live_client.h"
#include "ui/browse_tile_window.h"
#include "ui/dialog_helper.h"
#include "ui/map_tab.h"
#include "game/animation_timer.h"
#include "ui/map_popup_menu.h"
#include "brushes/brush_utility.h"
#include "rendering/ui/clipboard_handler.h"
#include "rendering/ui/keyboard_handler.h"
#include "rendering/ui/brush_selector.h"
#include "rendering/ui/popup_action_handler.h"
#include "rendering/ui/zoom_controller.h"
#include "rendering/ui/navigation_controller.h"
#include "rendering/ui/selection_controller.h"
#include "rendering/ui/drawing_controller.h"
#include "rendering/ui/map_menu_handler.h"
#include "rendering/drawers/overlays/lua_overlay_drawer.h"

#include "brushes/doodad/doodad_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/table/table_brush.h"

bool MapCanvas::processed[] = { 0 };

// Helper to create attributes
static wxGLAttributes& GetCoreProfileAttributes() {
	static wxGLAttributes vAttrs = []() {
		wxGLAttributes a;
		a.PlatformDefaults().Defaults().RGBA().DoubleBuffer().Depth(24).Stencil(8).EndList();
		return a;
	}();
	return vAttrs;
}

MapCanvas::MapCanvas(wxWindow* parent, Editor& editor, int* attriblist) :
	wxGLCanvas(parent, GetCoreProfileAttributes(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS),
	editor(editor),
	floor(GROUND_LAYER),
	zoom(1.0),
	renderer_initialized(false),
	cursor_x(-1),
	cursor_y(-1),
	dragging(false),
	boundbox_selection(false),
	screendragging(false),

	last_cursor_map_x(-1),
	last_cursor_map_y(-1),
	last_cursor_map_z(-1),

	last_click_map_x(-1),
	last_click_map_y(-1),
	last_click_map_z(-1),
	last_click_abs_x(-1),
	last_click_abs_y(-1),
	last_click_x(-1),
	last_click_y(-1),
	last_mmb_click_x(-1),
	last_mmb_click_y(-1),
	m_last_gc_time(0) {
	// Context creation must happen on the main/UI thread
	m_glContext = std::make_unique<wxGLContext>(this, g_gui.GetGLContext(this));
	if (!m_glContext->IsOK()) {
		spdlog::error("MapCanvas: Failed to create wxGLContext");
		m_glContext.reset();
	}

	popup_menu = std::make_unique<MapPopupMenu>(editor);
	animation_timer = std::make_unique<AnimationTimer>(this);
	drawer = std::make_unique<MapDrawer>(this);
	selection_controller = std::make_unique<SelectionController>(this, editor);
	drawing_controller = std::make_unique<DrawingController>(this, editor);
	screenshot_controller = std::make_unique<ScreenshotController>(this);
	menu_handler = std::make_unique<MapMenuHandler>(this, editor);
	menu_handler->BindEvents();
	keyCode = WXK_NONE;

	Bind(wxEVT_KEY_DOWN, &MapCanvas::OnKeyDown, this);
	Bind(wxEVT_KEY_UP, &MapCanvas::OnKeyUp, this);

	Bind(wxEVT_MOTION, &MapCanvas::OnMouseMove, this);
	Bind(wxEVT_LEFT_UP, &MapCanvas::OnMouseLeftRelease, this);
	Bind(wxEVT_LEFT_DOWN, &MapCanvas::OnMouseLeftClick, this);
	Bind(wxEVT_LEFT_DCLICK, &MapCanvas::OnMouseLeftDoubleClick, this);
	Bind(wxEVT_MIDDLE_DOWN, &MapCanvas::OnMouseCenterClick, this);
	Bind(wxEVT_MIDDLE_UP, &MapCanvas::OnMouseCenterRelease, this);
	Bind(wxEVT_RIGHT_DOWN, &MapCanvas::OnMouseRightClick, this);
	Bind(wxEVT_RIGHT_UP, &MapCanvas::OnMouseRightRelease, this);
	Bind(wxEVT_MOUSEWHEEL, &MapCanvas::OnWheel, this);
	Bind(wxEVT_ENTER_WINDOW, &MapCanvas::OnGainMouse, this);
	Bind(wxEVT_LEAVE_WINDOW, &MapCanvas::OnLoseMouse, this);

	Bind(wxEVT_PAINT, &MapCanvas::OnPaint, this);
	Bind(wxEVT_ERASE_BACKGROUND, &MapCanvas::OnEraseBackground, this);
	Bind(wxEVT_IDLE, &MapCanvas::OnIdle, this);
}

MapCanvas::~MapCanvas() {
	bool context_ok = false;
	if (m_glContext) {
		context_ok = g_gl_context.EnsureContextCurrent(*m_glContext, this);
	} else if (auto context = g_gui.GetGLContext(this)) {
		context_ok = g_gl_context.EnsureContextCurrent(*context, this);
	}

	if (!context_ok) {
		spdlog::warn("MapCanvas: Destroying canvas without a current OpenGL context. Cleanup might fail or assert.");
	}

	drawer.reset();
	m_nvg.reset();

	g_gl_context.UnregisterCanvas(this);
}

bool MapCanvas::IsAnimationEnabled() const {
	return g_settings.getBoolean(Config::SHOW_PREVIEW);
}

int MapCanvas::GetAnimationRefreshIntervalMs() const noexcept {
	constexpr double far_zoom_threshold = 2.0;
	constexpr int near_zoom_refresh_interval_ms = 1000 / 60;
	constexpr int far_zoom_refresh_interval_ms = 1000 / 20;
	return zoom <= far_zoom_threshold ? near_zoom_refresh_interval_ms : far_zoom_refresh_interval_ms;
}

void MapCanvas::QueueNativeRefresh(bool immediate) {
	wxGLCanvas::Refresh();
	if (immediate || refresh_watch.Time() > g_settings.getInteger(Config::HARD_REFRESH_RATE)) {
		refresh_watch.Start();
		wxGLCanvas::Update();
	}
}

void MapCanvas::RequestLocalRefresh(bool immediate) {
	QueueNativeRefresh(immediate);
}

void MapCanvas::RequestSharedMapRefresh(bool immediate) {
	g_gui.RefreshView(immediate);
}

void MapCanvas::RequestAnimationRepaint() {
	if (!IsAnimationEnabled()) {
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	const auto refresh_interval = std::chrono::milliseconds(GetAnimationRefreshIntervalMs());
	if (last_animation_refresh_time_ != std::chrono::steady_clock::time_point{} && now - last_animation_refresh_time_ < refresh_interval) {
		return;
	}

	last_animation_refresh_time_ = now;
	RequestLocalRefresh();
}

void MapCanvas::SetHoverPreviewActive(bool active) {
	if (hover_preview_active_ == active) {
		return;
	}

	hover_preview_active_ = active;
	RequestLocalRefresh();
}

void MapCanvas::SetZoom(double value) {
	ZoomController::SetZoom(this, value);
}

void MapCanvas::GetViewBox(int* view_scroll_x, int* view_scroll_y, int* screensize_x, int* screensize_y) const {
	if (auto mw = GetMapWindow()) {
		mw->GetViewSize(screensize_x, screensize_y);
		mw->GetViewStart(view_scroll_x, view_scroll_y);
	} else {
		if (screensize_x) *screensize_x = GetSize().x;
		if (screensize_y) *screensize_y = GetSize().y;
		if (view_scroll_x) *view_scroll_x = 0;
		if (view_scroll_y) *view_scroll_y = 0;
	}
}

MapWindow* MapCanvas::GetMapWindow() const {
	return wxDynamicCast(GetParent(), MapWindow);
}

void MapCanvas::EnsureNanoVG() {
	if (!m_nvg) {
		if (!gladLoadGL()) {
			spdlog::error("MapCanvas: Failed to initialize GLAD");
		}
		m_nvg.reset(nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES));
		if (m_nvg) {
			TextRenderer::LoadFont(m_nvg.get());
		} else {
			spdlog::error("MapCanvas: Failed to initialize NanoVG");
		}
	}
}

void MapCanvas::DrawOverlays(NVGcontext* vg, const DrawingOptions& options) {
	if (!vg || (drawer && !drawer->hasOverlays())) {
		return;
	}

	// Sanitize state before handover to NanoVG
	glUseProgram(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glClear(GL_STENCIL_BUFFER_BIT);
	TextRenderer::BeginFrame(vg, GetSize().x, GetSize().y, GetContentScaleFactor());

	if (options.show_creatures) {
		drawer->DrawCreatureNames(vg);
	}
	if (options.show_tooltips) {
		drawer->DrawTooltips(vg);
	}
	if (options.show_hooks) {
		drawer->DrawHookIndicators(vg);
	}
	if (options.highlight_locked_doors) {
		drawer->DrawDoorIndicators(vg);
	}
	if (drawer->getLuaOverlayDrawer()) {
		drawer->getLuaOverlayDrawer()->DrawUI(vg, drawer->getView(), options);
	}

	TextRenderer::EndFrame(vg);

	// Sanitize state after NanoVG to avoid polluting the next frame or other tabs
	glUseProgram(0);
	glBindVertexArray(0);
}

void MapCanvas::PerformGarbageCollection() {
	// Clean unused textures once every second
	// Only run GC if this is the active tab to prevent multiple tabs from fighting over resources
	long current_time = wxGetLocalTime();
	if (current_time - m_last_gc_time >= 1 && g_gui.GetCurrentMapTab() == GetParent()) {
		g_gui.gfx.garbageCollection();
		m_last_gc_time = current_time;
	}
}

void MapCanvas::OnPaint(wxPaintEvent& event) {
	wxPaintDC dc(this); // validates the paint event
	if (m_glContext) {
		g_gl_context.EnsureContextCurrent(*m_glContext, this);
		g_gl_context.SetFallbackCanvas(this);
		g_gl_context.ApplyVSyncIfNeeded(*this);
	}

	EnsureNanoVG();

	if (g_gui.IsRenderingEnabled()) {
		// Advance graphics clock and drain the preloader queue before rendering
		g_gui.gfx.updateTime();

		DrawingOptions& options = drawer->getOptions();
		if (screenshot_controller->IsCapturing()) {
			options.SetIngame();
		} else {
			options.UpdateIfNeeded();
		}

		options.dragging = selection_controller->IsDragging();
		options.boundbox_selection = selection_controller->IsBoundboxSelection();
		options.is_drawing_mode = g_gui.IsDrawingMode();

		if (options.show_preview) {
			animation_timer->Start();
			g_gui.gfx.resumeAnimation();
		} else {
			animation_timer->Stop();
			g_gui.gfx.pauseAnimation();
			last_animation_refresh_time_ = {};
		}

		// BatchRenderer calls removed - MapDrawer handles its own renderers

		drawer->SetupVars();
		drawer->SetupGL();
		drawer->Draw();

		if (screenshot_controller->IsCapturing()) {
			drawer->TakeScreenshot(screenshot_controller->GetBuffer());
			options.MarkDirty();
		}

		drawer->Release();

		// Draw UI (Tooltips, Overlays & HUD) using NanoVG
		DrawOverlays(m_nvg.get(), options);

		drawer->ClearFrameOverlays();
	}

	SwapBuffers();

	fps_counter.Update();
	if (g_settings.getBoolean(Config::SHOW_FPS_COUNTER) && fps_counter.HasChanged()) {
		MapStatusUpdater::UpdateFPS(fps_counter.GetStatusString());
	}
}

void MapCanvas::OnIdle(wxIdleEvent& event) {
	PerformGarbageCollection();
	event.Skip();
}

void MapCanvas::TakeScreenshot(wxFileName path, wxString format) {
	screenshot_controller->TakeScreenshot(path, format);
}

void MapCanvas::ScreenToMap(int screen_x, int screen_y, int* map_x, int* map_y) {
	int start_x = 0, start_y = 0;
	if (auto mw = GetMapWindow()) {
		mw->GetViewStart(&start_x, &start_y);
	}

	CoordinateMapper::ScreenToMap(screen_x, screen_y, start_x, start_y, zoom, floor, GetContentScaleFactor(), map_x, map_y);
}
#if 0

*map_y = int(start_y + (screen_y * zoom)) / TILE_SIZE;
}

if (floor <= GROUND_LAYER) {
	*map_x += GROUND_LAYER - floor;
	*map_y += GROUND_LAYER - floor;
} /* else {
	 *map_x += MAP_MAX_LAYER - floor;
	 *map_y += MAP_MAX_LAYER - floor;
 }*/
}

#endif
void MapCanvas::GetScreenCenter(int* map_x, int* map_y) {
	const wxSize logical_size = GetSize();
	ScreenToMap(logical_size.x / 2, logical_size.y / 2, map_x, map_y);
}

Position MapCanvas::GetCursorPosition() const {
	return Position(last_cursor_map_x, last_cursor_map_y, floor);
}

void MapCanvas::SetLightVisibilityOrigin(const Position& pos) {
	light_visibility_origin_override_ = pos;
}

void MapCanvas::ClearLightVisibilityOrigin() {
	light_visibility_origin_override_.reset();
}

std::optional<Position> MapCanvas::GetLightVisibilityOrigin() const {
	return light_visibility_origin_override_;
}

void MapCanvas::UpdatePositionStatus(int x, int y) {
	if (x == -1) {
		x = cursor_x;
	}
	if (y == -1) {
		y = cursor_y;
	}

	int map_x, map_y;
	ScreenToMap(x, y, &map_x, &map_y);

	MapStatusUpdater::Update(editor, map_x, map_y, floor);
}

void MapCanvas::UpdateZoomStatus() {
	ZoomController::UpdateStatus(this);
}

void MapCanvas::SyncCursorHoverState() {
	int mouse_map_x, mouse_map_y;
	MouseToMap(&mouse_map_x, &mouse_map_y);

	const bool map_update = last_cursor_map_x != mouse_map_x || last_cursor_map_y != mouse_map_y || last_cursor_map_z != floor;
	last_cursor_map_x = mouse_map_x;
	last_cursor_map_y = mouse_map_y;
	last_cursor_map_z = floor;

	g_gui.UpdateAutoborderPreview(Position(mouse_map_x, mouse_map_y, floor));
	UpdatePositionStatus(cursor_x, cursor_y);
	UpdateZoomStatus();

	if (map_update) {
		RequestLocalRefresh();
	}
}

void MapCanvas::OnMouseMove(wxMouseEvent& event) {
	NavigationController::HandleMouseDrag(this, event);

	cursor_x = event.GetX();
	cursor_y = event.GetY();

	int mouse_map_x, mouse_map_y;
	MouseToMap(&mouse_map_x, &mouse_map_y);

	bool map_update = false;
	if (last_cursor_map_x != mouse_map_x || last_cursor_map_y != mouse_map_y || last_cursor_map_z != floor) {
		map_update = true;
	}
	last_cursor_map_x = mouse_map_x;
	last_cursor_map_y = mouse_map_y;
	last_cursor_map_z = floor;

	if (screendragging) {
		return;
	}

	if (map_update) {
		g_gui.UpdateAutoborderPreview(Position(mouse_map_x, mouse_map_y, floor));
		UpdatePositionStatus(cursor_x, cursor_y);
		UpdateZoomStatus();
		RequestLocalRefresh();
	}

	if (g_gui.IsSelectionMode()) {
		selection_controller->HandleDrag(Position(mouse_map_x, mouse_map_y, floor), event.ShiftDown(), event.ControlDown(), event.AltDown());
	} else { // Drawing mode
		drawing_controller->HandleDrag(Position(mouse_map_x, mouse_map_y, floor), event.ShiftDown(), event.ControlDown(), event.AltDown());
	}
}

void MapCanvas::OnMouseLeftRelease(wxMouseEvent& event) {
	OnMouseActionRelease(event);
}

void MapCanvas::OnMouseLeftClick(wxMouseEvent& event) {
	OnMouseActionClick(event);
}

void MapCanvas::OnMouseLeftDoubleClick(wxMouseEvent& event) {
	int mouse_map_x, mouse_map_y;
	ScreenToMap(event.GetX(), event.GetY(), &mouse_map_x, &mouse_map_y);
	if (selection_controller) {
		selection_controller->HandleDoubleClick(Position(mouse_map_x, mouse_map_y, floor));
	}
}

void MapCanvas::OnMouseCenterClick(wxMouseEvent& event) {
	if (g_settings.getInteger(Config::SWITCH_MOUSEBUTTONS)) {
		OnMousePropertiesClick(event);
	} else {
		OnMouseCameraClick(event);
	}
}

void MapCanvas::OnMouseCenterRelease(wxMouseEvent& event) {
	if (g_settings.getInteger(Config::SWITCH_MOUSEBUTTONS)) {
		OnMousePropertiesRelease(event);
	} else {
		OnMouseCameraRelease(event);
	}
}

void MapCanvas::OnMouseRightClick(wxMouseEvent& event) {
	if (g_settings.getInteger(Config::SWITCH_MOUSEBUTTONS)) {
		OnMouseCameraClick(event);
	} else {
		OnMousePropertiesClick(event);
	}
}

void MapCanvas::OnMouseRightRelease(wxMouseEvent& event) {
	if (g_settings.getInteger(Config::SWITCH_MOUSEBUTTONS)) {
		OnMouseCameraRelease(event);
	} else {
		OnMousePropertiesRelease(event);
	}
}

void MapCanvas::OnMouseActionClick(wxMouseEvent& event) {
	SetFocus();
	if (auto* map_window = GetMapWindow()) {
		map_window->ResumeMinimapTrackingToCurrentView();
	}

	int mouse_map_x, mouse_map_y;
	ScreenToMap(event.GetX(), event.GetY(), &mouse_map_x, &mouse_map_y);

	if (event.ControlDown() && event.AltDown()) {
		Tile* tile = editor.map.getTile(mouse_map_x, mouse_map_y, floor);
		BrushSelector::SelectSmartBrush(editor, tile);
	} else if (g_gui.IsSelectionMode()) {
		if (selection_controller) {
			selection_controller->HandleClick(Position(mouse_map_x, mouse_map_y, floor), event.ShiftDown(), event.ControlDown(), event.AltDown());
		}
	} else if (g_gui.GetCurrentBrush()) { // Drawing mode
		if (drawing_controller) {
			drawing_controller->HandleClick(Position(mouse_map_x, mouse_map_y, floor), event.ShiftDown(), event.ControlDown(), event.AltDown());
		}
	}
	last_click_x = int(event.GetX() * zoom);
	last_click_y = int(event.GetY() * zoom);

	int start_x, start_y;
	if (auto mw = GetMapWindow()) {
		mw->GetViewStart(&start_x, &start_y);
	} else {
		start_x = 0; start_y = 0;
	}
	last_click_abs_x = last_click_x + start_x;
	last_click_abs_y = last_click_y + start_y;

	last_click_map_x = mouse_map_x;
	last_click_map_y = mouse_map_y;
	last_click_map_z = floor;
	SetLightVisibilityOrigin(Position(mouse_map_x, mouse_map_y, floor));
	RequestSharedMapRefresh();
	g_gui.UpdateMinimap();
}

void MapCanvas::OnMouseActionRelease(wxMouseEvent& event) {
	int mouse_map_x, mouse_map_y;
	ScreenToMap(event.GetX(), event.GetY(), &mouse_map_x, &mouse_map_y);

	int move_x = last_click_map_x - mouse_map_x;
	int move_y = last_click_map_y - mouse_map_y;
	int move_z = last_click_map_z - floor;

	if (g_gui.IsSelectionMode()) {
		if (selection_controller) {
			selection_controller->HandleRelease(Position(mouse_map_x, mouse_map_y, floor), event.ShiftDown(), event.ControlDown(), event.AltDown());
		}
	} else if (g_gui.GetCurrentBrush()) { // Drawing mode
		if (drawing_controller) {
			drawing_controller->HandleRelease(Position(mouse_map_x, mouse_map_y, floor), event.ShiftDown(), event.ControlDown(), event.AltDown());
		}
	}
	RequestSharedMapRefresh();
	g_gui.UpdateMinimap();
}

void MapCanvas::OnMouseCameraClick(wxMouseEvent& event) {
	SetFocus();
	if (auto* map_window = GetMapWindow()) {
		map_window->ResumeMinimapTrackingToCurrentView();
	}

	last_mmb_click_x = event.GetX();
	last_mmb_click_y = event.GetY();
	if (event.ControlDown()) {
		ZoomController::ApplyRelativeZoom(this, 1.0 - zoom);
	} else {
		NavigationController::HandleCameraClick(this, event);
	}
}

void MapCanvas::OnMouseCameraRelease(wxMouseEvent& event) {
	NavigationController::HandleCameraRelease(this, event);
}

void MapCanvas::OnMousePropertiesClick(wxMouseEvent& event) {
	SetFocus();
	if (auto* map_window = GetMapWindow()) {
		map_window->ResumeMinimapTrackingToCurrentView();
	}

	int mouse_map_x, mouse_map_y;
	ScreenToMap(event.GetX(), event.GetY(), &mouse_map_x, &mouse_map_y);
	Tile* tile = editor.map.getTile(mouse_map_x, mouse_map_y, floor);

	if (g_gui.IsDrawingMode()) {
		g_gui.SetSelectionMode();
	}

	last_click_x = int(event.GetX() * zoom);
	last_click_y = int(event.GetY() * zoom);
	selection_controller->HandlePropertiesClick(Position(mouse_map_x, mouse_map_y, floor), event.ShiftDown(), event.ControlDown(), event.AltDown());

	int start_x = 0, start_y = 0;
	if (auto mw = GetMapWindow()) {
		mw->GetViewStart(&start_x, &start_y);
	}
	last_click_abs_x = last_click_x + start_x;
	last_click_abs_y = last_click_y + start_y;

	last_click_map_x = mouse_map_x;
	last_click_map_y = mouse_map_y;
	RequestLocalRefresh();
}

void MapCanvas::OnMousePropertiesRelease(wxMouseEvent& event) {
	int mouse_map_x, mouse_map_y;
	ScreenToMap(event.GetX(), event.GetY(), &mouse_map_x, &mouse_map_y);

	if (g_gui.IsDrawingMode()) {
		g_gui.SetSelectionMode();
	}

	selection_controller->HandlePropertiesRelease(Position(mouse_map_x, mouse_map_y, floor), event.ShiftDown(), event.ControlDown(), event.AltDown());

	popup_menu->Update();
	PopupMenu(popup_menu.get());

	editor.actionQueue->resetTimer();
	dragging = false;
	boundbox_selection = false;

	last_cursor_map_x = mouse_map_x;
	last_cursor_map_y = mouse_map_y;
	last_cursor_map_z = floor;

	RequestLocalRefresh();
}

void MapCanvas::OnWheel(wxMouseEvent& event) {
	if (event.ControlDown()) {
		NavigationController::HandleWheel(this, event);
	} else if (event.AltDown()) {
		drawing_controller->HandleWheel(event.GetWheelRotation(), event.AltDown(), event.ControlDown());
	} else {
		ZoomController::OnWheel(this, event);
	}

	RequestLocalRefresh();
}

void MapCanvas::OnLoseMouse(wxMouseEvent& event) {
	RequestLocalRefresh();
}

void MapCanvas::OnGainMouse(wxMouseEvent& event) {
	if (!event.LeftIsDown()) {
		dragging = false;
		boundbox_selection = false;
		drawing_controller->Reset();
	}
	if (!event.MiddleIsDown()) {
		screendragging = false;
	}

	RequestLocalRefresh();
}

void MapCanvas::OnKeyDown(wxKeyEvent& event) {
	KeyboardHandler::OnKeyDown(this, event);
}

void MapCanvas::OnKeyUp(wxKeyEvent& event) {
	KeyboardHandler::OnKeyUp(this, event);
}

void MapCanvas::ChangeFloor(int new_floor) {
	NavigationController::ChangeFloor(this, new_floor);
}

void MapCanvas::EnterDrawingMode() {
	dragging = false;
	boundbox_selection = false;
	EndPasting();
	RequestLocalRefresh();
}

void MapCanvas::EnterSelectionMode() {
	drawing_controller->Reset();
	editor.replace_brush = nullptr;
	RequestLocalRefresh();
}

bool MapCanvas::isPasting() const {
	return g_gui.IsPasting();
}

void MapCanvas::StartPasting() {
	g_gui.StartPasting();
}

void MapCanvas::EndPasting() {
	g_gui.EndPasting();
}

void MapCanvas::Reset() {
	cursor_x = 0;
	cursor_y = 0;

	zoom = 1.0;
	floor = GROUND_LAYER;

	dragging = false;
	boundbox_selection = false;
	screendragging = false;
	drawing_controller->Reset();

	editor.replace_brush = nullptr;

	last_click_map_x = -1;
	last_click_map_y = -1;
	last_click_map_z = -1;

	last_mmb_click_x = -1;
	last_mmb_click_y = -1;
	ClearLightVisibilityOrigin();

	editor.selection.clear();
	editor.actionQueue->clear();
}
