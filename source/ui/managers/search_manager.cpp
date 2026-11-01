#include "app/main.h"
#include "ui/managers/search_manager.h"
#include "ui/gui.h"
#include "ui/result_window.h"

SearchManager g_search;

SearchManager::SearchManager() :
	search_result_window(nullptr) {
}

SearchManager::~SearchManager() {
	// Explicit teardown happens from MainFrame::OnExit() while g_gui is still valid.
}

void SearchManager::DestroySearchWindow() {
	if (search_result_window == nullptr) {
		return;
	}

	SearchResultWindow* window = search_result_window;
	search_result_window = nullptr;

	if (g_gui.aui_manager != nullptr) {
		g_gui.aui_manager->DetachPane(window);
		g_gui.aui_manager->Update();
	}

	window->Destroy();
}

SearchResultWindow* SearchManager::ShowSearchWindow() {
	if (search_result_window == nullptr) {
		search_result_window = new SearchResultWindow(g_gui.root);
		g_gui.aui_manager->AddPane(search_result_window, wxAuiPaneInfo().Caption("Search Results"));
	} else {
		g_gui.aui_manager->GetPane(search_result_window).Show();
	}
	g_gui.aui_manager->Update();
	return search_result_window;
}
