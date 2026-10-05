#include "ui/menubar/palette_menu_handler.h"

#include "app/main.h"
#include "game/materials.h"
#include "palette/hardcoded_palette_registry.h"
#include "palette/house/house_palette.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "ui/main_menubar.h"
#include "util/image_manager.h"

namespace {

void ClearMenu(wxMenu* menu) {
	if (!menu) {
		return;
	}

	while (menu->GetMenuItemCount() > 0) {
		if (wxMenuItem* item = menu->FindItemByPosition(0)) {
			menu->Destroy(item);
		} else {
			break;
		}
	}
}

void SetMenuIcon(wxMenuItem* item, std::string_view icon) {
	if (item && !icon.empty()) {
		item->SetBitmap(IMAGE_MANAGER.GetBitmap(icon, wxSize(16, 16)));
	}
}

[[nodiscard]] wxString PaletteLabel(std::string_view name) {
	if (name == "House") {
		return "House\tH";
	}
	if (name == "Waypoint") {
		return "Waypoint\tW";
	}
	return wxString::FromUTF8(std::string(name).c_str());
}

[[nodiscard]] std::string_view PaletteIcon(std::string_view name) {
	if (name == "House") {
		return ICON_HOUSE;
	}
	if (name == "Waypoint") {
		return ICON_FLAG;
	}
	if (name == "Zone") {
		return ICON_VECTOR_SQUARE;
	}
	return ICON_PALETTE;
}

} // namespace

PaletteMenuHandler::PaletteMenuHandler(MainFrame* frame, MainMenuBar* menubar) :
	frame(frame), menubar(menubar) {
	for (int id = PALETTE_MENU_FIRST; id <= PALETTE_MENU_LAST; ++id) {
		frame->Bind(wxEVT_COMMAND_MENU_SELECTED, &PaletteMenuHandler::OnSelectCatalogPalette, this, MAIN_FRAME_MENU + id);
	}
}

PaletteMenuHandler::~PaletteMenuHandler() {
}

void PaletteMenuHandler::OnNewPalette(wxCommandEvent& event) {
	g_gui.NewPalette();
}

void PaletteMenuHandler::OnShowPalette(wxCommandEvent& event) {
	g_gui.ShowPalette();
}

void PaletteMenuHandler::OnSelectHousePalette(wxCommandEvent& event) {
	g_gui.SelectPalettePage("House");
}

void PaletteMenuHandler::OnSelectWaypointPalette(wxCommandEvent& event) {
	g_gui.SelectPalettePage("Waypoint");
}

void PaletteMenuHandler::OnSelectCatalogPalette(wxCommandEvent& event) {
	const auto it = catalog_menu_ids.find(event.GetId());
	if (it == catalog_menu_ids.end()) {
		return;
	}

	g_gui.SelectPalettePage(it->second);
}

void PaletteMenuHandler::LoadPaletteMenu(wxMenu* menu) {
	palette_menu = menu;
	last_menu_entries.clear();
	RebuildPaletteMenu();
}

void PaletteMenuHandler::RefreshPaletteMenu() {
	if (!palette_menu) {
		return;
	}

	const bool loaded = g_version.IsVersionLoaded();
	const std::vector<std::string> entries = CurrentPaletteMenuEntries();
	if (loaded == last_loaded_state && entries == last_menu_entries) {
		return;
	}

	RebuildPaletteMenu();
}

std::vector<std::string> PaletteMenuHandler::CurrentPaletteMenuEntries() const {
	std::vector<std::string> entries;
	if (!g_version.IsVersionLoaded()) {
		return entries;
	}

	for (const auto& palette : g_materials.paletteCatalog().dynamicPalettes()) {
		entries.push_back(palette.name);
	}
	for (const auto& provider : GetHardcodedPaletteProviders()) {
		entries.push_back(std::string(provider.name));
	}
	return entries;
}

void PaletteMenuHandler::RebuildPaletteMenu() {
	if (!palette_menu) {
		return;
	}

	ClearMenu(palette_menu);
	catalog_menu_ids.clear();

	const bool loaded = g_version.IsVersionLoaded();
	wxMenuItem* showItem = palette_menu->Append(MAIN_FRAME_MENU + MenuBar::SHOW_PALETTE, "Show Palettes", "Displays the primary palette window.");
	SetMenuIcon(showItem, ICON_PALETTE);
	showItem->Enable(loaded);

	if (loaded) {
		const auto& palettes = g_materials.paletteCatalog().dynamicPalettes();
		const size_t maxMenuCount = static_cast<size_t>(PALETTE_MENU_LAST - PALETTE_MENU_FIRST + 1);
		if (!palettes.empty()) {
			palette_menu->AppendSeparator();
			for (size_t index = 0; index < palettes.size() && index < maxMenuCount; ++index) {
				const int menuId = MAIN_FRAME_MENU + PALETTE_MENU_FIRST + static_cast<int>(index);
				const wxString label = wxString::FromUTF8(palettes[index].name.c_str());
				wxMenuItem* item = palette_menu->Append(menuId, label, "Select the " + label + " palette.");
				SetMenuIcon(item, ICON_PALETTE);
				catalog_menu_ids.emplace(menuId, palettes[index].name);
			}
		}

		const size_t providerOffset = palettes.size();
		const auto& providers = GetHardcodedPaletteProviders();
		if (!providers.empty() && providerOffset < maxMenuCount) {
			palette_menu->AppendSeparator();
			for (size_t index = 0; index < providers.size() && providerOffset + index < maxMenuCount; ++index) {
				const int menuId = MAIN_FRAME_MENU + PALETTE_MENU_FIRST + static_cast<int>(providerOffset + index);
				const std::string providerName(providers[index].name);
				const wxString label = PaletteLabel(providers[index].name);
				wxMenuItem* item = palette_menu->Append(menuId, label, "Select the " + wxString::FromUTF8(providerName.c_str()) + " palette.");
				SetMenuIcon(item, PaletteIcon(providers[index].name));
				catalog_menu_ids.emplace(menuId, providerName);
			}
		}
	}

	last_loaded_state = loaded;
	last_menu_entries = CurrentPaletteMenuEntries();
}
