#include "app/main.h"

#include <cstring>
#include <format>
#include <spdlog/spdlog.h>
#include <vector>
#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/image.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/tglbtn.h>

#include "editor/editor.h"
#include "rendering/drawers/minimap_drawer.h"
#include "rendering/io/screen_capture.h"
#include "rendering/ui/map_display.h"
#include "rendering/ui/minimap_viewport.h"
#include "rendering/ui/minimap_window.h"
#include "ui/dialog_util.h"
#include "ui/gui.h"
#include "ui/map_tab.h"
#include "ui/map_window.h"
#include "util/image_manager.h"

namespace {

wxGLAttributes& GetCoreProfileAttributes() {
	static wxGLAttributes attributes = []() {
		wxGLAttributes attr;
		attr.PlatformDefaults().Defaults().RGBA().DoubleBuffer().Depth(24).Stencil(8).EndList();
		return attr;
	}();
	return attributes;
}

wxSize dipSize(wxWindow* window, const wxSize& size) {
	return window->FromDIP(size);
}

int dip(wxWindow* window, int value) {
	return window->FromDIP(value);
}

wxButton* createHeaderButton(wxWindow* parent, const wxBitmap& bitmap, const wxString& label = wxString()) {
	const wxSize button_size = dipSize(parent, wxSize(24, 22));
	auto* button = new wxButton(parent, wxID_ANY, label, wxDefaultPosition, button_size, wxBU_EXACTFIT);
	if (bitmap.IsOk()) {
		button->SetBitmap(bitmap);
	}
	button->SetMinSize(button_size);
	return button;
}

wxToggleButton* createHeaderToggleButton(wxWindow* parent, const wxString& label) {
	const wxSize button_size = dipSize(parent, wxSize(32, 22));
	auto* button = new wxToggleButton(parent, wxID_ANY, label, wxDefaultPosition, button_size, wxBU_EXACTFIT);
	button->SetMinSize(button_size);
	return button;
}

wxPanel* createSeparator(wxWindow* parent) {
	const wxSize separator_size = dipSize(parent, wxSize(1, 18));
	auto* separator = new wxPanel(parent, wxID_ANY, wxDefaultPosition, separator_size);
	separator->SetMinSize(separator_size);
	separator->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
	return separator;
}

} // namespace

