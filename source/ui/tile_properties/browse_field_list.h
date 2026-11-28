//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_BROWSE_FIELD_LIST_H_
#define RME_UI_BROWSE_FIELD_LIST_H_

#include <wx/wx.h>
#include <wx/panel.h>

class Tile;
class Map;
class Item;
class TilePropertiesListBox;

class BrowseFieldList : public wxPanel {
public:
	BrowseFieldList(wxWindow* parent);
	virtual ~BrowseFieldList();

	void SetTile(Tile* tile, Map* map);
	void SelectItem(Item* item);

	void SetOnItemSelectedCallback(std::function<void(Item*)> cb) {
		on_item_selected_cb = cb;
	}

	void OnItemSelected(wxCommandEvent& event);
	void OnClickUp(wxCommandEvent& event);
	void OnClickDown(wxCommandEvent& event);
	void OnClickDelete(wxCommandEvent& event);
	void OnContextMenu(wxContextMenuEvent& event);
	void UpdateButtons(int selection);

protected:
	// Reports the list row to the owner and makes it the editor selection (left click and context menu).
	void ApplyListSelection();

	TilePropertiesListBox* item_list;
	wxButton* btn_up;
	wxButton* btn_down;
	wxButton* btn_delete;

	Tile* current_tile;
	Map* current_map;

	std::function<void(Item*)> on_item_selected_cb;
};

#endif
