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
#include "ui/positionctrl.h"
#include "ui/numbertextctrl.h"
#include "map/position.h"
#include "ui/position_paste.h"

#include <algorithm>

PositionCtrl::PositionCtrl(wxWindow* parent, const wxString& label, int x, int y, int z, int map_width /*= MAP_MAX_WIDTH*/, int map_height /*= MAP_MAX_HEIGHT*/, int maxz /*= MAP_MAX_LAYER*/) :
	wxStaticBoxSizer(wxHORIZONTAL, parent, label) {
	const wxSize xySize = parent ? parent->FromDIP(wxSize(60, 20)) : wxSize(60, 20);
	const wxSize zSize = parent ? parent->FromDIP(wxSize(35, 20)) : wxSize(35, 20);
	x_field = new NumberTextCtrl(GetStaticBox(), wxID_ANY, x, 0, std::max(map_width - 1, 0), wxTE_PROCESS_ENTER, "X", wxDefaultPosition, xySize);
	x_field->SetToolTip("X Coordinate");
	Add(x_field, wxSizerFlags(2).Expand().Border(wxLEFT | wxBOTTOM, 5));

	y_field = new NumberTextCtrl(GetStaticBox(), wxID_ANY, y, 0, std::max(map_height - 1, 0), wxTE_PROCESS_ENTER, "Y", wxDefaultPosition, xySize);
	y_field->SetToolTip("Y Coordinate");
	Add(y_field, wxSizerFlags(2).Expand().Border(wxLEFT | wxBOTTOM, 5));

	z_field = new NumberTextCtrl(GetStaticBox(), wxID_ANY, z, 0, maxz, wxTE_PROCESS_ENTER, "Z", wxDefaultPosition, zSize);
	z_field->SetToolTip("Z Coordinate (Layer)");
	Add(z_field, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 5));

	BindPositionPaste(x_field, y_field, z_field, [map_width, map_height] {
		return PositionBounds { map_width, map_height };
	});
}

PositionCtrl::~PositionCtrl() {
	////
}

Position PositionCtrl::GetPosition() const {
	Position pos;
	pos.x = x_field->GetIntValue();
	pos.y = y_field->GetIntValue();
	pos.z = z_field->GetIntValue();
	return pos;
}

void PositionCtrl::SetPosition(Position pos) {
	x_field->SetIntValue(pos.x);
	y_field->SetIntValue(pos.y);
	z_field->SetIntValue(pos.z);
}

void PositionCtrl::CheckRange()
{
    x_field->CheckRange();
    y_field->CheckRange();
    z_field->CheckRange();
}

bool PositionCtrl::Enable(bool enable) {
	return (x_field->Enable(enable) && y_field->Enable(enable) && z_field->Enable(enable));
}
