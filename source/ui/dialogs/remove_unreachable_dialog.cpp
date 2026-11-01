//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "ui/dialogs/remove_unreachable_dialog.h"
#include "util/image_manager.h"
#include "app/main.h"

#include <wx/radiobut.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>

RemoveUnreachableDialog::RemoveUnreachableDialog(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Remove Unreachable Tiles", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
	wxBoxSizer* top_sizer = new wxBoxSizer(wxVERTICAL);

	// 1. Viewport Dimensions Group
	wxStaticBoxSizer* viewport_box = new wxStaticBoxSizer(wxVERTICAL, this, "Viewport Dimensions");

	radio_standard = new wxRadioButton(viewport_box->GetStaticBox(), wxID_ANY, "Standard Client (15 x 11)", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	radio_custom = new wxRadioButton(viewport_box->GetStaticBox(), wxID_ANY, "Custom Viewport");

	viewport_box->Add(radio_standard, wxSizerFlags().Border(wxALL, 5));

	wxBoxSizer* custom_sizer = new wxBoxSizer(wxHORIZONTAL);
	custom_sizer->Add(radio_custom, wxSizerFlags().CenterVertical().Border(wxRIGHT, 15));

	custom_sizer->Add(new wxStaticText(viewport_box->GetStaticBox(), wxID_ANY, "Width:"), wxSizerFlags().CenterVertical().Border(wxRIGHT, 5));
	custom_width_spin = new wxSpinCtrl(viewport_box->GetStaticBox(), wxID_ANY, "26", wxDefaultPosition, FromDIP(wxSize(70, -1)), wxSP_ARROW_KEYS, 5, 100, 26);
	custom_sizer->Add(custom_width_spin, wxSizerFlags().CenterVertical().Border(wxRIGHT, 15));

	custom_sizer->Add(new wxStaticText(viewport_box->GetStaticBox(), wxID_ANY, "Height:"), wxSizerFlags().CenterVertical().Border(wxRIGHT, 5));
	custom_height_spin = new wxSpinCtrl(viewport_box->GetStaticBox(), wxID_ANY, "11", wxDefaultPosition, FromDIP(wxSize(70, -1)), wxSP_ARROW_KEYS, 5, 100, 11);
	custom_sizer->Add(custom_height_spin, wxSizerFlags().CenterVertical());

	viewport_box->Add(custom_sizer, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 5));
	top_sizer->Add(viewport_box, wxSizerFlags().Expand().Border(wxALL, 10));

	// 2. Multi-Floor Visibility Group
	wxStaticBoxSizer* floor_box = new wxStaticBoxSizer(wxVERTICAL, this, "Multi-Floor Visibility");

	multi_floor_checkbox = new wxCheckBox(floor_box->GetStaticBox(), wxID_ANY, "Consider multi-floor visibility (Z-axis)");
	multi_floor_checkbox->SetValue(true);
	multi_floor_checkbox->SetToolTip("Checks viewports across visible adjacent floors with isometric floor offset projection.");
	floor_box->Add(multi_floor_checkbox, wxSizerFlags().Border(wxALL, 5));

	wxBoxSizer* scope_sizer = new wxBoxSizer(wxVERTICAL);

	radio_scope_all = new wxRadioButton(floor_box->GetStaticBox(), wxID_ANY, "All Floors (0 - 15)", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	radio_scope_surface = new wxRadioButton(floor_box->GetStaticBox(), wxID_ANY, "Surface Only (0 - 7)");
	radio_scope_underground = new wxRadioButton(floor_box->GetStaticBox(), wxID_ANY, "Underground Only (8 - 15)");

	radio_scope_all->SetValue(true);
	radio_scope_all->SetToolTip("Scan and remove unreachable tiles across both surface and underground layers.");
	radio_scope_surface->SetToolTip("Scan and remove unreachable tiles on surface levels only (e.g. open sea). Floors 8 - 15 are left untouched.");
	radio_scope_underground->SetToolTip("Scan and remove unreachable tiles underground only (e.g. cavern voids). Floors 0 - 7 are left untouched.");

	scope_sizer->Add(radio_scope_all, wxSizerFlags().Border(wxBOTTOM, 3));
	scope_sizer->Add(radio_scope_surface, wxSizerFlags().Border(wxBOTTOM, 3));
	scope_sizer->Add(radio_scope_underground, 0, 0);

	floor_box->Add(scope_sizer, wxSizerFlags().Border(wxLEFT | wxBOTTOM, 22));
	top_sizer->Add(floor_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 10));

	// 3. Cleanup Options Group
	wxStaticBoxSizer* options_box = new wxStaticBoxSizer(wxVERTICAL, this, "Cleanup Options");

	wxBoxSizer* margin_sizer = new wxBoxSizer(wxHORIZONTAL);
	margin_sizer->Add(new wxStaticText(options_box->GetStaticBox(), wxID_ANY, "Safety Margin (tiles):"), wxSizerFlags().CenterVertical().Border(wxRIGHT, 10));
	safety_margin_spin = new wxSpinCtrl(options_box->GetStaticBox(), wxID_ANY, "2", wxDefaultPosition, FromDIP(wxSize(65, -1)), wxSP_ARROW_KEYS, 0, 10, 2);
	safety_margin_spin->SetToolTip("Additional border margin added around viewports to prevent clipping tall sprites.");
	margin_sizer->Add(safety_margin_spin, wxSizerFlags().CenterVertical());
	options_box->Add(margin_sizer, wxSizerFlags().Border(wxALL, 5));

	backup_checkbox = new wxCheckBox(options_box->GetStaticBox(), wxID_ANY, "Create map backup before modifying");
	backup_checkbox->SetValue(true);
	backup_checkbox->SetToolTip("Saves a timestamped .otbm copy before removing any tiles.");
	options_box->Add(backup_checkbox, wxSizerFlags().Border(wxALL, 5));

	top_sizer->Add(options_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 10));

	// 4. OK / Cancel Buttons
	wxBoxSizer* button_sizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton* ok_button = new wxButton(this, wxID_OK, "Remove Tiles");
	ok_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	ok_button->SetDefault();
	button_sizer->Add(ok_button, wxSizerFlags().Border(wxRIGHT, 10));

	wxButton* cancel_button = new wxButton(this, wxID_CANCEL, "Cancel");
	cancel_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	button_sizer->Add(cancel_button, 0);

	top_sizer->Add(button_sizer, wxSizerFlags().Right().Border(wxLEFT | wxRIGHT | wxBOTTOM, 10));

	SetSizerAndFit(top_sizer);
	Centre(wxBOTH);

	radio_standard->Bind(wxEVT_RADIOBUTTON, &RemoveUnreachableDialog::OnViewportModeChanged, this);
	radio_custom->Bind(wxEVT_RADIOBUTTON, &RemoveUnreachableDialog::OnViewportModeChanged, this);
	multi_floor_checkbox->Bind(wxEVT_CHECKBOX, &RemoveUnreachableDialog::OnMultiFloorChanged, this);

	// Default to standard client (15x11)
	radio_standard->SetValue(true);
	UpdateControlStates();

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_BROOM, FromDIP(wxSize(32, 32))));
	SetIcon(icon);
}

