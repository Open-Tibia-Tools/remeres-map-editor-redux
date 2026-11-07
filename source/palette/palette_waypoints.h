//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_WAYPOINTS_H_
#define RME_PALETTE_WAYPOINTS_H_

#include "palette/palette_common.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "game/waypoints.h"

#include <string>
#include <vector>

class Map;
class Waypoint;
class Brush;

class WaypointPalettePanel : public PalettePanel {
public:
	explicit WaypointPalettePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	~WaypointPalettePanel() override;

	wxString GetName() const override;

	void SelectFirstBrush() override;
	Brush* GetSelectedBrush() const override;
	int GetSelectedBrushSize() const override;
	bool SelectBrush(const Brush* whatbrush) override;

	void OnUpdate() override;
	void OnSwitchIn() override;
	void OnSwitchOut() override;

	void SetMap(Map* map);
	void SelectWaypointByName(const std::string& name);

	// Context menu & action operations
	void OnClickAddWaypoint();
	void OnClickRenameWaypoint();
	void OnClickRemoveWaypoint();
	void OnGoToWaypoint();

protected:
	Waypoint* GetSelectedWaypoint() const;
	void RemoveWaypointFromMap(Waypoint* wp);

private:
	class WaypointCanvas;

	Map* map_ = nullptr;
	std::string selected_waypoint_name_;

	TilesetGridToolbar* toolbar_ = nullptr;
	WaypointCanvas* canvas_ = nullptr;
	wxButton* add_button_ = nullptr;
	wxButton* rename_button_ = nullptr;
	wxButton* remove_button_ = nullptr;
};

#endif // RME_PALETTE_WAYPOINTS_H_
