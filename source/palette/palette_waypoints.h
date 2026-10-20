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

#ifndef RME_PALETTE_WAYPOINTS_H_
#define RME_PALETTE_WAYPOINTS_H_

#include "util/nanovg_table_ctrl.h"
#include "palette/controls/palette_search_ctrl.h"
#include "game/waypoints.h"
#include "palette/palette_common.h"

#include <string>

class WaypointPalettePanel : public PalettePanel {
public:
	WaypointPalettePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	~WaypointPalettePanel() override = default;

	wxString GetName() const override;

	// Select the first brush
	void SelectFirstBrush() override;
	// Returns the currently selected brush (first brush if panel is not loaded)
	Brush* GetSelectedBrush() const override;
	// Returns the currently selected brush size
	int GetSelectedBrushSize() const override;
	// Select the brush in the parameter, this only changes the look of the panel
	bool SelectBrush(const Brush* whatbrush) override;

	void OnUpdate() override;
	void OnSwitchIn() override;
	void OnSwitchOut() override;

	// Table callbacks
	void OnSelectWaypoint(int index, const TableRow& row);
	void OnActivateWaypoint(int index, const TableRow& row);
	void OnSortWaypoints(int column, bool ascending);
	void OnWaypointContextMenu(int index, const TableRow* row, const wxPoint& pos);

	// Event handlers
	void OnSearchChange(wxCommandEvent& event);
	void OnClickAddWaypoint(wxCommandEvent& event);
	void OnClickRenameWaypoint(wxCommandEvent& event);
	void OnClickRemoveWaypoint(wxCommandEvent& event);
	void OnGoToWaypoint(wxCommandEvent& event);

	void SetMap(Map* map);

protected:
	void RemoveWaypointFromMap(Waypoint* wp);
	Waypoint* GetSelectedWaypoint() const;

	Map* map = nullptr;
	std::string selected_waypoint_name_;

	PaletteSearchCtrl* search_ctrl = nullptr;
	NanoVGTableCtrl* waypoint_table = nullptr;
	wxButton* add_waypoint_button = nullptr;
	wxButton* rename_waypoint_button = nullptr;
	wxButton* remove_waypoint_button = nullptr;
};

#endif

