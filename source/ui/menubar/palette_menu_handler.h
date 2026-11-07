#ifndef RME_UI_MENUBAR_PALETTE_MENU_HANDLER_H
#define RME_UI_MENUBAR_PALETTE_MENU_HANDLER_H

#include "ui/gui_ids.h"

#include <wx/event.h>
#include <wx/menu.h>
#include <wx/menuitem.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class MainFrame;
class MainMenuBar;

class PaletteMenuHandler : public wxEvtHandler {
public:
	PaletteMenuHandler(MainFrame* frame, MainMenuBar* menubar);
	~PaletteMenuHandler();

	void OnNewPalette(wxCommandEvent& event);
	void OnShowPalette(wxCommandEvent& event);
	void OnSelectHousePalette(wxCommandEvent& event);
	void OnSelectWaypointPalette(wxCommandEvent& event);
	void OnSelectCatalogPalette(wxCommandEvent& event);

	void LoadPaletteMenu(wxMenu* menu);
	void RefreshPaletteMenu();

protected:
	void RebuildPaletteMenu();
	[[nodiscard]] std::vector<std::string> CurrentPaletteMenuEntries() const;

	MainFrame* frame;
	MainMenuBar* menubar;
	wxMenu* palette_menu = nullptr;
	bool last_loaded_state = false;
	std::vector<std::string> last_menu_entries;
	std::unordered_map<int, std::string> catalog_menu_ids;
};

#endif
