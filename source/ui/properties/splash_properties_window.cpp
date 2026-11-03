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

	wxSizer* topsizer = new wxBoxSizer(wxVERTICAL);
	wxSizer* boxsizer = new wxStaticBoxSizer(wxVERTICAL, this, "Splash Properties");

	wxFlexGridSizer* subsizer = new wxFlexGridSizer(2, 10, 10);
	subsizer->AddGrowableCol(1);

	if (item->getClientID() != 0 && item->getClientID() != item->getID()) {
		subsizer->Add(new wxStaticText(this, wxID_ANY, std::format("SID: {}   CID: {}", item->getID(), item->getClientID())));
	} else {
		subsizer->Add(new wxStaticText(this, wxID_ANY, std::format("ID: {}", item->getID())));
	}
	subsizer->Add(new wxStaticText(this, wxID_ANY, "\"" + wxstr(item->getName()) + "\""));


	subsizer->Add(new wxStaticText(this, wxID_ANY, "Type"));

	// Splash types
	splash_type_field = new wxChoice(this, wxID_ANY);
	if (edit_item->isFluidContainer()) {
		splash_type_field->Append(wxstr(Item::LiquidID2Name(LIQUID_NONE)), reinterpret_cast<void*>(static_cast<intptr_t>(LIQUID_NONE)));
	}

	for (const auto& entry : kFluidDropdownItems) {
		splash_type_field->Append(wxstr(entry.label), reinterpret_cast<void*>(static_cast<intptr_t>(entry.subtype)));
	}

	int selection = wxNOT_FOUND;
	if (item->getSubtype()) {
		for (unsigned int i = 0; i < splash_type_field->GetCount(); ++i) {
			if (reinterpret_cast<intptr_t>(splash_type_field->GetClientData(i)) == static_cast<intptr_t>(item->getSubtype())) {
				selection = static_cast<int>(i);
				break;
			}
		}
		if (selection != wxNOT_FOUND) {
			splash_type_field->SetSelection(selection);
		} else {
			splash_type_field->Append(wxstr(std::format("Unknown ({})", item->getSubtype())), reinterpret_cast<void*>(static_cast<intptr_t>(item->getSubtype())));
			selection = static_cast<int>(splash_type_field->GetCount()) - 1;
			splash_type_field->SetSelection(selection);
		}
	} else {
		selection = 0;
		if (reinterpret_cast<intptr_t>(splash_type_field->GetClientData(0)) < 0) {
			for (unsigned int i = 0; i < splash_type_field->GetCount(); ++i) {
				if (reinterpret_cast<intptr_t>(splash_type_field->GetClientData(i)) >= 0) {
					selection = static_cast<int>(i);
					break;
				}
			}
		}
		splash_type_field->SetSelection(selection);
	}
	last_valid_selection = selection;

	splash_type_field->Bind(wxEVT_CHOICE, [this](wxCommandEvent& event) {
		int sel = splash_type_field->GetSelection();
		if (sel != wxNOT_FOUND && reinterpret_cast<intptr_t>(splash_type_field->GetClientData(sel)) < 0) {
			splash_type_field->SetSelection(last_valid_selection);
		} else if (sel != wxNOT_FOUND) {
			last_valid_selection = sel;
		}
	});

	subsizer->Add(splash_type_field, wxSizerFlags(1).Expand());

	subsizer->Add(new wxStaticText(this, wxID_ANY, "Action ID"));
	action_id_field = new wxSpinCtrl(this, wxID_ANY, std::to_string(edit_item->getActionID()), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getActionID());
	subsizer->Add(action_id_field, wxSizerFlags(1).Expand());

	subsizer->Add(new wxStaticText(this, wxID_ANY, "Unique ID"));
	unique_id_field = new wxSpinCtrl(this, wxID_ANY, std::to_string(edit_item->getUniqueID()), wxDefaultPosition, FromDIP(wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getUniqueID());
	subsizer->Add(unique_id_field, wxSizerFlags(1).Expand());

	boxsizer->Add(subsizer, wxSizerFlags(1).Expand());

	topsizer->Add(boxsizer, wxSizerFlags(0).Expand().Border(wxALL, 20));

	wxSizer* buttonsizer = new wxBoxSizer(wxHORIZONTAL);
	wxButton* okBtn = new wxButton(this, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	buttonsizer->Add(okBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");
	cancelBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	buttonsizer->Add(cancelBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	topsizer->Add(buttonsizer, wxSizerFlags(0).Center().Border(wxLEFT | wxRIGHT, 20));

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_WATER, FromDIP(wxSize(32, 32))));
	SetIcon(icon);
}

SplashPropertiesWindow::~SplashPropertiesWindow() {
	// No cleanup needed
}

void SplashPropertiesWindow::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	if (edit_item) {
		int new_uid = unique_id_field->GetValue();
		int new_aid = action_id_field->GetValue();

		if (!PropertyValidator::validateItemProperties(this, new_uid, new_aid, 0)) {
			return;
		}

		const int selection = splash_type_field->GetSelection();
		if (selection != wxNOT_FOUND) {
			intptr_t data = reinterpret_cast<intptr_t>(splash_type_field->GetClientData(selection));
			if (data >= 0) {
				edit_item->setSubtype(static_cast<int>(data));
			}
		}
		edit_item->setUniqueID(new_uid);
		edit_item->setActionID(new_aid);
	}
	EndModal(1);
}

void SplashPropertiesWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(0);
}
