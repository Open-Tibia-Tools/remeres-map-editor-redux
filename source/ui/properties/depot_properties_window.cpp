//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "map/tile.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "game/town.h"
#include "map/map.h"
#include "ui/dialog_util.h"
#include "ui/properties/depot_properties_window.h"
#include "util/image_manager.h"

// ============================================================================
// Depot Properties Window

DepotPropertiesWindow::DepotPropertiesWindow(wxWindow* parent, const Map* map, const Tile* tile, Item* item, wxPoint pos) :
	ObjectPropertiesWindowBase(parent, "Depot Properties", map, tile, item, pos),
	depot_id_field(nullptr) {
	Depot* depot = dynamic_cast<Depot*>(edit_item);
	ASSERT(depot);

	Bind(wxEVT_BUTTON, &DepotPropertiesWindow::OnClickOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &DepotPropertiesWindow::OnClickCancel, this, wxID_CANCEL);

	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);
	wxSizer* boxsizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Depot Properties");
	wxFlexGridSizer* subsizer = newd wxFlexGridSizer(2, 10, 10);

	subsizer->AddGrowableCol(1);
	if (item->getClientID() != 0 && item->getClientID() != item->getID()) {
		subsizer->Add(newd wxStaticText(this, wxID_ANY, std::format("SID: {}   CID: {}", item->getID(), item->getClientID())));
	} else {
		subsizer->Add(newd wxStaticText(this, wxID_ANY, std::format("ID: {}", item->getID())));
	}
	subsizer->Add(newd wxStaticText(this, wxID_ANY, "\"" + wxstr(item->getName()) + "\""));


	const Towns& towns = map->towns;
	subsizer->Add(newd wxStaticText(this, wxID_ANY, "Depot ID"));
	depot_id_field = newd wxChoice(this, wxID_ANY);
	int to_select_index = 0;
	if (towns.count() > 0) {
		bool found = false;
		for (TownMap::const_iterator town_iter = towns.begin();
			 town_iter != towns.end();
			 ++town_iter) {
			if (town_iter->second->getID() == depot->getDepotID()) {
				found = true;
			}
			depot_id_field->Append(wxstr(town_iter->second->getName()), reinterpret_cast<void*>(static_cast<intptr_t>(town_iter->second->getID())));
			if (!found) {
				++to_select_index;
			}
		}
		if (!found) {
			if (depot->getDepotID() != 0) {
				depot_id_field->Append("Undefined Town (id:" + i2ws(depot->getDepotID()) + ")", reinterpret_cast<void*>(static_cast<intptr_t>(depot->getDepotID())));
			}
		}
	}
	depot_id_field->Append("No Town", reinterpret_cast<void*>(static_cast<intptr_t>(0)));
	if (depot->getDepotID() == 0) {
		to_select_index = depot_id_field->GetCount() - 1;
	}
	depot_id_field->SetSelection(to_select_index);

	subsizer->Add(depot_id_field, wxSizerFlags(1).Expand());

	boxsizer->Add(subsizer, wxSizerFlags(5).Expand());
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
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_BOX, wxSize(32, 32)));
	SetIcon(icon);
}

DepotPropertiesWindow::~DepotPropertiesWindow() {
	// No cleanup needed
}

void DepotPropertiesWindow::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	if (Depot* depot = dynamic_cast<Depot*>(edit_item)) {
		int new_depotid = static_cast<int>(reinterpret_cast<intptr_t>(depot_id_field->GetClientData(depot_id_field->GetSelection())));
		depot->setDepotID(new_depotid);
	}
	EndModal(1);
}

void DepotPropertiesWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(0);
}
