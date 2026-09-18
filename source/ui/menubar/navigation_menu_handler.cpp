#include "ui/menubar/navigation_menu_handler.h"
#include "app/main.h"
#include "ui/gui.h"
#include "ui/dialogs/find_dialog.h"
#include "ui/map_tab.h"
#include "ui/map_window.h"
#include "ui/dialogs/goto_position_dialog.h"
#include "ui/find_item_window.h"
#include "ui/main_menubar.h"
#include "app/preferences.h"
#include "ui/result_window.h"
#include "editor/operations/search_operations.h"

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
}

NavigationMenuHandler::NavigationMenuHandler(MainFrame* frame, MainMenuBar* menubar) :
	frame(frame), menubar(menubar) {
}

NavigationMenuHandler::~NavigationMenuHandler() {
}

void NavigationMenuHandler::OnZoomIn(wxCommandEvent& event) {
	double zoom = g_gui.GetCurrentZoom();
	g_gui.SetCurrentZoom(zoom - 0.1);
}

void NavigationMenuHandler::OnZoomOut(wxCommandEvent& event) {
	double zoom = g_gui.GetCurrentZoom();
	g_gui.SetCurrentZoom(zoom + 0.1);
}

void NavigationMenuHandler::OnZoomNormal(wxCommandEvent& event) {
	g_gui.SetCurrentZoom(1.0);
}

void NavigationMenuHandler::OnGotoPreviousPosition(wxCommandEvent& WXUNUSED(event)) {
	MapTab* mapTab = g_gui.GetCurrentMapTab();
	if (mapTab) {
		mapTab->GoToPreviousCenterPosition();
	}
}

void NavigationMenuHandler::OnGotoPosition(wxCommandEvent& WXUNUSED(event)) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	// Display dialog, it also controls the actual jump
	GotoPositionDialog dlg(frame, *g_gui.GetCurrentEditor());
	dlg.ShowModal();
}

void NavigationMenuHandler::OnJumpToBrush(wxCommandEvent& WXUNUSED(event)) {
	if (!g_version.IsVersionLoaded()) {
		return;
	}

	// Create the jump to dialog
	FindDialog* dlg = newd FindBrushDialog(frame);

	// Display dialog to user
	dlg->ShowModal();

	// Retrieve result, if null user canceled
	const Brush* brush = dlg->getResult();
	if (brush) {
		g_gui.SelectBrush(brush);
	}
	dlg->Destroy();
}

void NavigationMenuHandler::OnJumpToItemBrush(wxCommandEvent& WXUNUSED(event)) {
	if (!g_version.IsVersionLoaded()) {
		return;
	}

	// Create the jump to dialog
	FindItemDialog dialog(frame, "Jump to Item", false, FindItemDialog::ActionSet::SearchAndSelect, AdvancedFinderDefaultAction::SelectItem, true);
	const int modal_result = dialog.ShowModal();
	if (modal_result != wxID_CANCEL) {
		if (dialog.getResultAction() == FindItemDialog::ResultAction::SearchMap) {
			if (!g_gui.IsEditorOpen()) {
				return;
			}

			Map* current_map = &g_gui.GetCurrentMap();
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

void NavigationMenuHandler::OnChangeFloor(wxCommandEvent& event) {
	// Workaround to stop events from looping
	if (menubar->checking_programmaticly) {
		return;
	}

	// this will have to be changed if you want to have more floors
	// see MAKE_ACTION(FLOOR_0, wxITEM_RADIO, OnChangeFloor);
	if (MAP_MAX_LAYER < 16) {
		for (int i = 0; i < MAP_LAYERS; ++i) {
			if (menubar->IsItemChecked(MenuBar::ActionID(MenuBar::FLOOR_0 + i))) {
				g_gui.ChangeFloor(i);
			}
		}
	}
}
