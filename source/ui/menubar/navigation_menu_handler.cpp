#include "ui/menubar/navigation_menu_handler.h"
#include "app/main.h"
#include "ui/gui.h"
#include "ui/dialogs/find_dialog.h"
#include "ui/map_tab.h"
#include "ui/map_window.h"
#include "ui/dialogs/goto_position_dialog.h"
#include "ui/find_item_window.h"
#include "ui/main_menubar.h"
#include "ui/menubar/search_handler.h"

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
				SearchResults::ShowCreatures(current_map, dialog.getResult(), "Searching map...");
			} else {
				SearchResults::ShowItems(current_map, dialog.getResultID(), "Searching map...");
			}
		} else if (dialog.getResultAction() == FindItemDialog::ResultAction::SelectItem) {
			SearchResults::SelectFoundBrush(dialog);
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
