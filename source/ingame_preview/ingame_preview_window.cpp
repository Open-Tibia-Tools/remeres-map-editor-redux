#include <iostream>
#include "ingame_preview/ingame_preview_window.h"
#include "ingame_preview/ingame_preview_canvas.h"
#include "ui/dialogs/outfit_chooser_dialog.h"
#include "ui/controls/outfit_color_palette.h"
#include "game/preview_preferences.h"
#include "util/image_manager.h"

#include "editor/editor.h"
#include "ui/gui.h"
#include "rendering/ui/map_display.h"
#include "rendering/core/light_defaults.h"
#include <cmath>
#include <wx/tglbtn.h>

namespace IngamePreview {

	enum {
		ID_FOLLOW_SELECTION = 10001,
		ID_ENABLE_LIGHTING,
		ID_CHOOSE_OUTFIT,
		ID_AMBIENT_SLIDER,
		ID_INTENSITY_SLIDER,
		ID_SERVER_COLOR_BUTTON,
		ID_VIEWPORT_W_UP,
		ID_VIEWPORT_W_DOWN,
		ID_VIEWPORT_H_UP,
		ID_VIEWPORT_H_DOWN,
		ID_UPDATE_TIMER
	};

	IngamePreviewWindow::IngamePreviewWindow(wxWindow* parent) :
		wxPanel(parent, wxID_ANY),
		update_timer(this, ID_UPDATE_TIMER),
		follow_selection(true),
		server_light_color(static_cast<uint8_t>(g_gui.GetServerLightColor())) {

		// Load initial preferences
		g_preview_preferences.load();
		preview_outfit = g_preview_preferences.getOutfit();
		current_name = wxString::FromUTF8(g_preview_preferences.getName());
		current_speed = g_preview_preferences.getSpeed();

		// Bind Events
		Bind(wxEVT_TIMER, &IngamePreviewWindow::OnUpdateTimer, this, ID_UPDATE_TIMER);
		Bind(wxEVT_TOGGLEBUTTON, &IngamePreviewWindow::OnToggleFollow, this, ID_FOLLOW_SELECTION);
		Bind(wxEVT_TOGGLEBUTTON, &IngamePreviewWindow::OnToggleLighting, this, ID_ENABLE_LIGHTING);
		Bind(wxEVT_SLIDER, &IngamePreviewWindow::OnClientBrightnessSlider, this, ID_AMBIENT_SLIDER);
		Bind(wxEVT_SLIDER, &IngamePreviewWindow::OnIntensitySlider, this, ID_INTENSITY_SLIDER);
		Bind(wxEVT_BUTTON, &IngamePreviewWindow::OnChooseServerColor, this, ID_SERVER_COLOR_BUTTON);
		Bind(wxEVT_BUTTON, &IngamePreviewWindow::OnViewportWidthUp, this, ID_VIEWPORT_W_UP);
		Bind(wxEVT_BUTTON, &IngamePreviewWindow::OnViewportWidthDown, this, ID_VIEWPORT_W_DOWN);
		Bind(wxEVT_BUTTON, &IngamePreviewWindow::OnViewportHeightUp, this, ID_VIEWPORT_H_UP);
		Bind(wxEVT_BUTTON, &IngamePreviewWindow::OnViewportHeightDown, this, ID_VIEWPORT_H_DOWN);
		Bind(wxEVT_BUTTON, &IngamePreviewWindow::OnChooseOutfit, this, ID_CHOOSE_OUTFIT);

		wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

		// Single Toolbar
		wxBoxSizer* toolbar_sizer = new wxBoxSizer(wxHORIZONTAL);

		// Toggles
		follow_btn = new wxToggleButton(this, ID_FOLLOW_SELECTION, "", wxDefaultPosition, wxSize(28, 24));
		follow_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LOCATION, wxSize(16, 16), wxColour(76, 175, 80)));
		follow_btn->SetValue(true);
		follow_btn->SetToolTip("Follow Selection / Camera");
		toolbar_sizer->Add(follow_btn, wxSizerFlags().CenterVertical().Border(wxALL, 1));

		lighting_btn = new wxToggleButton(this, ID_ENABLE_LIGHTING, "", wxDefaultPosition, wxSize(28, 24));
		lighting_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_SUNNY, wxSize(16, 16), wxColour(255, 235, 59)));
		lighting_btn->SetValue(false);
		lighting_btn->SetToolTip("Toggle Lighting");
		toolbar_sizer->Add(lighting_btn, wxSizerFlags().CenterVertical().Border(wxALL, 1));

		outfit_btn = new wxButton(this, ID_CHOOSE_OUTFIT, "", wxDefaultPosition, wxSize(28, 24));
		outfit_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_ACCOUNT, wxSize(16, 16), wxColour(96, 125, 139)));
		outfit_btn->SetToolTip("Change Preview Creature Outfit");
		toolbar_sizer->Add(outfit_btn, wxSizerFlags().CenterVertical().Border(wxALL, 1));

		// Sliders
		const int initial_brightness = static_cast<int>(std::lround(g_gui.GetAmbientLightLevel() * 100.0f));
		ambient_slider = new wxSlider(this, ID_AMBIENT_SLIDER, initial_brightness, 0, 100, wxDefaultPosition, wxSize(60, -1));
		ambient_slider->SetToolTip("Client Light: minimum ambient light percentage");
		toolbar_sizer->Add(ambient_slider, wxSizerFlags().CenterVertical().Border(wxALL, 1));

		intensity_slider = new wxSlider(this, ID_INTENSITY_SLIDER, g_gui.GetLightIntensity(), 0, 255, wxDefaultPosition, wxSize(60, -1));
		intensity_slider->SetToolTip("Server Light: world light intensity received from the server");
		toolbar_sizer->Add(intensity_slider, wxSizerFlags().CenterVertical().Border(wxALL, 1));

		server_color_button = new wxButton(this, ID_SERVER_COLOR_BUTTON, "", wxDefaultPosition, wxSize(40, 24));
		server_color_button->SetToolTip("Server Color: world light color from the Tibia 8-bit palette");
		UpdateServerColorButton();
		toolbar_sizer->Add(server_color_button, wxSizerFlags().CenterVertical().Border(wxALL, 1));

		// Spacer
		toolbar_sizer->AddSpacer(4);
		toolbar_sizer->Add(new wxStaticText(this, wxID_ANY, "|"), wxSizerFlags().CenterVertical().Border(wxALL, 2));
		toolbar_sizer->AddSpacer(4);

		// Viewport Controls
		// Width
		viewport_w_down = new wxButton(this, ID_VIEWPORT_W_DOWN, "", wxDefaultPosition, wxSize(24, 24));
		viewport_w_down->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16), wxColour(0, 150, 136)));
		toolbar_sizer->Add(viewport_w_down, wxSizerFlags().CenterVertical().Border(wxALL, 0));

		viewport_x_text = new wxTextCtrl(this, wxID_ANY, "15", wxDefaultPosition, wxSize(30, -1), wxTE_READONLY | wxTE_CENTER);
		toolbar_sizer->Add(viewport_x_text, wxSizerFlags().CenterVertical().Border(wxALL, 1));

		viewport_w_up = new wxButton(this, ID_VIEWPORT_W_UP, "", wxDefaultPosition, wxSize(24, 24));
		viewport_w_up->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16), wxColour(0, 150, 136)));
		toolbar_sizer->Add(viewport_w_up, wxSizerFlags().CenterVertical().Border(wxALL, 0));

		toolbar_sizer->Add(new wxStaticText(this, wxID_ANY, "x"), wxSizerFlags().CenterVertical().Border(wxALL, 2));

		// Height
		viewport_h_down = new wxButton(this, ID_VIEWPORT_H_DOWN, "", wxDefaultPosition, wxSize(24, 24));
		viewport_h_down->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16), wxColour(0, 150, 136)));
		toolbar_sizer->Add(viewport_h_down, wxSizerFlags().CenterVertical().Border(wxALL, 0));

		viewport_y_text = new wxTextCtrl(this, wxID_ANY, "11", wxDefaultPosition, wxSize(30, -1), wxTE_READONLY | wxTE_CENTER);
		toolbar_sizer->Add(viewport_y_text, wxSizerFlags().CenterVertical().Border(wxALL, 1));

		viewport_h_up = new wxButton(this, ID_VIEWPORT_H_UP, "", wxDefaultPosition, wxSize(24, 24));
		viewport_h_up->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16), wxColour(0, 150, 136)));
		toolbar_sizer->Add(viewport_h_up, wxSizerFlags().CenterVertical().Border(wxALL, 0));

		main_sizer->Add(toolbar_sizer, wxSizerFlags().Expand().Border(wxALL, 2));

		// Canvas
		canvas = new IngamePreviewCanvas(this);
		main_sizer->Add(canvas, wxSizerFlags(1).Expand());

		// Sync UI State to Canvas
		canvas->SetLightingEnabled(lighting_btn->GetValue());
		canvas->SetClientBrightness(static_cast<uint8_t>(ambient_slider->GetValue()));
		canvas->SetLightIntensity(static_cast<uint8_t>(intensity_slider->GetValue()));
		canvas->SetServerLightColor(server_light_color);
		canvas->SetPreviewOutfit(preview_outfit);
		canvas->SetName(current_name.ToStdString());
		canvas->SetSpeed(current_speed);

		SetSizer(main_sizer);

		update_timer.Start(50);
	}

	IngamePreviewWindow::~IngamePreviewWindow() {
		spdlog::debug("IngamePreviewWindow destructor started");
		spdlog::default_logger()->flush();

		spdlog::debug("IngamePreviewWindow destructor - stopping update_timer");
		spdlog::default_logger()->flush();
		update_timer.Stop();

		spdlog::debug("IngamePreviewWindow destructor finished");
		// spdlog::default_logger()->flush();
	}

	void IngamePreviewWindow::OnUpdateTimer(wxTimerEvent& event) {
		UpdateState();
	}

	void IngamePreviewWindow::UpdateState() {
		if (!IsShownOnScreen()) {
			return;
		}

		MapTab* tab = g_gui.GetCurrentMapTab();
		if (!tab) {
			return;
		}

		Editor* active_editor = tab->GetEditor();
		if (!active_editor) {
			return;
		}

		if (follow_selection && !canvas->IsWalking()) {
			Position target;
			// Prioritize Active Selection
			if (!active_editor->selection.empty()) {
				Position min = active_editor->selection.minPosition();
				Position max = active_editor->selection.maxPosition();
				target = Position(min.x + (max.x - min.x) / 2, min.y + (max.y - min.y) / 2, min.z);
			} else {
				// Fallback to Screen Center (Camera)
				target = tab->GetScreenCenterPosition();
			}
			canvas->SetCameraPosition(target);
		}
	}

	void IngamePreviewWindow::OnToggleFollow(wxCommandEvent& event) {
		SetFollowSelection(follow_btn->GetValue());
	}

	void IngamePreviewWindow::SetFollowSelection(bool follow) {
		follow_selection = follow;
		if (follow_selection) {
			follow_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LOCATION, wxSize(16, 16), wxColour(76, 175, 80)));
		} else {
			follow_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LOCATION, wxSize(16, 16), wxColour(158, 158, 158)));
		}
		follow_btn->SetValue(follow_selection);
	}

	void IngamePreviewWindow::OnToggleLighting(wxCommandEvent& event) {
		canvas->SetLightingEnabled(lighting_btn->GetValue());
	}

	void IngamePreviewWindow::UpdateServerColorButton() {
		server_color_button->SetBitmap(CreateLightColorSwatchBitmap(this, server_light_color, wxSize(20, 16)));
		server_color_button->SetLabelText(wxEmptyString);
	}

	void IngamePreviewWindow::OnClientBrightnessSlider(wxCommandEvent& event) {
		canvas->SetClientBrightness(static_cast<uint8_t>(ambient_slider->GetValue()));
	}

	void IngamePreviewWindow::OnIntensitySlider(wxCommandEvent& event) {
		canvas->SetLightIntensity(static_cast<uint8_t>(intensity_slider->GetValue()));
	}

	void IngamePreviewWindow::OnChooseServerColor(wxCommandEvent& event) {
		LightColorPickerDialog dialog(this, server_light_color);
		if (dialog.ShowModal() != wxID_OK) {
			return;
		}

		server_light_color = static_cast<uint8_t>(dialog.GetSelectedColor());
		UpdateServerColorButton();
		canvas->SetServerLightColor(server_light_color);
	}

	void IngamePreviewWindow::OnViewportWidthUp(wxCommandEvent& event) {
		int w, h;
		canvas->GetViewportSize(w, h);
		w++;
		canvas->SetViewportSize(w, h);
		viewport_x_text->SetValue(std::to_string(w));
	}

	void IngamePreviewWindow::OnViewportWidthDown(wxCommandEvent& event) {
		int w, h;
		canvas->GetViewportSize(w, h);
		if (w > 15) {
			w--;
			canvas->SetViewportSize(w, h);
			viewport_x_text->SetValue(std::to_string(w));
		}
	}

	void IngamePreviewWindow::OnViewportHeightUp(wxCommandEvent& event) {
		int w, h;
		canvas->GetViewportSize(w, h);
		h++;
		canvas->SetViewportSize(w, h);
		viewport_y_text->SetValue(std::to_string(h));
	}

	void IngamePreviewWindow::OnViewportHeightDown(wxCommandEvent& event) {
		int w, h;
		canvas->GetViewportSize(w, h);
		if (h > 11) {
			h--;
			canvas->SetViewportSize(w, h);
			viewport_y_text->SetValue(std::to_string(h));
		}
	}

	void IngamePreviewWindow::OnChooseOutfit(wxCommandEvent& event) {
		OutfitChooserDialog dialog(this, preview_outfit);
		if (dialog.ShowModal() == wxID_OK) {
			preview_outfit = dialog.GetOutfit();
			current_name = dialog.GetName();
			current_speed = dialog.GetSpeed();

			canvas->SetPreviewOutfit(preview_outfit);
			canvas->SetName(current_name.ToStdString());
			canvas->SetSpeed(current_speed);
		}
	}

} // namespace IngamePreview
