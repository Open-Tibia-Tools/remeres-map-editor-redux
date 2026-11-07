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

#ifndef RME_EDIT_TOWN_DIALOG_H_
#define RME_EDIT_TOWN_DIALOG_H_

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>

#include "map/position.h"

class Map;
class Town;
class PositionCtrl;

class EditTownDialog : public wxDialog {
public:
	enum CustomReturnCode {
		DIALOG_PICK_TEMPLE = wxID_HIGHEST + 101
	};

	EditTownDialog(wxWindow* parent, Map* map, Town* town, bool is_new = false);
	~EditTownDialog() override;

	void OnClickPickFromMap(wxCommandEvent& event);
	void OnClickOK(wxCommandEvent& event);
	void OnClickCancel(wxCommandEvent& event);

	std::string GetTownName() const;
	uint32_t GetTownID() const;
	Position GetTemplePosition() const;

	void SetTemplePosition(const Position& pos);

protected:
	Map* map = nullptr;
	Town* town = nullptr;
	bool is_new_ = false;

	wxString town_name;
	wxTextCtrl* name_field = nullptr;
	wxSpinCtrl* id_field = nullptr;
	PositionCtrl* temple_position = nullptr;
	wxButton* pick_map_button = nullptr;
};

// Custom styled wxWidgets confirmation dialogs (matching RME theme & English labels)
class TownIdWarningDialog : public wxDialog {
public:
	TownIdWarningDialog(wxWindow* parent, uint32_t old_id, uint32_t new_id, size_t house_count);
};

class TownDeleteDialog : public wxDialog {
public:
	TownDeleteDialog(wxWindow* parent, const std::string& town_name, uint32_t town_id, size_t house_count);
};

#endif