void RemoveUnreachableDialog::OnViewportModeChanged(wxCommandEvent& WXUNUSED(event)) {
	UpdateControlStates();
}

void RemoveUnreachableDialog::OnMultiFloorChanged(wxCommandEvent& WXUNUSED(event)) {
	UpdateControlStates();
}

void RemoveUnreachableDialog::UpdateControlStates() {
	const bool is_custom = radio_custom->GetValue();
	custom_width_spin->Enable(is_custom);
	custom_height_spin->Enable(is_custom);

	const bool has_multi_floor = multi_floor_checkbox->GetValue();
	radio_scope_all->Enable(has_multi_floor);
	radio_scope_surface->Enable(has_multi_floor);
	radio_scope_underground->Enable(has_multi_floor);
}

EditorOperations::UnreachableCleanerSettings RemoveUnreachableDialog::GetSettings() const {
	EditorOperations::UnreachableCleanerSettings settings;
	if (radio_standard->GetValue()) {
		settings.viewport_width = 15;
		settings.viewport_height = 11;
	} else {
		settings.viewport_width = custom_width_spin->GetValue();
		settings.viewport_height = custom_height_spin->GetValue();
	}
	settings.safety_margin = safety_margin_spin->GetValue();
	settings.multi_floor = multi_floor_checkbox->GetValue();
	settings.create_backup = backup_checkbox->GetValue();

	if (settings.multi_floor) {
		if (radio_scope_surface && radio_scope_surface->GetValue()) {
			settings.floor_scope = EditorOperations::CleanerFloorScope::SurfaceOnly;
		} else if (radio_scope_underground && radio_scope_underground->GetValue()) {
			settings.floor_scope = EditorOperations::CleanerFloorScope::UndergroundOnly;
		} else {
			settings.floor_scope = EditorOperations::CleanerFloorScope::All;
		}
	} else {
		settings.floor_scope = EditorOperations::CleanerFloorScope::All;
	}
	return settings;
}
