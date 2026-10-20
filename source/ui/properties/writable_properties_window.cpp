//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "map/tile.h"
#include "game/item.h"
#include "ui/dialog_util.h"
#include "ui/properties/property_validator.h"
#include "ui/properties/writable_properties_window.h"
#include "util/image_manager.h"

// ============================================================================
// Writable Properties Window

WritablePropertiesWindow::WritablePropertiesWindow(wxWindow* parent, const Map* map, const Tile* tile, Item* item, wxPoint pos) :
	ObjectPropertiesWindowBase(parent, "Writable Properties", map, tile, item, pos),
	action_id_field(nullptr),
	unique_id_field(nullptr),
	text_field(nullptr) {
	ASSERT(edit_item);

	Bind(wxEVT_BUTTON, &WritablePropertiesWindow::OnClickOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &WritablePropertiesWindow::OnClickCancel, this, wxID_CANCEL);

	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer* boxsizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Writeable Properties");

	wxFlexGridSizer* subsizer = newd wxFlexGridSizer(2, 10, 10);
	subsizer->AddGrowableCol(1);

	if (item->getClientID() != 0 && item->getClientID() != item->getID()) {
		subsizer->Add(newd wxStaticText(boxsizer->GetStaticBox(), wxID_ANY, std::format("SID: {}   CID: {}", item->getID(), item->getClientID())));
	} else {
		subsizer->Add(newd wxStaticText(boxsizer->GetStaticBox(), wxID_ANY, std::format("ID: {}", item->getID())));
	}
	subsizer->Add(newd wxStaticText(boxsizer->GetStaticBox(), wxID_ANY, "\"" + wxstr(item->getName()) + "\""));


	subsizer->Add(newd wxStaticText(boxsizer->GetStaticBox(), wxID_ANY, "Action ID"));
	action_id_field = newd wxSpinCtrl(boxsizer->GetStaticBox(), wxID_ANY, i2ws(edit_item->getActionID()), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getActionID());
	action_id_field->SetSelection(-1, -1);
	subsizer->Add(action_id_field, wxSizerFlags(1).Expand());

	subsizer->Add(newd wxStaticText(boxsizer->GetStaticBox(), wxID_ANY, "Unique ID"));
	unique_id_field = newd wxSpinCtrl(boxsizer->GetStaticBox(), wxID_ANY, i2ws(edit_item->getUniqueID()), wxDefaultPosition, FromDIP(wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getUniqueID());
	subsizer->Add(unique_id_field, wxSizerFlags(1).Expand());

	boxsizer->Add(subsizer, wxSizerFlags(1).Expand());

	wxSizer* textsizer = newd wxBoxSizer(wxVERTICAL);
	textsizer->Add(newd wxStaticText(boxsizer->GetStaticBox(), wxID_ANY, "Text"), wxSizerFlags(0).Center());
	text_field = newd wxTextCtrl(boxsizer->GetStaticBox(), wxID_ANY, wxstr(item->getText()), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
	textsizer->Add(text_field, wxSizerFlags(1).Expand());

	boxsizer->Add(textsizer, wxSizerFlags(2).Expand());

	topsizer->Add(boxsizer, wxSizerFlags(0).Expand().Border(wxALL, 20));

	wxSizer* buttonsizer = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* okBtn = newd wxButton(this, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CHECK, wxSize(16, 16)));
	buttonsizer->Add(okBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	wxButton* cancelBtn = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancelBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_XMARK, wxSize(16, 16)));
	buttonsizer->Add(cancelBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	topsizer->Add(buttonsizer, wxSizerFlags(0).Center().Border(wxLEFT | wxRIGHT, 20));

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_FILE_PEN, wxSize(32, 32)));
	SetIcon(icon);
}

WritablePropertiesWindow::~WritablePropertiesWindow() {
	//
}

void WritablePropertiesWindow::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	if (edit_item) {
		int new_uid = unique_id_field->GetValue();
		int new_aid = action_id_field->GetValue();
		std::string text = nstr(text_field->GetValue());

		if (!PropertyValidator::validateItemProperties(this, new_uid, new_aid, 0)) {
			return;
		}
		if (!PropertyValidator::validateTextProperties(this, edit_item, text)) {
			return;
		}

		edit_item->setUniqueID(new_uid);
		edit_item->setActionID(new_aid);
		edit_item->setText(text);
	}
	EndModal(1);
}

void WritablePropertiesWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(0);
}
