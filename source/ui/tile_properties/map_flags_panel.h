//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_MAP_FLAGS_PANEL_H_
#define RME_UI_MAP_FLAGS_PANEL_H_

#include <wx/wx.h>
#include <wx/panel.h>

class Tile;
class Map;

class MapFlagsPanel : public wxPanel {
public:
	MapFlagsPanel(wxWindow* parent);
	virtual ~MapFlagsPanel();

	void SetTile(Tile* tile, Map* map);

protected:
	void OnToggleFlag(wxCommandEvent& event);

	wxCheckBox* chk_pz;
	wxCheckBox* chk_nopvp;
	wxCheckBox* chk_nologout;
	wxCheckBox* chk_pvpzone;
	wxCheckBox* chk_refresh;

	Tile* current_tile;
	Map* current_map;
};

#endif
