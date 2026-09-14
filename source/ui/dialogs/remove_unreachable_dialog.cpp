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
	wxBoxSizer* top_sizer = newd wxBoxSizer(wxVERTICAL);

	// 1. Viewport Dimensions Group
	wxStaticBoxSizer* viewport_box = newd wxStaticBoxSizer(wxVERTICAL, this, "Viewport Dimensions");

	radio_standard = newd wxRadioButton(viewport_box->GetStaticBox(), wxID_ANY, "Standard Client (15 x 11)", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	radio_custom = newd wxRadioButton(viewport_box->GetStaticBox(), wxID_ANY, "Custom Viewport");

	viewport_box->Add(radio_standard, 0, wxALL, 5);

	wxBoxSizer* custom_sizer = newd wxBoxSizer(wxHORIZONTAL);
	custom_sizer->Add(radio_custom, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 15);

	custom_sizer->Add(newd wxStaticText(viewport_box->GetStaticBox(), wxID_ANY, "Width:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	custom_width_spin = newd wxSpinCtrl(viewport_box->GetStaticBox(), wxID_ANY, "26", wxDefaultPosition, FROM_DIP(this, wxSize(70, -1)), wxSP_ARROW_KEYS, 5, 100, 26);
	custom_sizer->Add(custom_width_spin, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 15);

	custom_sizer->Add(newd wxStaticText(viewport_box->GetStaticBox(), wxID_ANY, "Height:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	custom_height_spin = newd wxSpinCtrl(viewport_box->GetStaticBox(), wxID_ANY, "11", wxDefaultPosition, FROM_DIP(this, wxSize(70, -1)), wxSP_ARROW_KEYS, 5, 100, 11);
	custom_sizer->Add(custom_height_spin, 0, wxALIGN_CENTER_VERTICAL);

	viewport_box->Add(custom_sizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	top_sizer->Add(viewport_box, 0, wxEXPAND | wxALL, 10);

	// 2. Floor Scope Group
	wxStaticBoxSizer* scope_box = newd wxStaticBoxSizer(wxVERTICAL, this, "Floor Scope");

	radio_scope_all = newd wxRadioButton(scope_box->GetStaticBox(), wxID_ANY, "All Floors (0 - 15)", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	radio_scope_surface = newd wxRadioButton(scope_box->GetStaticBox(), wxID_ANY, "Surface Only (0 - 7)");
	radio_scope_underground = newd wxRadioButton(scope_box->GetStaticBox(), wxID_ANY, "Underground Only (8 - 15)");

	radio_scope_all->SetValue(true);
	radio_scope_all->SetToolTip("Scan and remove unreachable tiles across both surface and underground layers.");
	radio_scope_surface->SetToolTip("Scan and remove unreachable tiles on surface levels only (e.g. open sea). Floors 8 - 15 are left untouched.");
	radio_scope_underground->SetToolTip("Scan and remove unreachable tiles underground only (e.g. cavern voids). Floors 0 - 7 are left untouched.");

	scope_box->Add(radio_scope_all, 0, wxALL, 5);
	scope_box->Add(radio_scope_surface, 0, wxALL, 5);
	scope_box->Add(radio_scope_underground, 0, wxALL, 5);
	top_sizer->Add(scope_box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

	// 3. Cleanup Options Group
	wxStaticBoxSizer* options_box = newd wxStaticBoxSizer(wxVERTICAL, this, "Cleanup Options");

	wxBoxSizer* margin_sizer = newd wxBoxSizer(wxHORIZONTAL);
	margin_sizer->Add(newd wxStaticText(options_box->GetStaticBox(), wxID_ANY, "Safety Margin (tiles):"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	safety_margin_spin = newd wxSpinCtrl(options_box->GetStaticBox(), wxID_ANY, "2", wxDefaultPosition, FROM_DIP(this, wxSize(65, -1)), wxSP_ARROW_KEYS, 0, 10, 2);
	safety_margin_spin->SetToolTip("Additional border margin added around viewports to prevent clipping tall sprites.");
	margin_sizer->Add(safety_margin_spin, 0, wxALIGN_CENTER_VERTICAL);
	options_box->Add(margin_sizer, 0, wxALL, 5);

	multi_floor_checkbox = newd wxCheckBox(options_box->GetStaticBox(), wxID_ANY, "Consider multi-floor visibility (Z-axis)");
	multi_floor_checkbox->SetValue(true);
	multi_floor_checkbox->SetToolTip("Checks viewports across visible adjacent floors with isometric floor offset projection.");
	options_box->Add(multi_floor_checkbox, 0, wxALL, 5);

	backup_checkbox = newd wxCheckBox(options_box->GetStaticBox(), wxID_ANY, "Create map backup before modifying");
	backup_checkbox->SetValue(true);
	backup_checkbox->SetToolTip("Saves a timestamped .otbm copy before removing any tiles.");
	options_box->Add(backup_checkbox, 0, wxALL, 5);

	top_sizer->Add(options_box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

	// 3. OK / Cancel Buttons
	wxBoxSizer* button_sizer = newd wxBoxSizer(wxHORIZONTAL);

	wxButton* ok_button = newd wxButton(this, wxID_OK, "Remove Tiles");
	ok_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CHECK, wxSize(16, 16)));
	ok_button->SetDefault();
	button_sizer->Add(ok_button, 0, wxRIGHT, 10);

	wxButton* cancel_button = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancel_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_XMARK, wxSize(16, 16)));
	button_sizer->Add(cancel_button, 0);

	top_sizer->Add(button_sizer, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 10);

	SetSizerAndFit(top_sizer);
	Centre(wxBOTH);

	radio_standard->Bind(wxEVT_RADIOBUTTON, &RemoveUnreachableDialog::OnViewportModeChanged, this);
	radio_custom->Bind(wxEVT_RADIOBUTTON, &RemoveUnreachableDialog::OnViewportModeChanged, this);

	// Default to custom (26x11) as requested by user
	radio_custom->SetValue(true);
	UpdateControlStates();

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_BROOM, wxSize(32, 32)));
	SetIcon(icon);
}

void RemoveUnreachableDialog::OnViewportModeChanged(wxCommandEvent& WXUNUSED(event)) {
	UpdateControlStates();
}

void RemoveUnreachableDialog::UpdateControlStates() {
	const bool is_custom = radio_custom->GetValue();
	custom_width_spin->Enable(is_custom);
	custom_height_spin->Enable(is_custom);
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

	if (radio_scope_surface && radio_scope_surface->GetValue()) {
		settings.floor_scope = EditorOperations::CleanerFloorScope::SurfaceOnly;
	} else if (radio_scope_underground && radio_scope_underground->GetValue()) {
		settings.floor_scope = EditorOperations::CleanerFloorScope::UndergroundOnly;
	} else {
		settings.floor_scope = EditorOperations::CleanerFloorScope::All;
	}
	return settings;
}
