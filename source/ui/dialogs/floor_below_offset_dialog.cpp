#include "ui/dialogs/floor_below_offset_dialog.h"
#include "app/settings.h"
#include "util/image_manager.h"

#include <wx/spinctrl.h>

FloorBelowOffsetDialog::FloorBelowOffsetDialog(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Floor Below Offset", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {

	auto* top_sizer = new wxBoxSizer(wxVERTICAL);

	auto* grid_sizer = new wxFlexGridSizer(2, FromDIP(8), FromDIP(12));
	grid_sizer->AddGrowableCol(1);

	auto* label_x = new wxStaticText(this, wxID_ANY, "X Offset (-1 to 1):");
	spin_x = new wxSpinCtrl(
		this,
		wxID_ANY,
		wxEmptyString,
		wxDefaultPosition,
		FromDIP(wxSize(72, 22)),
		wxSP_ARROW_KEYS,
		-1,
		1,
		g_settings.getInteger(Config::DRAW_ON_FLOOR_BELOW_OFFSET_X)
	);

	auto* label_y = new wxStaticText(this, wxID_ANY, "Y Offset (-1 to 1):");
	spin_y = new wxSpinCtrl(
		this,
		wxID_ANY,
		wxEmptyString,
		wxDefaultPosition,
		FromDIP(wxSize(72, 22)),
		wxSP_ARROW_KEYS,
		-1,
		1,
		g_settings.getInteger(Config::DRAW_ON_FLOOR_BELOW_OFFSET_Y)
	);

	grid_sizer->Add(label_x, wxSizerFlags().CenterVertical());
	grid_sizer->Add(spin_x, wxSizerFlags().Expand());
	grid_sizer->Add(label_y, wxSizerFlags().CenterVertical());
	grid_sizer->Add(spin_y, wxSizerFlags().Expand());

	top_sizer->Add(grid_sizer, wxSizerFlags().Expand().Border(wxALL, FromDIP(16)));

	auto* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
	auto* ok_btn = new wxButton(this, wxID_OK, "OK");
	ok_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	auto* cancel_btn = new wxButton(this, wxID_CANCEL, "Cancel");
	cancel_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));

	btn_sizer->Add(ok_btn, wxSizerFlags().Center().Border(wxRIGHT, FromDIP(6)));
	btn_sizer->Add(cancel_btn, wxSizerFlags().Center());
	top_sizer->Add(btn_sizer, wxSizerFlags().Right().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(16)));

	SetSizerAndFit(top_sizer);
	Centre(wxBOTH);

	ok_btn->Bind(wxEVT_BUTTON, &FloorBelowOffsetDialog::OnClickOK, this);
	cancel_btn->Bind(wxEVT_BUTTON, &FloorBelowOffsetDialog::OnClickCancel, this);

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_ARROWS_UP_DOWN_LEFT_RIGHT, FromDIP(wxSize(16, 16))));
	SetIcon(icon);
}

void FloorBelowOffsetDialog::OnClickOK(wxCommandEvent&) {
	g_settings.setInteger(Config::DRAW_ON_FLOOR_BELOW_OFFSET_X, spin_x->GetValue());
	g_settings.setInteger(Config::DRAW_ON_FLOOR_BELOW_OFFSET_Y, spin_y->GetValue());
	EndModal(wxID_OK);
}

void FloorBelowOffsetDialog::OnClickCancel(wxCommandEvent&) {
	EndModal(wxID_CANCEL);
}
