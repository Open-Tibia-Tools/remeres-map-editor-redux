#include "ui/menubar/search_handler.h"
#include "app/definitions.h"

#include "ui/gui.h"
#include "ui/dialog_util.h"
#include "ui/find_item_window.h"
#include "ui/result_window.h"
#include "ui/map_window.h"
#include "ui/map_tab.h"
#include "app/preferences.h"
#include "editor/editor.h"
#include "editor/operations/search_query.h"
#include "editor/operations/clean_operations.h"
#include "map/map.h"
#include "editor/action_queue.h"
#include "game/items.h"

#include <algorithm>
#include <format>

void SearchResults::ExecuteAndShow(Map* map, SearchQuery query) {
	if (map == nullptr || !g_gui.IsEditorOpen()) {
		return;
	}

	auto results = SearchEngine::Execute(*map, query);
	g_gui.ShowSearchWindow()->SetSearchResults(std::move(results), std::move(query));
}

void SearchResults::SelectFoundBrush(const FindItemDialog& dialog) {
	if (const Brush* brush = dialog.getResult()) {
		g_gui.SelectBrush(brush);
	}
}

void SearchResults::ShowItems(Map* map, uint16_t item_id, const wxString& WXUNUSED(load_bar_label), bool on_selection) {
	SearchQuery query;
	query.specific_item_id = item_id;
	query.on_selection = on_selection;
	const auto def = g_item_definitions.get(item_id);
	const std::string item_name = def ? std::string(def.name()) : std::format("Item #{}", item_id);
	query.query_title = std::format("Search: {}", item_name);

	ExecuteAndShow(map, std::move(query));
}

void SearchResults::ShowCreatures(Map* map, const Brush* creature_brush, const wxString& WXUNUSED(load_bar_label)) {
	SearchQuery query;
	query.specific_creature_brush = creature_brush;
	query.query_title = creature_brush ? std::format("Search: {}", creature_brush->getName()) : "Creature Search";

	ExecuteAndShow(map, std::move(query));
}

void SearchResults::ShowFiltered(Map* map, const std::vector<uint16_t>& item_ids, const std::vector<const Brush*>& creature_brushes, const wxString& WXUNUSED(load_bar_label), bool on_selection) {
	SearchQuery query;
	query.item_ids = item_ids;
	query.item_id_set.insert(item_ids.begin(), item_ids.end());
	query.creature_brushes = creature_brushes;
	query.on_selection = on_selection;
	query.query_title = std::format("Filtered Search ({} items)", item_ids.size() + creature_brushes.size());

	ExecuteAndShow(map, std::move(query));
}

SearchHandler::SearchHandler(MainFrame* frame) :
	frame(frame) {
}

void SearchHandler::OnSearchForItem(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	FindItemDialog dialog(frame, "Search for Item", false, FindItemDialog::ActionSet::SearchAndSelect, AdvancedFinderDefaultAction::SearchMap, true);
	const int modal_result = dialog.ShowModal();
	if (modal_result != wxID_CANCEL) {
		Map* current_map = &g_gui.GetCurrentMap();
		if (dialog.getResultAction() == FindItemDialog::ResultAction::SearchMap) {
			if (dialog.getResultKind() == AdvancedFinderCatalogKind::Creature) {
				SearchResults::ShowCreatures(current_map, dialog.getResult(), "Searching map...");
			} else {
				SearchResults::ShowItems(current_map, dialog.getResultID(), "Searching map...");
			}
		} else if (dialog.getResultAction() == FindItemDialog::ResultAction::SearchFiltered) {
			SearchResults::ShowFiltered(current_map, dialog.getFilteredItemIDs(), dialog.getFilteredCreatureBrushes(), "Searching map...");
		} else if (dialog.getResultAction() == FindItemDialog::ResultAction::SelectItem) {
			SearchResults::SelectFoundBrush(dialog);
		}
	}
}