MinimapWindow::MinimapWindow(wxWindow* parent) :
	wxPanel(parent, wxID_ANY, wxDefaultPosition, dipSize(parent, wxSize(260, 200))) {
	auto* root_sizer = new wxBoxSizer(wxVERTICAL);
	auto* header_sizer = new wxBoxSizer(wxHORIZONTAL);
	const int pad_2 = dip(this, 2);
	const int pad_4 = dip(this, 4);
	const int pad_6 = dip(this, 6);

	const wxSize icon_size = dipSize(this, wxSize(14, 14));
	zoom_out_button_ = createHeaderButton(this, IMAGE_MANAGER.GetBitmap(ICON_MINUS, icon_size));
	zoom_in_button_ = createHeaderButton(this, IMAGE_MANAGER.GetBitmap(ICON_PLUS, icon_size));
	floor_up_button_ = createHeaderButton(this, IMAGE_MANAGER.GetBitmap(ICON_ARROW_UP, icon_size));
	floor_down_button_ = createHeaderButton(this, IMAGE_MANAGER.GetBitmap(ICON_ARROW_DOWN, icon_size));
	show_all_floors_button_ = createHeaderToggleButton(this, "All");
	screenshot_button_ = createHeaderButton(this, IMAGE_MANAGER.GetBitmap(ICON_CAMERA, icon_size));
	help_button_ = createHeaderButton(this, IMAGE_MANAGER.GetBitmap(ICON_QUESTION_CIRCLE, icon_size));
	zoom_label_ = new wxStaticText(this, wxID_ANY, "9");
	floor_label_ = new wxStaticText(this, wxID_ANY, "F: 7");

	header_sizer->Add(zoom_out_button_, 0, wxALL | wxALIGN_CENTER_VERTICAL, pad_2);
	header_sizer->Add(zoom_label_, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, pad_4);
	header_sizer->Add(zoom_in_button_, 0, wxALL | wxALIGN_CENTER_VERTICAL, pad_2);
	header_sizer->Add(createSeparator(this), 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, pad_6);
	header_sizer->Add(floor_label_, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, pad_4);
	header_sizer->Add(floor_up_button_, 0, wxALL | wxALIGN_CENTER_VERTICAL, pad_2);
	header_sizer->Add(floor_down_button_, 0, wxALL | wxALIGN_CENTER_VERTICAL, pad_2);
	header_sizer->Add(createSeparator(this), 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, pad_6);
	header_sizer->Add(show_all_floors_button_, 0, wxALL | wxALIGN_CENTER_VERTICAL, pad_2);
	header_sizer->Add(createSeparator(this), 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, pad_6);
	header_sizer->Add(screenshot_button_, 0, wxALL | wxALIGN_CENTER_VERTICAL, pad_2);
	header_sizer->Add(help_button_, 0, wxALL | wxALIGN_CENTER_VERTICAL, pad_2);
	header_sizer->AddStretchSpacer(1);

	canvas_ = new MinimapCanvas(this);

	help_panel_ = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
	help_panel_->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
	auto* help_sizer = new wxBoxSizer(wxVERTICAL);
	help_text_ = new wxStaticText(help_panel_, wxID_ANY,
		"Left drag: pan\n"
		"Ctrl+Left Click: move camera\n"
		"Wheel: zoom\n"
		"Ctrl+Wheel: change floor");
	help_sizer->Add(help_text_, 0, wxALL | wxEXPAND, pad_6);
	help_panel_->SetSizer(help_sizer);
	help_panel_->Hide();

	root_sizer->Add(header_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad_2);
	root_sizer->Add(canvas_, 1, wxEXPAND | wxALL, pad_2);
	SetSizer(root_sizer);

	Bind(wxEVT_CLOSE_WINDOW, &MinimapWindow::OnClose, this);
	Bind(wxEVT_SIZE, &MinimapWindow::OnSize, this);
	zoom_out_button_->Bind(wxEVT_BUTTON, &MinimapWindow::OnZoomOut, this);
	zoom_in_button_->Bind(wxEVT_BUTTON, &MinimapWindow::OnZoomIn, this);
	floor_up_button_->Bind(wxEVT_BUTTON, &MinimapWindow::OnFloorUp, this);
	floor_down_button_->Bind(wxEVT_BUTTON, &MinimapWindow::OnFloorDown, this);
	show_all_floors_button_->Bind(wxEVT_TOGGLEBUTTON, &MinimapWindow::OnShowAllFloorsToggle, this);
	screenshot_button_->Bind(wxEVT_BUTTON, &MinimapWindow::OnScreenshot, this);
	help_button_->Bind(wxEVT_BUTTON, &MinimapWindow::OnHelpClick, this);
	help_button_->Bind(wxEVT_ENTER_WINDOW, &MinimapWindow::OnHelpEnter, this);
	help_button_->Bind(wxEVT_LEAVE_WINDOW, &MinimapWindow::OnHelpLeave, this);

	SyncHeaderState();
	PositionHelpPanel();
}

MinimapWindow::~MinimapWindow() {
}

void MinimapWindow::DelayedUpdate() {
	RefreshMinimap(false);
}

void MinimapWindow::RefreshMinimap(bool immediate) {
	EnsureActiveViewportState();
	SyncHeaderState();
	if (!canvas_) {
		return;
	}

	if (immediate) {
		canvas_->Refresh();
	} else {
		canvas_->DelayedUpdate();
	}
}

void MinimapWindow::SyncHeaderState() {
	if (g_gui.IsLoading()) {
		zoom_label_->SetLabel("9");
		floor_label_->SetLabel("F: -");
		zoom_out_button_->Enable(false);
		zoom_in_button_->Enable(false);
		floor_up_button_->Enable(false);
		floor_down_button_->Enable(false);
		show_all_floors_button_->SetValue(true);
		show_all_floors_button_->Enable(false);
		screenshot_button_->Enable(false);
	} else if (auto* state = GetActiveViewportState()) {
		zoom_label_->SetLabel(wxString::FromUTF8(MinimapViewport::GetZoomLabel(state->zoom_step).data()));
		const std::string floor_label = std::format("F: {}", state->floor);
		floor_label_->SetLabel(wxString::FromUTF8(floor_label.c_str()));
		zoom_out_button_->Enable(state->zoom_step > 0);
		zoom_in_button_->Enable(state->zoom_step < static_cast<int>(MinimapViewport::ZoomFactors.size()) - 1);
		floor_up_button_->Enable(state->floor > 0);
		floor_down_button_->Enable(state->floor < MAP_MAX_LAYER);
		show_all_floors_button_->SetValue(state->show_all_floors);
		show_all_floors_button_->Enable(true);
		screenshot_button_->Enable(canvas_ != nullptr);
	} else {
		zoom_label_->SetLabel("9");
		floor_label_->SetLabel("F: -");
		zoom_out_button_->Enable(false);
		zoom_in_button_->Enable(false);
		floor_up_button_->Enable(false);
		floor_down_button_->Enable(false);
		show_all_floors_button_->SetValue(true);
		show_all_floors_button_->Enable(false);
		screenshot_button_->Enable(false);
	}
}

