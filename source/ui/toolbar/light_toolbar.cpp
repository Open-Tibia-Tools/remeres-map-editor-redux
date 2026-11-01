//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/toolbar/light_toolbar.h"
#include "ui/controls/outfit_color_palette.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "app/settings.h"
#include "util/image_manager.h"
#include "util/common.h"

#include <format>

const wxString LightToolBar::PANE_NAME = "light_toolbar";

LightToolBar::LightToolBar(wxWindow* parent) {
	wxSize icon_size = parent->FromDIP(wxSize(16, 16));

	toolbar = newd wxAuiToolBar(parent, TOOLBAR_LIGHT, wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_HORZ_TEXT);
	toolbar->SetToolBitmapSize(icon_size);

	wxStaticText* intensity_label = newd wxStaticText(toolbar, wxID_ANY, "Server Light:");
	server_light_intensity_slider = newd wxSlider(toolbar, ID_LIGHT_INTENSITY_SLIDER, g_gui.GetLightIntensity(), 0, 255, wxDefaultPosition, parent->FromDIP(wxSize(100, 20)));
	server_light_intensity_slider->SetToolTip("Server Light: world light intensity received from the server");

	wxStaticText* ambient_label = newd wxStaticText(toolbar, wxID_ANY, "Client Light:");
	client_brightness_slider = newd wxSlider(toolbar, ID_AMBIENT_LIGHT_SLIDER, static_cast<int>(g_gui.GetAmbientLightLevel() * 100.0f), 0, 100, wxDefaultPosition, parent->FromDIP(wxSize(100, 20)));
	client_brightness_slider->SetToolTip("Client Light: minimum ambient light percentage");

	wxStaticText* color_label = newd wxStaticText(toolbar, wxID_ANY, "Server Color:");
	server_light_color_button = newd wxButton(toolbar, ID_SERVER_LIGHT_COLOR_BUTTON, "", wxDefaultPosition, parent->FromDIP(wxSize(64, 24)));
	server_light_color_button->SetToolTip("Server Color: world light color from the Tibia 8-bit palette");
	UpdateServerLightColorButton();

	toolbar->AddControl(intensity_label);
	toolbar->AddControl(server_light_intensity_slider);
	toolbar->AddSeparator();
	toolbar->AddControl(ambient_label);
	toolbar->AddControl(client_brightness_slider);
	toolbar->AddSeparator();
	toolbar->AddControl(color_label);
	toolbar->AddControl(server_light_color_button);
	toolbar->AddSeparator();

	wxBitmap light_bitmap = IMAGE_MANAGER.GetBitmap(ICON_SUNNY, icon_size, wxColour(255, 235, 59));
	toolbar->AddTool(ID_LIGHT_TOGGLE, "Toggle Lighting", light_bitmap, "Toggle Lighting", wxITEM_CHECK);
	toolbar->ToggleTool(ID_LIGHT_TOGGLE, g_settings.getBoolean(Config::SHOW_LIGHTS));

	toolbar->Realize();

	server_light_intensity_slider->Bind(wxEVT_SLIDER, &LightToolBar::OnServerLightIntensitySlider, this);
	client_brightness_slider->Bind(wxEVT_SLIDER, &LightToolBar::OnClientBrightnessSlider, this);
	server_light_color_button->Bind(wxEVT_BUTTON, &LightToolBar::OnChooseServerLightColor, this);
	toolbar->Bind(wxEVT_TOOL, &LightToolBar::OnToggleLight, this, ID_LIGHT_TOGGLE);
}

LightToolBar::~LightToolBar() {
	server_light_intensity_slider->Unbind(wxEVT_SLIDER, &LightToolBar::OnServerLightIntensitySlider, this);
	client_brightness_slider->Unbind(wxEVT_SLIDER, &LightToolBar::OnClientBrightnessSlider, this);
	server_light_color_button->Unbind(wxEVT_BUTTON, &LightToolBar::OnChooseServerLightColor, this);
	if (toolbar) {
		toolbar->Unbind(wxEVT_TOOL, &LightToolBar::OnToggleLight, this, ID_LIGHT_TOGGLE);
	}
}

void LightToolBar::UpdateServerLightColorButton() {
	const int color_index = g_gui.GetServerLightColor();
	server_light_color_button->SetBitmap(CreateLightColorSwatchBitmap(toolbar, color_index, wxSize(24, 16)));
	server_light_color_button->SetLabel(wxstr(std::format(" {}", color_index)));
}

void LightToolBar::OnServerLightIntensitySlider(wxCommandEvent& event) {
	g_gui.SetLightIntensity(event.GetInt());
	spdlog::info("[LightToolBar] Server light intensity -> {}", event.GetInt());
	g_gui.RefreshView();
}

void LightToolBar::OnClientBrightnessSlider(wxCommandEvent& event) {
	g_gui.SetAmbientLightLevel(event.GetInt() / 100.0f);
	spdlog::info("[LightToolBar] Client brightness -> {} ({})", event.GetInt() / 100.0f, event.GetInt());
	g_gui.RefreshView();
}

void LightToolBar::OnChooseServerLightColor(wxCommandEvent& event) {
	LightColorPickerDialog dialog(toolbar, g_gui.GetServerLightColor());
	if (dialog.ShowModal() != wxID_OK) {
		return;
	}

	g_gui.SetServerLightColor(dialog.GetSelectedColor());
	UpdateServerLightColorButton();
	spdlog::info("[LightToolBar] Server light color -> {} (palette index)", dialog.GetSelectedColor());
	g_gui.RefreshView();
}

void LightToolBar::OnToggleLight(wxCommandEvent& event) {
	g_settings.setInteger(Config::SHOW_LIGHTS, event.IsChecked());
	g_gui.RefreshView();
}
