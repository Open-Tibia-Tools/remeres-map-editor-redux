#include "app/preferences/interface_page.h"
#include <format>

#include <cmath>

#include "app/main.h"
#include "app/preferences/preferences_layout.h"
#include "app/settings.h"
#include "ui/dialog_util.h"
#include "ui/gui.h"
#include "ui/theme.h"

namespace {
wxString FormatSpeedLabel(int raw_value) {
	return wxstr(std::format("{:.1f}x", raw_value / 10.0));
}
}

InterfacePage::InterfacePage(wxWindow* parent) : ScrollablePreferencesPage(parent) {
	auto* page_sizer = GetPageSizer();

	auto* theme_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Theme",
		"Choose the global editor theme and keep the overall interface readable across long mapping sessions."
	);
	theme_choice = new wxChoice(theme_section, wxID_ANY);
	theme_choice->Append("System Default");
	theme_choice->Append("Dark");
	theme_choice->Append("Light");
	const int current_theme = g_settings.getInteger(Config::THEME);
	theme_choice->SetSelection(current_theme >= 0 && current_theme <= 2 ? current_theme : 0);
	PreferencesLayout::AddControlRow(
		theme_section,
		"Theme",
		"Apply the system theme or force the editor into a specific light or dark appearance.",
		theme_choice
	);
	page_sizer->Add(theme_section, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(10)));

	auto* palette_style_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Palette Style",
		"Pick the icon size used by XML-defined dynamic palettes."
	);
	auto* palette_style_sizer = palette_style_section->GetBodySizer();
	dynamic_palette_style_choice = AddPaletteStyleChoice(palette_style_section, palette_style_sizer, "Dynamic palette icon size", "Choose the icon size used by XML-defined dynamic palettes.", g_settings.getString(Config::PALETTE_DYNAMIC_STYLE));
	page_sizer->Add(palette_style_section, wxSizerFlags().Expand().Border(wxALL, FromDIP(10)));

	auto* density_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Icon Density",
		"Scale palette and picker icons for denser browsing or for easier reading on larger displays."
	);
	large_terrain_tools_chkbox = PreferencesLayout::AddCheckBoxRow(density_section, "Large terrain tool and size icons", "Increase the size of terrain palette tool buttons and size selectors.", g_settings.getBoolean(Config::USE_LARGE_TERRAIN_TOOLBAR));
	large_collection_tools_chkbox = PreferencesLayout::AddCheckBoxRow(density_section, "Large collections tool and size icons", "Increase the size of collection palette tool buttons and size selectors.", g_settings.getBoolean(Config::USE_LARGE_COLLECTION_TOOLBAR));
	large_doodad_sizebar_chkbox = PreferencesLayout::AddCheckBoxRow(density_section, "Large doodad palette icons", "Use larger icon sizes inside the doodad palette controls.", g_settings.getBoolean(Config::USE_LARGE_DOODAD_SIZEBAR));
	large_item_sizebar_chkbox = PreferencesLayout::AddCheckBoxRow(density_section, "Large item palette icons", "Use larger icon sizes inside the item palette controls.", g_settings.getBoolean(Config::USE_LARGE_ITEM_SIZEBAR));
	large_house_sizebar_chkbox = PreferencesLayout::AddCheckBoxRow(density_section, "Large house palette icons", "Use larger icon sizes inside house palette controls.", g_settings.getBoolean(Config::USE_LARGE_HOUSE_SIZEBAR));
	large_raw_sizebar_chkbox = PreferencesLayout::AddCheckBoxRow(density_section, "Large RAW palette icons", "Use larger icon sizes inside RAW palette controls.", g_settings.getBoolean(Config::USE_LARGE_RAW_SIZEBAR));
	large_container_icons_chkbox = PreferencesLayout::AddCheckBoxRow(density_section, "Large container view icons", "Increase icon size in container browsing windows.", g_settings.getBoolean(Config::USE_LARGE_CONTAINER_ICONS));
	large_pick_item_icons_chkbox = PreferencesLayout::AddCheckBoxRow(density_section, "Large item picker icons", "Increase icon size in add-item and item-picker dialogs.", g_settings.getBoolean(Config::USE_LARGE_CHOOSE_ITEM_ICONS));
	page_sizer->Add(density_section, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10)));

	auto* navigation_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Mouse and Navigation",
		"Adjust input behavior for scrolling, zooming, and opening property dialogs while mapping."
	);
	switch_mousebtn_chkbox = PreferencesLayout::AddCheckBoxRow(navigation_section, "Switch mouse buttons", "Swap the middle and right mouse button behavior used in the editor.", g_settings.getBoolean(Config::SWITCH_MOUSEBUTTONS));
	doubleclick_properties_chkbox = PreferencesLayout::AddCheckBoxRow(navigation_section, "Double click opens properties", "Open the top item properties dialog by double clicking a tile.", g_settings.getBoolean(Config::DOUBLECLICK_PROPERTIES));
	inversed_scroll_chkbox = PreferencesLayout::AddCheckBoxRow(navigation_section, "Use inverted map drag", "Invert middle-mouse dragging for RTS-style navigation.", g_settings.getFloat(Config::SCROLL_SPEED) < 0);

	auto* scroll_panel = new wxPanel(navigation_section, wxID_ANY);
	scroll_panel->SetBackgroundColour(navigation_section->GetBackgroundColour());
	auto* scroll_panel_sizer = new wxBoxSizer(wxHORIZONTAL);
	auto true_scrollspeed = static_cast<int>(std::abs(g_settings.getFloat(Config::SCROLL_SPEED)) * 10);
	scroll_speed_slider = new wxSlider(scroll_panel, wxID_ANY, true_scrollspeed, 1, std::max(true_scrollspeed, 100));
	scroll_panel_sizer->Add(scroll_speed_slider, wxSizerFlags(1).Expand().Border(wxRIGHT, FromDIP(10)));
	scroll_speed_value_label = PreferencesLayout::CreateBodyText(scroll_panel, FormatSpeedLabel(true_scrollspeed), true);
	scroll_panel_sizer->Add(scroll_speed_value_label, wxSizerFlags().CenterVertical());
	scroll_panel->SetSizer(scroll_panel_sizer);
	PreferencesLayout::AddControlRow(navigation_section, "Scroll speed", "How fast the map pans while dragging with the middle mouse button.", scroll_panel, true);

	auto* zoom_panel = new wxPanel(navigation_section, wxID_ANY);
	zoom_panel->SetBackgroundColour(navigation_section->GetBackgroundColour());
	auto* zoom_panel_sizer = new wxBoxSizer(wxHORIZONTAL);
	auto true_zoomspeed = static_cast<int>(g_settings.getFloat(Config::ZOOM_SPEED) * 10);
	zoom_speed_slider = new wxSlider(zoom_panel, wxID_ANY, true_zoomspeed, 1, std::max(true_zoomspeed, 100));
	zoom_panel_sizer->Add(zoom_speed_slider, wxSizerFlags(1).Expand().Border(wxRIGHT, FromDIP(10)));
	zoom_speed_value_label = PreferencesLayout::CreateBodyText(zoom_panel, FormatSpeedLabel(true_zoomspeed), true);
	zoom_panel_sizer->Add(zoom_speed_value_label, wxSizerFlags().CenterVertical());
	zoom_panel->SetSizer(zoom_panel_sizer);
	PreferencesLayout::AddControlRow(navigation_section, "Zoom speed", "How quickly the view zooms in and out when using the mouse wheel.", zoom_panel, true);
	page_sizer->Add(navigation_section, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10)));

	scroll_speed_slider->Bind(wxEVT_SLIDER, [this](wxCommandEvent&) {
		UpdateSpeedLabels();
	});
	zoom_speed_slider->Bind(wxEVT_SLIDER, [this](wxCommandEvent&) {
		UpdateSpeedLabels();
	});
	UpdateSpeedLabels();

	FinishLayout();
}