void MinimapWindow::StepZoom(int delta) {
	if (auto* state = GetActiveViewportState()) {
		const int next_step = MinimapViewport::ClampZoomStep(state->zoom_step + delta);
		if (next_step != state->zoom_step) {
			state->zoom_step = next_step;
			if (canvas_) {
				canvas_->NormalizeViewportState();
				canvas_->Refresh();
			}
		}
	}
	SyncHeaderState();
}

void MinimapWindow::StepFloor(int delta) {
	if (auto* state = GetActiveViewportState()) {
		const int next_floor = MinimapViewport::ClampFloor(state->floor + delta);
		if (next_floor != state->floor) {
			state->floor = next_floor;
			if (canvas_) {
				canvas_->Refresh();
			}
		}
	}
	SyncHeaderState();
}

void MinimapWindow::SetShowAllFloors(bool show_all_floors) {
	if (auto* state = GetActiveViewportState()) {
		if (state->show_all_floors != show_all_floors) {
			state->show_all_floors = show_all_floors;
			if (canvas_) {
				canvas_->Refresh();
			}
		}
	}
	SyncHeaderState();
}

void MinimapWindow::JumpCameraTo(int map_x, int map_y) {
	auto* state = GetActiveViewportState();
	if (!state) {
		return;
	}

	g_gui.SetScreenCenterPosition(Position(map_x, map_y, state->floor));
	g_gui.RefreshView();
	RefreshMinimap(true);
}

Editor* MinimapWindow::GetActiveEditor() const {
	MapTab* map_tab = g_gui.GetCurrentMapTab();
	return map_tab ? map_tab->GetEditor() : nullptr;
}

MapCanvas* MinimapWindow::GetActiveCanvas() const {
	MapTab* map_tab = g_gui.GetCurrentMapTab();
	return map_tab ? map_tab->GetCanvas() : nullptr;
}

MapWindow* MinimapWindow::GetActiveMapWindow() const {
	MapTab* map_tab = g_gui.GetCurrentMapTab();
	return map_tab ? map_tab->GetView() : nullptr;
}

MinimapViewportState* MinimapWindow::GetActiveViewportState() const {
	if (g_gui.IsLoading()) {
		return nullptr;
	}

	MapWindow* map_window = GetActiveMapWindow();
	if (!map_window) {
		return nullptr;
	}

	if (!map_window->GetMinimapViewportState().initialized) {
		return nullptr;
	}
	return &map_window->GetMinimapViewportState();
}

MinimapViewportState* MinimapWindow::EnsureActiveViewportState() {
	if (g_gui.IsLoading()) {
		return nullptr;
	}

	MapWindow* map_window = GetActiveMapWindow();
	if (!map_window) {
		return nullptr;
	}

	map_window->EnsureMinimapViewportInitialized();
	return &map_window->GetMinimapViewportState();
}

void MinimapWindow::OnClose(wxCloseEvent& event) {
	wxUnusedVar(event);
	g_gui.DestroyMinimap();
}

void MinimapWindow::OnSize(wxSizeEvent& event) {
	SyncHeaderState();
	PositionHelpPanel();
	event.Skip();
}

void MinimapWindow::OnZoomOut(wxCommandEvent& event) {
	wxUnusedVar(event);
	StepZoom(-1);
}

void MinimapWindow::OnZoomIn(wxCommandEvent& event) {
	wxUnusedVar(event);
	StepZoom(1);
}

void MinimapWindow::OnFloorUp(wxCommandEvent& event) {
	wxUnusedVar(event);
	StepFloor(-1);
}

void MinimapWindow::OnFloorDown(wxCommandEvent& event) {
	wxUnusedVar(event);
	StepFloor(1);
}

void MinimapWindow::OnShowAllFloorsToggle(wxCommandEvent& event) {
	SetShowAllFloors(event.IsChecked());
}

