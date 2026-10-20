//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "map/tile.h"
#include "game/item.h"
#include "ui/dialog_util.h"
#include "ui/properties/property_validator.h"
#include "ui/properties/splash_properties_window.h"
#include "util/image_manager.h"

// ============================================================================
// Splash Properties Window

SplashPropertiesWindow::SplashPropertiesWindow(wxWindow* parent, const Map* map, const Tile* tile, Item* item, wxPoint pos) :
	ObjectPropertiesWindowBase(parent, "Splash Properties", map, tile, item, pos),
	action_id_field(nullptr),
	unique_id_field(nullptr),
	splash_type_field(nullptr) {
	ASSERT(edit_item);

	Bind(wxEVT_BUTTON, &SplashPropertiesWindow::OnClickOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &SplashPropertiesWindow::OnClickCancel, this, wxID_CANCEL);

	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);
	wxSizer* boxsizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Splash Properties");

	wxFlexGridSizer* subsizer = newd wxFlexGridSizer(2, 10, 10);
	subsizer->AddGrowableCol(1);

	if (item->getClientID() != 0 && item->getClientID() != item->getID()) {
		subsizer->Add(newd wxStaticText(this, wxID_ANY, std::format("SID: {}   CID: {}", item->getID(), item->getClientID())));
	} else {
		subsizer->Add(newd wxStaticText(this, wxID_ANY, std::format("ID: {}", item->getID())));
	}
	subsizer->Add(newd wxStaticText(this, wxID_ANY, "\"" + wxstr(item->getName()) + "\""));


	subsizer->Add(newd wxStaticText(this, wxID_ANY, "Type"));

	// Splash types
	splash_type_field = newd wxChoice(this, wxID_ANY);
	if (edit_item->isFluidContainer()) {
		splash_type_field->Append(wxstr(Item::LiquidID2Name(LIQUID_NONE)), reinterpret_cast<void*>(static_cast<intptr_t>(LIQUID_NONE)));
	}

	for (SplashType splashType = LIQUID_FIRST; splashType != LIQUID_LAST; ++splashType) {
		splash_type_field->Append(wxstr(Item::LiquidID2Name(splashType)), reinterpret_cast<void*>(static_cast<intptr_t>(splashType)));
	}

	if (item->getSubtype()) {
		const std::string_view text = Item::LiquidID2Name(item->getSubtype());
		if (text == "Unknown") {
			splash_type_field->Append(wxstr(Item::LiquidID2Name(LIQUID_NONE)), reinterpret_cast<void*>(static_cast<intptr_t>(LIQUID_NONE)));
		}
		splash_type_field->SetStringSelection(wxstr(text));
	} else {
		splash_type_field->SetSelection(0);
	}

	subsizer->Add(splash_type_field, wxSizerFlags(1).Expand());

	subsizer->Add(newd wxStaticText(this, wxID_ANY, "Action ID"));
	action_id_field = newd wxSpinCtrl(this, wxID_ANY, i2ws(edit_item->getActionID()), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getActionID());
	subsizer->Add(action_id_field, wxSizerFlags(1).Expand());

	subsizer->Add(newd wxStaticText(this, wxID_ANY, "Unique ID"));
	unique_id_field = newd wxSpinCtrl(this, wxID_ANY, i2ws(edit_item->getUniqueID()), wxDefaultPosition, FromDIP(wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getUniqueID());
	subsizer->Add(unique_id_field, wxSizerFlags(1).Expand());

	boxsizer->Add(subsizer, wxSizerFlags(1).Expand());

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
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_WATER, wxSize(32, 32)));
	SetIcon(icon);
}

SplashPropertiesWindow::~SplashPropertiesWindow() {
	// No cleanup needed
}

void SplashPropertiesWindow::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	if (edit_item) {
		int new_uid = unique_id_field->GetValue();
		int new_aid = action_id_field->GetValue();
		int new_type = static_cast<int>(reinterpret_cast<intptr_t>(splash_type_field->GetClientData(splash_type_field->GetSelection())));

		if (!PropertyValidator::validateItemProperties(this, new_uid, new_aid, 0)) {
			return;
		}

		if (new_type) {
			edit_item->setSubtype(new_type);
		}
		edit_item->setUniqueID(new_uid);
		edit_item->setActionID(new_aid);
	}
	EndModal(1);
}

void SplashPropertiesWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(0);
}
