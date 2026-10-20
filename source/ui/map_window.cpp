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

#include "ui/map_window.h"
#include "ui/gui.h"
#include "game/sprites.h"
#include "editor/editor.h"
#include "rendering/core/map_view_math.h"
#include "ui/replace_tool/replace_tool_window.h"
#include "ui/dialogs/spawn_validator_window.h"

void ReplaceToolWindowDeleter::operator()(ReplaceToolWindow* w) {
	if (w) {
		w->Destroy();
	}
}

void SpawnValidatorWindowDeleter::operator()(SpawnValidatorWindow* w) {
	if (w) {
		w->Destroy();
	}
}

MapWindow::MapWindow(wxWindow* parent, Editor& editor) :
	wxPanel(parent, PANE_MAIN),
	editor(editor),
	replaceItemsDialog(nullptr),
	spawnValidatorDialog(nullptr) {
	int GL_settings[3];
	GL_settings[0] = WX_GL_RGBA;
	GL_settings[1] = WX_GL_DOUBLEBUFFER;
	GL_settings[2] = 0;
	canvas = newd MapCanvas(this, editor, GL_settings);

	vScroll = newd MapScrollBar(this, MAP_WINDOW_VSCROLL, wxVERTICAL, canvas);
	hScroll = newd MapScrollBar(this, MAP_WINDOW_HSCROLL, wxHORIZONTAL, canvas);

	gem = newd DCButton(this, MAP_WINDOW_GEM, wxDefaultPosition, DC_BTN_NORMAL, RENDER_SIZE_16x16, EDITOR_SPRITE_SELECTION_GEM);
	gem->SetToolTip("Switch between Drawing and Selection mode (Space)");

	wxFlexGridSizer* topsizer = newd wxFlexGridSizer(2, 0, 0);

	topsizer->AddGrowableCol(0);
	topsizer->AddGrowableRow(0);

	topsizer->Add(canvas, wxSizerFlags(1).Expand());
	topsizer->Add(vScroll, wxSizerFlags(1).Expand());
	topsizer->Add(hScroll, wxSizerFlags(1).Expand());
	topsizer->Add(gem, wxSizerFlags(1));

	SetSizerAndFit(topsizer);

	Bind(wxEVT_SIZE, &MapWindow::OnSize, this);

	hScroll->Bind(wxEVT_SCROLL_LINEUP, &MapWindow::OnScrollLineUp, this);
	hScroll->Bind(wxEVT_SCROLL_LINEDOWN, &MapWindow::OnScrollLineDown, this);
	hScroll->Bind(wxEVT_SCROLL_PAGEUP, &MapWindow::OnScrollPageUp, this);
	hScroll->Bind(wxEVT_SCROLL_PAGEDOWN, &MapWindow::OnScrollPageDown, this);
	hScroll->Bind(wxEVT_SCROLL_THUMBTRACK, &MapWindow::OnScroll, this);
	hScroll->Bind(wxEVT_SCROLL_THUMBRELEASE, &MapWindow::OnScroll, this);
	hScroll->Bind(wxEVT_SCROLL_TOP, &MapWindow::OnScroll, this);
	hScroll->Bind(wxEVT_SCROLL_BOTTOM, &MapWindow::OnScroll, this);

	vScroll->Bind(wxEVT_SCROLL_LINEUP, &MapWindow::OnScrollLineUp, this);
	vScroll->Bind(wxEVT_SCROLL_LINEDOWN, &MapWindow::OnScrollLineDown, this);
	vScroll->Bind(wxEVT_SCROLL_PAGEUP, &MapWindow::OnScrollPageUp, this);
	vScroll->Bind(wxEVT_SCROLL_PAGEDOWN, &MapWindow::OnScrollPageDown, this);
	vScroll->Bind(wxEVT_SCROLL_THUMBTRACK, &MapWindow::OnScroll, this);
	vScroll->Bind(wxEVT_SCROLL_THUMBRELEASE, &MapWindow::OnScroll, this);
	vScroll->Bind(wxEVT_SCROLL_TOP, &MapWindow::OnScroll, this);
	vScroll->Bind(wxEVT_SCROLL_BOTTOM, &MapWindow::OnScroll, this);

	Bind(wxEVT_BUTTON, &MapWindow::OnGem, this, MAP_WINDOW_GEM);
}

MapWindow::~MapWindow() {
	DetachReplaceItemsDialog();
	DetachSpawnValidatorDialog();
}