void MinimapWindow::OnScreenshot(wxCommandEvent& event) {
	wxUnusedVar(event);
	if (!canvas_) {
		return;
	}

	wxFileDialog dialog(this, "Save minimap screenshot", "", "minimap.png", "PNG image (*.png)|*.png", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	if (dialog.ShowModal() != wxID_OK) {
		return;
	}

	wxFileName file(dialog.GetPath());
	if (file.GetExt().empty()) {
		file.SetExt("png");
	}

	if (!canvas_->SaveCleanScreenshot(file)) {
		DialogUtil::PopupDialog(this, "Screenshot Error", "Failed to save minimap screenshot.", wxOK);
	}
}

void MinimapWindow::OnHelpEnter(wxMouseEvent& event) {
	help_hovered_ = true;
	UpdateHelpVisibility();
	event.Skip();
}

void MinimapWindow::OnHelpLeave(wxMouseEvent& event) {
	help_hovered_ = false;
	UpdateHelpVisibility();
	event.Skip();
}

void MinimapWindow::OnHelpClick(wxCommandEvent& event) {
	wxUnusedVar(event);
	help_pinned_ = !help_pinned_;
	UpdateHelpVisibility();
}

void MinimapWindow::UpdateHelpVisibility() {
	const bool show_help = help_hovered_ || help_pinned_;
	PositionHelpPanel();
	help_panel_->Show(show_help);
	if (show_help) {
		help_panel_->Raise();
	}
}

void MinimapWindow::PositionHelpPanel() {
	if (!help_panel_ || !canvas_) {
		return;
	}

	const wxPoint canvas_position = canvas_->GetPosition();
	const wxSize canvas_size = canvas_->GetSize();
	const int panel_margin = dip(this, 8);
	const wxSize best_size = help_panel_->GetBestSize();
	const int panel_width = std::min(best_size.GetWidth(), std::max(dip(this, 180), canvas_size.GetWidth() - panel_margin * 2));
	help_panel_->SetInitialSize(wxSize(panel_width, wxDefaultCoord));
	help_panel_->Layout();
	help_panel_->Fit();

	const wxSize panel_size = help_panel_->GetBestSize();
	help_panel_->SetSize(
		canvas_position.x + panel_margin,
		canvas_position.y + panel_margin,
		panel_width,
		panel_size.GetHeight());
}

MinimapCanvas::MinimapCanvas(MinimapWindow* parent) :
	wxGLCanvas(parent, GetCoreProfileAttributes(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS),
	owner_(parent),
	drawer(std::make_unique<MinimapDrawer>()),
	update_timer(this) {
	m_glContext = std::make_unique<wxGLContext>(this, g_gui.GetGLContext(this));
	if (!m_glContext || !m_glContext->IsOK()) {
		spdlog::error("MinimapCanvas: Failed to create wxGLContext");
	}

	Bind(wxEVT_LEFT_DOWN, &MinimapCanvas::OnLeftDown, this);
	Bind(wxEVT_LEFT_UP, &MinimapCanvas::OnLeftUp, this);
	Bind(wxEVT_MOTION, &MinimapCanvas::OnMouseMove, this);
	Bind(wxEVT_MOUSEWHEEL, &MinimapCanvas::OnMouseWheel, this);
	Bind(wxEVT_ENTER_WINDOW, &MinimapCanvas::OnMouseEnter, this);
	Bind(wxEVT_LEAVE_WINDOW, &MinimapCanvas::OnMouseLeave, this);
	Bind(wxEVT_SIZE, &MinimapCanvas::OnSize, this);
	Bind(wxEVT_PAINT, &MinimapCanvas::OnPaint, this);
	Bind(wxEVT_ERASE_BACKGROUND, &MinimapCanvas::OnEraseBackground, this);
	Bind(wxEVT_TIMER, &MinimapCanvas::OnDelayedUpdate, this, wxID_ANY);
	Bind(wxEVT_KEY_DOWN, &MinimapCanvas::OnKey, this);
}

MinimapCanvas::~MinimapCanvas() {
	StopInteraction();

	bool context_ok = false;
	if (m_glContext) {
		context_ok = g_gl_context.EnsureContextCurrent(*m_glContext, this);
	}

	if (context_ok) {
		if (drawer) {
			drawer->ReleaseGL();
		}
	} else {
		spdlog::warn("MinimapCanvas: Destroying without a current OpenGL context. Cleanup might be incomplete.");
	}

	g_gl_context.UnregisterCanvas(this);
}

void MinimapCanvas::DelayedUpdate() {
	update_timer.Start(g_settings.getInteger(Config::MINIMAP_UPDATE_DELAY), true);
}

void MinimapCanvas::NormalizeViewportState() {
	if (auto* state = owner_->GetActiveViewportState()) {
		ClampViewportState(*state);
	}
}

bool MinimapCanvas::SaveCleanScreenshot(const wxFileName& file) {
	if (!m_glContext) {
		return false;
	}

	const wxSize size = GetClientSize();
	const int width = size.GetWidth();
	const int height = size.GetHeight();
	if (width <= 0 || height <= 0) {
		return false;
	}

	SetCurrent(*m_glContext);
	static bool glad_initialized = false;
	if (!glad_initialized) {
		if (!gladLoadGL()) {
			spdlog::error("MinimapCanvas::SaveCleanScreenshot - Failed to load GLAD");
			return false;
		}
		glad_initialized = true;
	}

	Editor* editor = owner_->GetActiveEditor();
	MapCanvas* active_canvas = owner_->GetActiveCanvas();
	MinimapViewportState* state = owner_->GetActiveViewportState();
	if (!editor || !active_canvas || !state) {
		return false;
	}

	ClampViewportState(*state);
	drawer->Draw(size, *editor, *active_canvas, *state, {
		.drawCameraBox = false,
		.drawBoundsBorder = false,
		.floor_visibility_mode = SanitizeFloorVisibilityMode(g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE)),
	});

	std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * PixelFormatRGB);
	glReadBuffer(GL_BACK);
	ScreenCapture::Capture(width, height, pixels.data());

	wxImage image(width, height);
	if (!image.IsOk() || !image.GetData()) {
		return false;
	}
	std::memcpy(image.GetData(), pixels.data(), pixels.size());

	const bool saved = image.SaveFile(file.GetFullPath(), wxBITMAP_TYPE_PNG);
	Refresh();
	return saved;
}