void SearchHandler::OnReplaceItems(wxCommandEvent& WXUNUSED(event)) {
	if (!g_version.IsVersionLoaded()) {
		return;
	}

	if (MapTab* tab = g_gui.GetCurrentMapTab()) {
		if (MapWindow* window = tab->GetView()) {
			window->ShowReplaceItemsDialog(false);
		}
	}
}

void SearchHandler::DispatchSearch(SearchCategory categories, bool onSelection, const std::string& title) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	SearchQuery query;
	query.categories = categories;
	query.on_selection = onSelection;
	query.query_title = title;

	SearchResults::ExecuteAndShow(&g_gui.GetCurrentMap(), std::move(query));
}

// Map searches
void SearchHandler::OnSearchForStuffOnMap(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Everything, false, "Find Everything (Map)");
}

void SearchHandler::OnSearchForUniqueOnMap(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::UniqueID, false, "Find Unique ID (Map)");
}

void SearchHandler::OnSearchForActionOnMap(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::ActionID, false, "Find Action ID (Map)");
}

void SearchHandler::OnSearchForContainerOnMap(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Container, false, "Find Containers (Map)");
}

void SearchHandler::OnSearchForWriteableOnMap(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Writeable, false, "Find Writeables (Map)");
}

void SearchHandler::OnSearchForTeleportOnMap(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Teleport, false, "Find Teleports (Map)");
}

void SearchHandler::OnSearchForCreatureOnMap(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Creature, false, "Find Creatures (Map)");
}

void SearchHandler::OnSearchForPickupableOnMap(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Pickupable, false, "Find Pickupable (Map)");
}

// Selection searches
void SearchHandler::OnSearchForStuffOnSelection(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Everything, true, "Find Everything (Selection)");
}

void SearchHandler::OnSearchForUniqueOnSelection(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::UniqueID, true, "Find Unique ID (Selection)");
}

void SearchHandler::OnSearchForActionOnSelection(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::ActionID, true, "Find Action ID (Selection)");
}

void SearchHandler::OnSearchForContainerOnSelection(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Container, true, "Find Containers (Selection)");
}

void SearchHandler::OnSearchForWriteableOnSelection(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Writeable, true, "Find Writeables (Selection)");
}

void SearchHandler::OnSearchForTeleportOnSelection(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Teleport, true, "Find Teleports (Selection)");
}

void SearchHandler::OnSearchForCreatureOnSelection(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Creature, true, "Find Creatures (Selection)");
}

void SearchHandler::OnSearchForPickupableOnSelection(wxCommandEvent& WXUNUSED(event)) {
	DispatchSearch(SearchCategory::Pickupable, true, "Find Pickupable (Selection)");
}

void SearchHandler::OnSearchForItemOnSelection(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	FindItemDialog dialog(frame, "Search on Selection", false, FindItemDialog::ActionSet::ConfirmOnly);
	if (dialog.ShowModal() == wxID_OK) {
		SearchResults::ShowItems(&g_gui.GetCurrentMap(), dialog.getResultID(), "Searching on selected area...", true);
	}
}

void SearchHandler::OnReplaceItemsOnSelection(wxCommandEvent& WXUNUSED(event)) {
	if (!g_version.IsVersionLoaded()) {
		return;
	}

	if (MapTab* tab = g_gui.GetCurrentMapTab()) {
		if (MapWindow* window = tab->GetView()) {
			window->ShowReplaceItemsDialog(true);
		}
	}
}

void SearchHandler::OnRemoveItemOnSelection(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	FindItemDialog dialog(frame, "Remove Item on Selection");
	if (dialog.ShowModal() == wxID_OK) {
		g_gui.GetCurrentEditor()->actionQueue->clear();
		g_gui.CreateLoadBar("Searching item on selection to remove...");
		EditorOperations::RemoveItemCondition condition(dialog.getResultID());
		int64_t count = RemoveItemOnMap(g_gui.GetCurrentMap(), condition, true);
		g_gui.DestroyLoadBar();

		wxString msg;
		msg << count << " items removed.";
		DialogUtil::PopupDialog("Remove Item", msg, wxOK);
		g_gui.GetCurrentMap().doChange();
		g_gui.RefreshView();
	}
}
