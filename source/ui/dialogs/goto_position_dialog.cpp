//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/dialogs/goto_position_dialog.h"
#include "editor/editor.h"
#include "map/map.h"
#include "ui/positionctrl.h"
#include "ui/gui.h"
#include "util/image_manager.h"

// ============================================================================
// Go To Position Dialog
// Jump to a position on the map by entering XYZ coordinates

GotoPositionDialog::GotoPositionDialog(wxWindow* parent, Editor& editor) :
	wxDialog(parent, wxID_ANY, "Go To Position", wxDefaultPosition, wxDefaultSize),
	editor(editor) {
	Map& map = editor.map;

	// create topsizer
	wxSizer* sizer = new wxBoxSizer(wxVERTICAL);

	posctrl = new PositionCtrl(this, "Destination", map.getWidth() / 2, map.getHeight() / 2, GROUND_LAYER, map.getWidth(), map.getHeight());
	sizer->Add(posctrl, wxSizerFlags().Border(wxTOP | wxLEFT | wxRIGHT, 20));

	// OK/Cancel buttons
	wxSizer* tmpsizer = new wxBoxSizer(wxHORIZONTAL);
	wxButton* okBtn = new wxButton(this, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	okBtn->SetToolTip("Go to position");
	tmpsizer->Add(okBtn, wxSizerFlags(1).Center());
	wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");
	cancelBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	cancelBtn->SetToolTip("Close this window");
	tmpsizer->Add(cancelBtn, wxSizerFlags(1).Center());
	sizer->Add(tmpsizer, wxSizerFlags().Center().Border(wxALL, 20)); // Border to top too

	SetSizerAndFit(sizer);
	Centre(wxBOTH);

	okBtn->Bind(wxEVT_BUTTON, &GotoPositionDialog::OnClickOK, this);
	cancelBtn->Bind(wxEVT_BUTTON, &GotoPositionDialog::OnClickCancel, this);

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_LOCATION, FromDIP(wxSize(32, 32))));
	SetIcon(icon);
}

void GotoPositionDialog::OnClickCancel(wxCommandEvent&) {
	EndModal(0);
}

void GotoPositionDialog::OnClickOK(wxCommandEvent&) {
	g_gui.SetScreenCenterPosition(posctrl->GetPosition());
	EndModal(1);
}
