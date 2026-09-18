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
#include "editor/operations/search_operations.h"
#include "editor/operations/clean_operations.h"
#include "map/map.h"
#include "editor/action_queue.h"

#include <algorithm>

namespace {
	[[nodiscard]] uint32_t searchResultsLimit() {
		return static_cast<uint32_t>(std::clamp(g_settings.getInteger(Config::SEARCH_RESULTS_LIMIT), 1, 100000));
	}

	[[nodiscard]] SearchResultRow makeSearchResultRow(uint32_t index, const wxString& name, const Position& position) {
		return SearchResultRow {
			.index = index,
			.name = name,
			.position = position,
		};
	}

	[[nodiscard]] Map* resolvePagedSearchMap(Map* map) {
		if (map == nullptr) {
			return nullptr;
		}

		for (int index = 0; index < g_gui.GetTabCount(); ++index) {
			if (auto* tab = dynamic_cast<MapTab*>(g_gui.GetTab(index)); tab != nullptr && tab->GetMap() == map) {
				return tab->GetMap();
			}
		}

		return nullptr;
	}

	void selectFoundBrush(const FindItemDialog& dialog) {
		if (const Brush* brush = dialog.getResult()) {
			g_gui.SelectBrush(brush);
		}
	}

	void showItemSearchResults(Map* map, uint16_t item_id, const wxString& load_bar_label, uint32_t page_offset = 0) {
		Map* search_map = resolvePagedSearchMap(map);
		if (search_map == nullptr) {
			return;
		}

		const uint32_t page_limit = searchResultsLimit();
		EditorOperations::ItemSearcher finder(item_id, page_limit, page_offset, [](uint32_t percent) { g_gui.SetLoadDone(percent); });
		g_gui.CreateLoadBar(load_bar_label);
		foreach_ItemOnMap(*search_map, finder, false);
		g_gui.DestroyLoadBar();

		SearchResultWindow* window = g_gui.ShowSearchWindow();
		std::vector<SearchResultRow> rows;
		rows.reserve(finder.result.size());
		for (size_t index = 0; index < finder.result.size(); ++index) {
			const auto& [tile, item] = finder.result[index];
			rows.push_back(makeSearchResultRow(page_offset + static_cast<uint32_t>(index) + 1, wxstr(item->getName()), tile->getPosition()));
		}
		window->SetResults(
			std::move(rows),
			finder.totalMatches,
			page_offset,
			page_limit,
			[map, item_id, load_bar_label](uint32_t next_offset) {
				showItemSearchResults(map, item_id, load_bar_label, next_offset);
			}
		);
	}

	void showCreatureSearchResults(Map* map, const Brush* creature_brush, const wxString& load_bar_label, uint32_t page_offset = 0) {
		Map* search_map = resolvePagedSearchMap(map);
		if (search_map == nullptr) {
			return;
		}

		const uint32_t page_limit = searchResultsLimit();
		EditorOperations::CreatureSearcher finder(creature_brush, page_limit, page_offset, [](uint32_t percent) { g_gui.SetLoadDone(percent); });
		g_gui.CreateLoadBar(load_bar_label);
		foreach_TileOnMap(*search_map, finder);
		g_gui.DestroyLoadBar();

		SearchResultWindow* window = g_gui.ShowSearchWindow();
		std::vector<SearchResultRow> rows;
		rows.reserve(finder.result.size());
		for (size_t index = 0; index < finder.result.size(); ++index) {
			const auto& [tile, creature] = finder.result[index];
			rows.push_back(makeSearchResultRow(page_offset + static_cast<uint32_t>(index) + 1, wxstr(creature->getName()), tile->getPosition()));
		}
		window->SetResults(
			std::move(rows),
			finder.totalMatches,
			page_offset,
			page_limit,
			[map, creature_brush, load_bar_label](uint32_t next_offset) {
				showCreatureSearchResults(map, creature_brush, load_bar_label, next_offset);
			}
		);
	}