void InterfacePage::UpdateSpeedLabels() {
	if (scroll_speed_value_label) {
		scroll_speed_value_label->SetLabelText(FormatSpeedLabel(scroll_speed_slider->GetValue()));
	}
	if (zoom_speed_value_label) {
		zoom_speed_value_label->SetLabelText(FormatSpeedLabel(zoom_speed_slider->GetValue()));
	}
}

wxChoice* InterfacePage::AddPaletteStyleChoice(wxWindow* parent, wxSizer* sizer, const wxString& short_description, const wxString& description, const std::string& setting) {
	wxUnusedVar(sizer);
	auto* choice = new wxChoice(parent, wxID_ANY);
	choice->Append("32x32 px");
	choice->Append("64x64 px");
	choice->Append("128x128 px");
	choice->Append("List style");

	if (setting == "32x32 px" || setting == "large icons" || setting == "small icons") {
		choice->SetSelection(0);
	} else if (setting == "64x64 px") {
		choice->SetSelection(1);
	} else if (setting == "128x128 px") {
		choice->SetSelection(2);
	} else if (setting == "listbox" || setting == "textlistbox" || setting == "List style") {
		choice->SetSelection(3);
	} else {
		choice->SetSelection(0);
	}

	PreferencesLayout::AddControlRow(static_cast<PreferencesSectionPanel*>(parent), short_description, description, choice);
	return choice;
}

