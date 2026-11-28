//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/properties/spawn_properties_window.h"

#include "map/map.h"
#include "app/application.h"
#include "util/image_manager.h"

SpawnPropertiesWindow::SpawnPropertiesWindow(wxWindow* win_parent, const Map* map, const Tile* tile_parent, Spawn* spawn, wxPoint pos) :
	ObjectPropertiesWindowBase(win_parent, "Spawn Properties", map, tile_parent, spawn, pos),
	count_field(nullptr) {
	ASSERT(edit_spawn);

	Bind(wxEVT_BUTTON, &SpawnPropertiesWindow::OnClickOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &SpawnPropertiesWindow::OnClickCancel, this, wxID_CANCEL);

	wxSizer* topsizer = new wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer* boxsizer = new wxStaticBoxSizer(wxVERTICAL, this, "Spawn Properties");
	wxWindow* static_box = boxsizer->GetStaticBox();

	wxFlexGridSizer* subsizer = new wxFlexGridSizer(2, 10, 10);
	subsizer->AddGrowableCol(1);

	subsizer->Add(new wxStaticText(static_box, wxID_ANY, "Spawn size"));
	count_field = new wxSpinCtrl(static_box, wxID_ANY, std::to_string(edit_spawn->getSize()), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, g_settings.getInteger(Config::MAX_SPAWN_RADIUS), edit_spawn->getSize());
	count_field->SetToolTip("Radius of the spawn area");
	subsizer->Add(count_field, wxSizerFlags(1).Expand());

	boxsizer->Add(subsizer, wxSizerFlags(1).Expand());

	topsizer->Add(boxsizer, wxSizerFlags(3).Expand().Border(wxALL, 20));

	wxSizer* std_sizer = new wxBoxSizer(wxHORIZONTAL);
	wxButton* okBtn = new wxButton(this, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	std_sizer->Add(okBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");
	cancelBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	std_sizer->Add(cancelBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	topsizer->Add(std_sizer, wxSizerFlags(0).Center().Border(wxLEFT | wxRIGHT, 20));

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_FIRE, FromDIP(wxSize(32, 32))));
	SetIcon(icon);
}

SpawnPropertiesWindow::~SpawnPropertiesWindow() {
	//
}

void SpawnPropertiesWindow::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	int new_spawnsize = count_field->GetValue();
	edit_spawn->setSize(new_spawnsize);
	EndModal(1);
}

void SpawnPropertiesWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(0);
}