	void showItemSearchResultsOnSelection(Map* map, uint16_t item_id, const wxString& load_bar_label, uint32_t page_offset = 0) {
		Map* search_map = resolvePagedSearchMap(map);
		if (search_map == nullptr) {
			return;
		}

		const uint32_t page_limit = searchResultsLimit();
		EditorOperations::ItemSearcher finder(item_id, page_limit, page_offset, [](uint32_t percent) { g_gui.SetLoadDone(percent); });
		g_gui.CreateLoadBar(load_bar_label);
		foreach_ItemOnMap(*search_map, finder, true);
		g_gui.DestroyLoadBar();

		SearchResultWindow* window = g_gui.ShowSearchWindow();
		std::vector<SearchResultRow> rows;
		rows.reserve(finder.result.size());
		for (size_t index = 0; index < finder.result.size(); ++index) {
			const auto& [tile, item] = finder.result[index];
			rows.push_back(makeSearchResultRow(page_offset + static_cast<uint32_t>(index) + 1, wxstr(item->getName()), tile->getPosition()));
		}
		window->SetResults(
			std::move(rows),
			finder.totalMatches,
			page_offset,
			page_limit,
			[map, item_id, load_bar_label](uint32_t next_offset) {
				showItemSearchResultsOnSelection(map, item_id, load_bar_label, next_offset);
			}
		);
	}
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
				showCreatureSearchResults(current_map, dialog.getResult(), "Searching map...");
			} else {
				showItemSearchResults(current_map, dialog.getResultID(), "Searching map...");
			}
		} else if (dialog.getResultAction() == FindItemDialog::ResultAction::SelectItem) {
			selectFoundBrush(dialog);
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

void SearchHandler::OnSearchForStuffOnMap(wxCommandEvent& WXUNUSED(event)) {
	SearchItems(true, true, true, true);
}

void SearchHandler::OnSearchForUniqueOnMap(wxCommandEvent& WXUNUSED(event)) {
	SearchItems(true, false, false, false);
}

void SearchHandler::OnSearchForActionOnMap(wxCommandEvent& WXUNUSED(event)) {
	SearchItems(false, true, false, false);
}

void SearchHandler::OnSearchForContainerOnMap(wxCommandEvent& WXUNUSED(event)) {
	SearchItems(false, false, true, false);
}

void SearchHandler::OnSearchForWriteableOnMap(wxCommandEvent& WXUNUSED(event)) {
	SearchItems(false, false, false, true);
}

void SearchHandler::OnSearchForStuffOnSelection(wxCommandEvent& WXUNUSED(event)) {
	SearchItems(true, true, true, true, true);
}

void SearchHandler::OnSearchForUniqueOnSelection(wxCommandEvent& WXUNUSED(event)) {
	SearchItems(true, false, false, false, true);
}

void SearchHandler::OnSearchForActionOnSelection(wxCommandEvent& WXUNUSED(event)) {
	SearchItems(false, true, false, false, true);
}

void SearchHandler::OnSearchForContainerOnSelection(wxCommandEvent& WXUNUSED(event)) {
	SearchItems(false, false, true, false, true);
}

void SearchHandler::OnSearchForWriteableOnSelection(wxCommandEvent& WXUNUSED(event)) {
	SearchItems(false, false, false, true, true);
}

void SearchHandler::OnSearchForItemOnSelection(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	FindItemDialog dialog(frame, "Search on Selection", false, FindItemDialog::ActionSet::ConfirmOnly);
	if (dialog.ShowModal() == wxID_OK) {
		showItemSearchResultsOnSelection(&g_gui.GetCurrentMap(), dialog.getResultID(), "Searching on selected area...");
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
		EditorOperations::RemoveItemCondition condition(dialog.getResultID(), [](uint32_t percent) { g_gui.SetLoadDone(percent); });
		int64_t count = RemoveItemOnMap(g_gui.GetCurrentMap(), condition, true);
		g_gui.DestroyLoadBar();

		wxString msg;
		msg << count << " items removed.";
		DialogUtil::PopupDialog("Remove Item", msg, wxOK);
		g_gui.GetCurrentMap().doChange();
		g_gui.RefreshView();
	}
}

struct SearchResult {
	Tile* tile;
	Item* item;
	std::string description;
};

void SearchHandler::SearchItems(bool unique, bool action, bool container, bool writable, bool onSelection /* = false*/) {
	if (!unique && !action && !container && !writable) {
		return;
	}

	if (!g_gui.IsEditorOpen()) {
		return;
	}

	if (onSelection) {
		g_gui.CreateLoadBar("Searching on selected area...");
	} else {
		g_gui.CreateLoadBar("Searching on map...");
	}

	// Use the MapSearcher from EditorOperations
	EditorOperations::MapSearcher finder([](uint32_t percent) { g_gui.SetLoadDone(percent); });
	finder.search_unique = unique;
	finder.search_action = action;
	finder.search_container = container;
	finder.search_writeable = writable;

	foreach_ItemOnMap(g_gui.GetCurrentMap(), finder, onSelection);
	finder.sort();

	std::vector<SearchResult> found;
	for (const auto& [tile, item] : finder.found) {
		SearchResult res;
		res.tile = tile;
		res.item = item;
		res.description = finder.desc(item);
		found.push_back(res);
	}

	g_gui.DestroyLoadBar();

	SearchResultWindow* result = g_gui.ShowSearchWindow();
	std::vector<SearchResultRow> rows;
	rows.reserve(found.size());
	for (size_t index = 0; index < found.size(); ++index) {
		const auto& res = found[index];
		rows.push_back(makeSearchResultRow(static_cast<uint32_t>(index + 1), wxstr(res.description), res.tile->getPosition()));
	}
	result->SetResults(std::move(rows), static_cast<uint32_t>(found.size()));
}
