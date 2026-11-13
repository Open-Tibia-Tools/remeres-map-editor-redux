//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_BROWSE_FIELD_LIST_H_
#define RME_UI_BROWSE_FIELD_LIST_H_

#include <wx/wx.h>
#include <wx/panel.h>

#include <functional>

class Tile;
class Map;
class Item;
class TilePropertiesItemGrid;
class TilesetGridToolbar;

class BrowseFieldList : public wxPanel {
public:
	BrowseFieldList(wxWindow* parent);
	virtual ~BrowseFieldList();

	void SetTile(Tile* tile, Map* map);
	void SelectItem(Item* item);

	void SetOnItemSelectedCallback(std::function<void(Item*)> cb) {
		on_item_selected_cb = cb;
	}

	void OnItemSelected(Item* item);
	void OnClickUp(wxCommandEvent& event);
	void OnClickDown(wxCommandEvent& event);
	void OnClickDelete(wxCommandEvent& event);

protected:
	void UpdateButtons();

	TilePropertiesItemGrid* item_grid;
	TilesetGridToolbar* grid_toolbar;
	wxButton* btn_up;
	wxButton* btn_down;
	wxButton* btn_delete;

	Tile* current_tile;
	Map* current_map;

	std::function<void(Item*)> on_item_selected_cb;
};

#endif