void MapWindow::ShowReplaceItemsDialog(bool selectionOnly) {
	if (replaceItemsDialog) {
		return;
	}

	replaceItemsDialog.reset(new ReplaceToolWindow(this, &editor));
	replaceItemsDialog->Show();
}

void MapWindow::ShowAdvancedReplaceForSelection(const std::vector<uint16_t>& ids) {
	if (replaceItemsDialog) {
		replaceItemsDialog->InitializeWithIDs(ids);
		replaceItemsDialog->Raise();
		return;
	}

	replaceItemsDialog.reset(new ReplaceToolWindow(this, &editor));
	replaceItemsDialog->InitializeWithIDs(ids);
	replaceItemsDialog->Show();
}

void MapWindow::CloseReplaceItemsDialog() {
	if (replaceItemsDialog) {
		replaceItemsDialog->DetachEditor();
		replaceItemsDialog->Close();
	}
}

void MapWindow::DetachReplaceItemsDialog() {
	if (replaceItemsDialog) {
		replaceItemsDialog->DetachEditor();
	}
}

void MapWindow::ForgetReplaceItemsDialog() {
	if (replaceItemsDialog) {
		// The dialog closes itself and may refuse to, so the pointer is only
		// dropped - without destroying a window that is still open.
		replaceItemsDialog.release();
	}
}

void MapWindow::ShowSpawnValidatorDialog() {
	if (spawnValidatorDialog) {
		spawnValidatorDialog->Raise();
		spawnValidatorDialog->SetFocus();
		return;
	}

	spawnValidatorDialog.reset(new SpawnValidatorWindow(this, &editor));
	spawnValidatorDialog->Show();
}

void MapWindow::CloseSpawnValidatorDialog() {
	if (spawnValidatorDialog) {
		spawnValidatorDialog->DetachEditor();
		spawnValidatorDialog->Close();
	}
}

void MapWindow::DetachSpawnValidatorDialog() {
	if (spawnValidatorDialog) {
		spawnValidatorDialog->DetachEditor();
	}
}

void MapWindow::ForgetSpawnValidatorDialog() {
	if (spawnValidatorDialog) {
		spawnValidatorDialog.release();
	}
}

void MapWindow::SetSize(int x, int y, bool center) {
	if (x == 0 || y == 0) {
		return;
	}

	int windowSizeX;
	int windowSizeY;

	canvas->GetSize(&windowSizeX, &windowSizeY);

	hScroll->SetScrollbar(center ? (x - windowSizeX) / 2 : hScroll->GetThumbPosition(), windowSizeX / x, x, windowSizeX / x);
	vScroll->SetScrollbar(center ? (y - windowSizeY) / 2 : vScroll->GetThumbPosition(), windowSizeY / y, y, windowSizeX / y);
	// wxPanel::SetSize(x, y);
}

void MapWindow::UpdateScrollbars(int nx, int ny) {
	// nx and ny are size of this window
	hScroll->SetScrollbar(hScroll->GetThumbPosition(), nx / std::max(1, hScroll->GetRange()), std::max(1, hScroll->GetRange()), 96);
	vScroll->SetScrollbar(vScroll->GetThumbPosition(), ny / std::max(1, vScroll->GetRange()), std::max(1, vScroll->GetRange()), 96);
}

void MapWindow::UpdateDialogs(bool show) {
	if (replaceItemsDialog) {
		replaceItemsDialog->Show(show);
	}
	if (spawnValidatorDialog) {
		spawnValidatorDialog->Show(show);
	}
}

void MapWindow::GetViewStart(int* x, int* y) {
	*x = hScroll->GetThumbPosition();
	*y = vScroll->GetThumbPosition();
}

void MapWindow::GetViewSize(int* x, int* y) {
	canvas->GetSize(x, y);
	*x *= canvas->GetContentScaleFactor();
	*y *= canvas->GetContentScaleFactor();
}

void MapWindow::FitToMap() {
	SetSize(editor.map.getWidth() * TILE_SIZE, editor.map.getHeight() * TILE_SIZE, true);
}

Position MapWindow::GetScreenCenterPosition() {
	int logical_width = 0;
	int logical_height = 0;
	canvas->GetSize(&logical_width, &logical_height);

	int x = 0;
	int y = 0;
	canvas->ScreenToMap(logical_width / 2, logical_height / 2, &x, &y);
	return Position(x, y, canvas->GetFloor());
}

void MapWindow::EnsureMinimapViewportInitialized() {
	if (!minimap_viewport_state.initialized) {
		SyncTrackedMinimapViewportToCurrentView(true);
	}
}