bool InterfacePage::SetPaletteStyleChoice(wxChoice* ctrl, int key) {
	std::string current = g_settings.getString(key);
	std::string new_val = current;

	if (ctrl->GetSelection() == 0) {
		new_val = "32x32 px";
	} else if (ctrl->GetSelection() == 1) {
		new_val = "64x64 px";
	} else if (ctrl->GetSelection() == 2) {
		new_val = "128x128 px";
	} else if (ctrl->GetSelection() == 3) {
		new_val = "listbox";
	}

	if (current != new_val) {
		g_settings.setString(key, new_val);
		return true;
	}
	return false;
}

void InterfacePage::Apply() {
	bool palette_update_needed = false;

	if (SetPaletteStyleChoice(dynamic_palette_style_choice, Config::PALETTE_DYNAMIC_STYLE)) {
		palette_update_needed = true;
	}

	auto check_and_set = [&](wxCheckBox* chk, int key) {
		if (g_settings.getBoolean(key) != chk->GetValue()) {
			g_settings.setInteger(key, chk->GetValue());
			return true;
		}
		return false;
	};

	if (check_and_set(large_terrain_tools_chkbox, Config::USE_LARGE_TERRAIN_TOOLBAR)) {
		palette_update_needed = true;
	}
	if (check_and_set(large_collection_tools_chkbox, Config::USE_LARGE_COLLECTION_TOOLBAR)) {
		palette_update_needed = true;
	}
	if (check_and_set(large_doodad_sizebar_chkbox, Config::USE_LARGE_DOODAD_SIZEBAR)) {
		palette_update_needed = true;
	}
	if (check_and_set(large_item_sizebar_chkbox, Config::USE_LARGE_ITEM_SIZEBAR)) {
		palette_update_needed = true;
	}
	if (check_and_set(large_house_sizebar_chkbox, Config::USE_LARGE_HOUSE_SIZEBAR)) {
		palette_update_needed = true;
	}
	if (check_and_set(large_raw_sizebar_chkbox, Config::USE_LARGE_RAW_SIZEBAR)) {
		palette_update_needed = true;
	}
	if (check_and_set(large_container_icons_chkbox, Config::USE_LARGE_CONTAINER_ICONS)) {
		palette_update_needed = true;
	}
	if (check_and_set(large_pick_item_icons_chkbox, Config::USE_LARGE_CHOOSE_ITEM_ICONS)) {
		palette_update_needed = true;
	}

	g_settings.setInteger(Config::SWITCH_MOUSEBUTTONS, switch_mousebtn_chkbox->GetValue());
	g_settings.setInteger(Config::DOUBLECLICK_PROPERTIES, doubleclick_properties_chkbox->GetValue());

	float scroll_mul = inversed_scroll_chkbox->GetValue() ? -1.0f : 1.0f;
	g_settings.setFloat(Config::SCROLL_SPEED, scroll_mul * scroll_speed_slider->GetValue() / 10.f);
	g_settings.setFloat(Config::ZOOM_SPEED, zoom_speed_slider->GetValue() / 10.f);

	if (palette_update_needed) {
		g_gui.RebuildPalettes();
	}

	const int selected_theme = theme_choice->GetSelection();
	if (selected_theme != wxNOT_FOUND && g_settings.getInteger(Config::THEME) != selected_theme) {
		g_settings.setInteger(Config::THEME, selected_theme);
		Theme::setType(static_cast<Theme::Type>(selected_theme));
		DialogUtil::PopupDialog(wxString("Theme Changed"), wxString("Theme changed. Please restart the application for all changes to take effect."), wxOK);
	}
}