void MinimapCanvas::OnPaint(wxPaintEvent& event) {
	wxUnusedVar(event);
	wxPaintDC dc(this);

	if (!m_glContext) {
		spdlog::error("MinimapCanvas::OnPaint - No context");
		return;
	}

	SetCurrent(*m_glContext);
	g_gl_context.ApplyVSyncIfNeeded(*this);

	static bool glad_initialized = false;
	if (!glad_initialized) {
		if (!gladLoadGL()) {
			spdlog::error("MinimapCanvas::OnPaint - Failed to load GLAD");
			return;
		}
		glad_initialized = true;
	}

	if (g_gui.IsLoading() || !g_gui.IsEditorOpen() || !g_gui.IsRenderingEnabled()) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		SwapBuffers();
		return;
	}

	Editor* editor = owner_->GetActiveEditor();
	MapCanvas* active_canvas = owner_->GetActiveCanvas();
	MinimapViewportState* state = owner_->GetActiveViewportState();
	if (!editor || !active_canvas || !state) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		SwapBuffers();
		return;
	}

	ClampViewportState(*state);
	drawer->Draw(GetClientSize(), *editor, *active_canvas, *state, {
		.drawCameraBox = g_settings.getBoolean(Config::MINIMAP_VIEW_BOX),
		.drawBoundsBorder = true,
		.floor_visibility_mode = SanitizeFloorVisibilityMode(g_settings.getInteger(Config::FLOOR_VISIBILITY_MODE)),
	});
	SwapBuffers();
}

void MinimapCanvas::OnDelayedUpdate(wxTimerEvent& event) {
	wxUnusedVar(event);
	Refresh();
}

void MinimapCanvas::OnSize(wxSizeEvent& event) {
	NormalizeViewportState();
	Refresh();
	event.Skip();
}

void MinimapCanvas::OnLeftDown(wxMouseEvent& event) {
	if (g_gui.IsLoading() || !g_gui.IsEditorOpen()) {
		return;
	}

	SetFocus();
	CaptureMouse();

	if (auto* state = owner_->GetActiveViewportState()) {
		if (event.ControlDown()) {
			ctrl_click_pending_ = true;
			is_dragging_ = false;
			drag_origin_ = event.GetPosition();
		} else {
			is_dragging_ = true;
			ctrl_click_pending_ = false;
			drag_origin_ = event.GetPosition();
			drag_origin_center_x_ = state->center_x;
			drag_origin_center_y_ = state->center_y;
			state->tracking_main_camera = false;
		}
	}
}

