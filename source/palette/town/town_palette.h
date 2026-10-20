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

#ifndef RME_PALETTE_TOWNS_H_
#define RME_PALETTE_TOWNS_H_

#include "util/nanovg_table_ctrl.h"
#include "palette/controls/palette_search_ctrl.h"
#include "game/town.h"
#include "palette/palette_common.h"

class TownPalettePanel : public PalettePanel {
public:
	TownPalettePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	~TownPalettePanel() override = default;

	wxString GetName() const override;

	void SelectFirstBrush() override;
	Brush* GetSelectedBrush() const override;
	int GetSelectedBrushSize() const override;
	bool SelectBrush(const Brush* whatbrush) override;

	void OnUpdate() override;
	void OnSwitchIn() override;
	void OnSwitchOut() override;

	// Table callbacks
	void OnSelectTown(int index, const TableRow& row);
	void OnActivateTown(int index, const TableRow& row);
	void OnSortTowns(int column, bool ascending);
	void OnTownContextMenu(int index, const TableRow* row, const wxPoint& pos);

	// Event handlers
	void OnSearchChange(wxCommandEvent& event);
	void OnClickAddTown(wxCommandEvent& event);
	void OnClickEditTown(wxCommandEvent& event);
	void OnClickRemoveTown(wxCommandEvent& event);
	void OnClickChangeTemple(wxCommandEvent& event);
	void OnGoToTemple(wxCommandEvent& event);

	void SetMap(Map* map);

protected:
	bool RunEditTownModal(wxWindow* parent, Map* map, Town* town, bool is_new);
	Town* GetSelectedTown() const;

	Map* map = nullptr;
	uint32_t selected_town_id_ = 0;

	PaletteSearchCtrl* search_ctrl = nullptr;
	NanoVGTableCtrl* town_table = nullptr;
	wxButton* add_town_button = nullptr;
	wxButton* edit_town_button = nullptr;
	wxButton* remove_town_button = nullptr;
	wxButton* change_temple_button = nullptr;
};

#endif
