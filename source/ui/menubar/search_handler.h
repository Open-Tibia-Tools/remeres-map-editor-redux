#ifndef RME_UI_MENUBAR_SEARCH_HANDLER_H_
#define RME_UI_MENUBAR_SEARCH_HANDLER_H_

#include <wx/wx.h>
#include "editor/operations/search_query.h"

#include <vector>

class MainFrame;
class Map;
class Brush;
class FindItemDialog;

namespace SearchResults {
	void ShowItems(Map* map, uint16_t item_id, const wxString& load_bar_label, bool on_selection = false);
	void ShowCreatures(Map* map, const Brush* creature_brush, const wxString& load_bar_label);
	void ShowFiltered(Map* map, const std::vector<uint16_t>& item_ids, const std::vector<const Brush*>& creature_brushes, const wxString& load_bar_label, bool on_selection = false);
	void SelectFoundBrush(const FindItemDialog& dialog);
}

class SearchHandler {
public:
	explicit SearchHandler(MainFrame* frame);

	void OnSearchForItem(wxCommandEvent& event);
	void OnReplaceItems(wxCommandEvent& event);

	// Map searches
	void OnSearchForStuffOnMap(wxCommandEvent& event);
	void OnSearchForUniqueOnMap(wxCommandEvent& event);
	void OnSearchForActionOnMap(wxCommandEvent& event);
	void OnSearchForContainerOnMap(wxCommandEvent& event);
	void OnSearchForWriteableOnMap(wxCommandEvent& event);
	void OnSearchForTeleportOnMap(wxCommandEvent& event);
	void OnSearchForCreatureOnMap(wxCommandEvent& event);
	void OnSearchForPickupableOnMap(wxCommandEvent& event);

	// Selection searches
	void OnSearchForStuffOnSelection(wxCommandEvent& event);
	void OnSearchForUniqueOnSelection(wxCommandEvent& event);
	void OnSearchForActionOnSelection(wxCommandEvent& event);
	void OnSearchForContainerOnSelection(wxCommandEvent& event);
	void OnSearchForWriteableOnSelection(wxCommandEvent& event);
	void OnSearchForTeleportOnSelection(wxCommandEvent& event);
	void OnSearchForCreatureOnSelection(wxCommandEvent& event);
	void OnSearchForPickupableOnSelection(wxCommandEvent& event);

	void OnSearchForItemOnSelection(wxCommandEvent& event);
	void OnReplaceItemsOnSelection(wxCommandEvent& event);
	void OnRemoveItemOnSelection(wxCommandEvent& event);

private:
	void DispatchSearch(SearchCategory categories, bool onSelection, const std::string& title);

	MainFrame* frame;
};

#endif // RME_UI_MENUBAR_SEARCH_HANDLER_H_