void MapWindow::ResetMinimapViewportToCurrentView() {
	SyncTrackedMinimapViewportToCurrentView(true);
}

void MapWindow::ResumeMinimapTrackingToCurrentView(bool reset_zoom) {
	minimap_viewport_state.tracking_main_camera = true;
	SyncTrackedMinimapViewportToCurrentView(reset_zoom);
}

void MapWindow::SyncTrackedMinimapViewportToCurrentView(bool reset_zoom) {
	if (minimap_viewport_state.initialized && !minimap_viewport_state.tracking_main_camera && !reset_zoom) {
		return;
	}

	const Position center = GetScreenCenterPosition();
	minimap_viewport_state.center_x = center.x;
	minimap_viewport_state.center_y = center.y;
	if (!minimap_viewport_state.initialized || reset_zoom) {
		minimap_viewport_state.zoom_step = MinimapViewport::DefaultZoomStep;
	}
	minimap_viewport_state.floor = MinimapViewport::ClampFloor(center.z);
	minimap_viewport_state.tracking_main_camera = true;
	minimap_viewport_state.initialized = true;
}

void MapWindow::SetScreenCenterPosition(const Position& position) {
	int x = MainMapViewMath::WorldTileToScrollPixel(position.x, position.z);
	int y = MainMapViewMath::WorldTileToScrollPixel(position.y, position.z);

	const Position& center = GetScreenCenterPosition();
	if (previous_position != center) {
		previous_position.x = center.x;
		previous_position.y = center.y;
		previous_position.z = center.z;
	}

	Scroll(x, y, true);
	canvas->ChangeFloor(position.z);
	canvas->SyncCursorHoverState();
	canvas->RequestLocalRefresh(true);
}

void MapWindow::GoToPreviousCenterPosition() {
	SetScreenCenterPosition(previous_position);
}

void MapWindow::Scroll(int x, int y, bool center) {
	if (center) {
		int window_size_x = 0;
		int window_size_y = 0;
		GetViewSize(&window_size_x, &window_size_y);
		x -= int((window_size_x * g_gui.GetCurrentZoom()) / 2.0);
		y -= int((window_size_y * g_gui.GetCurrentZoom()) / 2.0);
	}

	hScroll->SetThumbPosition(x);
	vScroll->SetThumbPosition(y);
	SyncTrackedMinimapViewportToCurrentView();
	g_gui.UpdateMinimap();
}

void MapWindow::ScrollRelative(int x, int y, bool immediate_minimap_update) {
	hScroll->SetThumbPosition(hScroll->GetThumbPosition() + x);
	vScroll->SetThumbPosition(vScroll->GetThumbPosition() + y);
	SyncTrackedMinimapViewportToCurrentView();
	g_gui.UpdateMinimap(immediate_minimap_update);
}

void MapWindow::OnGem(wxCommandEvent& WXUNUSED(event)) {
	g_gui.SwitchMode();
}

void MapWindow::OnSize(wxSizeEvent& event) {
	UpdateScrollbars(event.GetSize().GetWidth(), event.GetSize().GetHeight());
	event.Skip();
}

void MapWindow::OnScroll(wxScrollEvent& event) {
	SyncTrackedMinimapViewportToCurrentView();
	g_gui.UpdateMinimap();
	canvas->RequestLocalRefresh();
}

void MapWindow::OnScrollLineDown(wxScrollEvent& event) {
	if (event.GetOrientation() == wxHORIZONTAL) {
		ScrollRelative(96, 0);
	} else {
		ScrollRelative(0, 96);
	}
	canvas->RequestLocalRefresh();
}

void MapWindow::OnScrollLineUp(wxScrollEvent& event) {
	if (event.GetOrientation() == wxHORIZONTAL) {
		ScrollRelative(-96, 0);
	} else {
		ScrollRelative(0, -96);
	}
	canvas->RequestLocalRefresh();
}

void MapWindow::OnScrollPageDown(wxScrollEvent& event) {
	if (event.GetOrientation() == wxHORIZONTAL) {
		ScrollRelative(5 * 96, 0);
	} else {
		ScrollRelative(0, 5 * 96);
	}
	canvas->RequestLocalRefresh();
}

void MapWindow::OnScrollPageUp(wxScrollEvent& event) {
	if (event.GetOrientation() == wxHORIZONTAL) {
		ScrollRelative(-5 * 96, 0);
	} else {
		ScrollRelative(0, -5 * 96);
	}
	canvas->RequestLocalRefresh();
}