void MinimapCanvas::OnLeftUp(wxMouseEvent& event) {
	if (g_gui.IsLoading() || !g_gui.IsEditorOpen()) {
		StopInteraction();
		return;
	}

	if (ctrl_click_pending_ && event.ControlDown()) {
		int map_x = 0;
		int map_y = 0;
		drawer->ScreenToMap(event.GetX(), event.GetY(), map_x, map_y);
		owner_->JumpCameraTo(map_x, map_y);
	}

	StopInteraction();
}

void MinimapCanvas::OnMouseMove(wxMouseEvent& event) {
	if (ctrl_click_pending_ && event.LeftIsDown()) {
		const wxPoint delta = event.GetPosition() - drag_origin_;
		if (std::abs(delta.x) > 2 || std::abs(delta.y) > 2) {
			ctrl_click_pending_ = false;
		}
	}

	if (!is_dragging_ || !event.LeftIsDown()) {
		return;
	}

	MinimapViewportState* state = owner_->GetActiveViewportState();
	if (!state) {
		return;
	}

	const wxPoint delta = event.GetPosition() - drag_origin_;
	const double zoom_factor = MinimapViewport::GetZoomFactor(state->zoom_step);
	state->center_x = drag_origin_center_x_ - delta.x * zoom_factor;
	state->center_y = drag_origin_center_y_ - delta.y * zoom_factor;
	ClampViewportState(*state);
	owner_->SyncHeaderState();
	Refresh();
}

void MinimapCanvas::OnMouseWheel(wxMouseEvent& event) {
	if (g_gui.IsLoading() || !g_gui.IsEditorOpen()) {
		return;
	}

	if (event.ControlDown()) {
		owner_->StepFloor(event.GetWheelRotation() > 0 ? 1 : -1);
	} else {
		auto* state = owner_->GetActiveViewportState();
		Editor* editor = owner_->GetActiveEditor();
		if (!state || !editor) {
			return;
		}

		const int next_step = MinimapViewport::ClampZoomStep(state->zoom_step + (event.GetWheelRotation() > 0 ? 1 : -1));
		if (next_step == state->zoom_step) {
			return;
		}

		const wxSize size = GetClientSize();
		const int safe_width = std::max(1, size.GetWidth());
		const int safe_height = std::max(1, size.GetHeight());
		const double normalized_x = std::clamp(event.GetX() / static_cast<double>(safe_width), 0.0, 1.0);
		const double normalized_y = std::clamp(event.GetY() / static_cast<double>(safe_height), 0.0, 1.0);

		const double old_zoom_factor = MinimapViewport::GetZoomFactor(state->zoom_step);
		const double old_visible_width = std::max(1.0, safe_width * old_zoom_factor);
		const double old_visible_height = std::max(1.0, safe_height * old_zoom_factor);
		const double old_start_x = state->center_x - old_visible_width / 2.0;
		const double old_start_y = state->center_y - old_visible_height / 2.0;
		const double anchor_x = old_start_x + normalized_x * old_visible_width;
		const double anchor_y = old_start_y + normalized_y * old_visible_height;

		state->zoom_step = next_step;

		const double new_zoom_factor = MinimapViewport::GetZoomFactor(state->zoom_step);
		const double new_visible_width = std::max(1.0, safe_width * new_zoom_factor);
		const double new_visible_height = std::max(1.0, safe_height * new_zoom_factor);
		state->center_x = anchor_x + (0.5 - normalized_x) * new_visible_width;
		state->center_y = anchor_y + (0.5 - normalized_y) * new_visible_height;

		ClampViewportState(*state);
		owner_->SyncHeaderState();
		Refresh();
	}
}

void MinimapCanvas::OnMouseEnter(wxMouseEvent& event) {
	event.Skip();
}

void MinimapCanvas::OnMouseLeave(wxMouseEvent& event) {
	if (!HasCapture()) {
		StopInteraction();
	}
	event.Skip();
}

void MinimapCanvas::OnKey(wxKeyEvent& event) {
	if (MapTab* map_tab = g_gui.GetCurrentMapTab()) {
		map_tab->GetEventHandler()->AddPendingEvent(event);
	} else {
		event.Skip();
	}
}

void MinimapCanvas::ClampViewportState(MinimapViewportState& state) const {
	state.floor = MinimapViewport::ClampFloor(state.floor);
}

void MinimapCanvas::StopInteraction() {
	is_dragging_ = false;
	ctrl_click_pending_ = false;
	if (HasCapture()) {
		ReleaseMouse();
	}
}
