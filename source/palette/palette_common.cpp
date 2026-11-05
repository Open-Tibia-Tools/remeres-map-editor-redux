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

#include "palette/palette_common.h"
#include "brushes/brush.h"
#include "brushes/flag/flag_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/border/optional_border_brush.h"
#include "brushes/managers/brush_manager.h"
#include "game/sprites.h"
#include "ui/gui.h"
#include "ui/controls/item_buttons.h"
#include "app/application.h"
#include "palette/palette_waypoints.h"
#include "palette/managers/palette_manager.h" // Added for g_palettes

// ============================================================================
// Palette Panel

PalettePanel::PalettePanel(wxWindow* parent, wxWindowID id, long style) :
	wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, style),
	refresh_timer(this, PALETTE_DELAYED_REFRESH_TIMER) {
	refresh_timer.Bind(wxEVT_TIMER, &PalettePanel::OnRefreshTimer, this);
	////
}

PalettePanel::~PalettePanel() {
	////
}

PaletteWindow* PalettePanel::GetParentPalette() const {
	const wxWindow* w = this;
	while (w) {
		const PaletteWindow* pw = dynamic_cast<const PaletteWindow*>(w);
		if (pw) {
			return const_cast<PaletteWindow*>(pw);
		}
		w = w->GetParent();
	}
	return nullptr;
}

void PalettePanel::InvalidateContents() {
	for (auto* toolbar : tool_bars) {
		toolbar->InvalidateContents();
	}
}

void PalettePanel::LoadCurrentContents() {
	for (auto* toolbar : tool_bars) {
		toolbar->OnSwitchIn();
	}
	Fit();
}

void PalettePanel::LoadAllContents() {
	for (auto* toolbar : tool_bars) {
		toolbar->LoadAllContents();
	}
}

void PalettePanel::AddToolPanel(PalettePanel* panel) {
	wxSizer* sp_sizer = new wxStaticBoxSizer(wxVERTICAL, this, panel->GetName());
	sp_sizer->Add(panel, wxSizerFlags().Expand());
	GetSizer()->Add(sp_sizer, wxSizerFlags().Expand());

	// GetSizer()->SetDimension(wxDefaultCoord, wxDefaultCoord, wxDefaultCoord, wxDefaultCoord);
	// GetSizer()->Layout();
	Fit();
	//

	tool_bars.push_back(panel);
}

void PalettePanel::SetToolbarIconSize(bool large_icons) {
	for (auto* toolbar : tool_bars) {
		toolbar->SetToolbarIconSize(large_icons);
	}
}

wxString PalettePanel::GetName() const {
	return "Unknown";
}

Brush* PalettePanel::GetSelectedBrush() const {
	return nullptr;
}

int PalettePanel::GetSelectedBrushSize() const {
	return 0;
}

void PalettePanel::SelectFirstBrush() {
	// Do nothing
}

bool PalettePanel::SelectBrush(const Brush*) {
	return false;
}

void PalettePanel::OnUpdateBrushSize(BrushShape shape, int size) {
	for (auto* toolbar : tool_bars) {
		toolbar->OnUpdateBrushSize(shape, size);
	}
}

void PalettePanel::OnSwitchIn() {
	g_palettes.ActivatePalette(GetParentPalette());
	g_gui.RestoreBrushSizeState(last_brush_size_state);
	for (auto* toolbar : tool_bars) {
		toolbar->OnSwitchIn();
	}
}

void PalettePanel::OnSwitchOut() {
	last_brush_size_state = g_brush_manager.GetBrushSizeState();
	for (auto* toolbar : tool_bars) {
		toolbar->OnSwitchOut();
	}
}

void PalettePanel::OnUpdate() {
	for (auto* toolbar : tool_bars) {
		toolbar->OnUpdate();
	}
}

void PalettePanel::RefreshOtherPalettes() {
	refresh_timer.Start(100, true);
}

void PalettePanel::OnRefreshTimer(wxTimerEvent&) {
	g_palettes.RefreshOtherPalettes(GetParentPalette());
}
