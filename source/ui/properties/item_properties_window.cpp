//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "map/tile.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "game/town.h"
#include "game/house.h"
#include "map/map.h"
#include "editor/editor.h"
#include "game/creature.h"

#include "ui/gui.h"
#include "ui/dialog_util.h"
#include "app/application.h"
#include "ui/properties/item_properties_window.h"
#include "ui/properties/property_validator.h"
#include "ui/properties/property_applier.h"
#include "ui/properties/teleport_service.h"
#include "item_definitions/core/item_definition_store.h"
#include "util/image_manager.h"

// ============================================================================
// Item Properties Window

ItemPropertiesWindow::ItemPropertiesWindow(wxWindow* win_parent, const Map* map, const Tile* tile_parent, Item* item, wxPoint pos) :
	ObjectPropertiesWindowBase(win_parent, "Item Properties", map, tile_parent, item, pos),
	count_field(nullptr),
	action_id_field(nullptr),
	unique_id_field(nullptr),
	door_id_field(nullptr),
	tier_field(nullptr) {
	ASSERT(edit_item);

	Bind(wxEVT_SET_FOCUS, &ItemPropertiesWindow::OnFocusChange, this);
	Bind(wxEVT_BUTTON, &ItemPropertiesWindow::OnClickOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &ItemPropertiesWindow::OnClickCancel, this, wxID_CANCEL);

	wxSizer* topsizer = new wxBoxSizer(wxVERTICAL);

	wxString description;
	if (dynamic_cast<Door*>(edit_item)) {
		ASSERT(tile_parent);
		description = "Door Properties";
	} else if (dynamic_cast<Teleport*>(edit_item)) {
		description = "Teleport Properties";
	} else {
		description = "Item Properties";
	}

	wxStaticBoxSizer* boxsizer = new wxStaticBoxSizer(wxVERTICAL, this, description);
	wxFlexGridSizer* subsizer = new wxFlexGridSizer(2, 10, 10);
	subsizer->AddGrowableCol(1);

	wxWindow* static_box = boxsizer->GetStaticBox();
	createHeaderFields(subsizer, static_box);
	createGenericFields(subsizer, static_box);
	createClassificationFields(subsizer, static_box);
	createDoorFields(subsizer, static_box);
	createTeleportFields(subsizer, static_box);

	boxsizer->Add(subsizer, wxSizerFlags(1).Expand());
	topsizer->Add(boxsizer, wxSizerFlags(0).Expand().Border(wxLEFT | wxRIGHT, 20));

	wxSizer* subsizer_btn = new wxBoxSizer(wxHORIZONTAL);
	wxButton* okBtn = new wxButton(this, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CHECK, wxSize(16, 16)));
	subsizer_btn->Add(okBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");
	cancelBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_XMARK, wxSize(16, 16)));
	subsizer_btn->Add(cancelBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	topsizer->Add(subsizer_btn, wxSizerFlags(0).Center().Border(wxLEFT | wxRIGHT, 20));

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_GEAR, wxSize(32, 32)));
	SetIcon(icon);
}

void ItemPropertiesWindow::createHeaderFields(wxFlexGridSizer* subsizer, wxWindow* parent) {
	if (edit_item->getClientID() != 0 && edit_item->getClientID() != edit_item->getID()) {
		subsizer->Add(new wxStaticText(parent, wxID_ANY, std::format("SID: {}   CID: {}", edit_item->getID(), edit_item->getClientID())));
	} else {
		subsizer->Add(new wxStaticText(parent, wxID_ANY, std::format("ID: {}", edit_item->getID())));
	}
	subsizer->Add(new wxStaticText(parent, wxID_ANY, "\"" + wxstr(edit_item->getName()) + "\""));
}


void ItemPropertiesWindow::createGenericFields(wxFlexGridSizer* subsizer, wxWindow* parent) {
	subsizer->Add(new wxStaticText(parent, wxID_ANY, (edit_item->isCharged() ? "Charges" : "Count")));
	int max_count = 100;
	if (edit_item->isClientCharged()) {
		max_count = 250;
	}
	if (edit_item->isExtraCharged()) {
		max_count = 65500;
	}
	count_field = new wxSpinCtrl(parent, wxID_ANY, std::to_string(edit_item->getCount()), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, max_count, edit_item->getCount());
	if (!edit_item->isStackable() && !edit_item->isCharged()) {
		count_field->Enable(false);
	}
	subsizer->Add(count_field, wxSizerFlags(1).Expand());

	subsizer->Add(new wxStaticText(parent, wxID_ANY, "Action ID"));
	action_id_field = new wxSpinCtrl(parent, wxID_ANY, std::to_string(edit_item->getActionID()), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getActionID());
	subsizer->Add(action_id_field, wxSizerFlags(1).Expand());

	subsizer->Add(new wxStaticText(parent, wxID_ANY, "Unique ID"));
	unique_id_field = new wxSpinCtrl(parent, wxID_ANY, std::to_string(edit_item->getUniqueID()), wxDefaultPosition, FromDIP(wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getUniqueID());
	subsizer->Add(unique_id_field, wxSizerFlags(1).Expand());
}

void ItemPropertiesWindow::createClassificationFields(wxFlexGridSizer* subsizer, wxWindow* parent) {
	// item classification (12.81+)
	if (supportsClassificationTiers() && (edit_item->getClassification() > 0 || edit_item->isWeapon() || edit_item->isWearableEquipment())) {
		subsizer->Add(new wxStaticText(parent, wxID_ANY, "Classification"));
		subsizer->Add(new wxStaticText(parent, wxID_ANY, std::to_string(edit_item->getClassification())));

		// item iter
		subsizer->Add(new wxStaticText(parent, wxID_ANY, "Tier"));
		tier_field = new wxSpinCtrl(parent, wxID_ANY, std::to_string(edit_item->getTier()), wxDefaultPosition, FromDIP(wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, 0xFF, edit_item->getTier());
		subsizer->Add(tier_field, wxSizerFlags(1).Expand());
	}
}

void ItemPropertiesWindow::createDoorFields(wxFlexGridSizer* subsizer, wxWindow* parent) {
	if (Door* door = dynamic_cast<Door*>(edit_item)) {
		subsizer->Add(new wxStaticText(parent, wxID_ANY, "Door ID"));
		door_id_field = new wxSpinCtrl(parent, wxID_ANY, std::to_string(door->getDoorID()), wxDefaultPosition, FromDIP(wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, 0xFF, door->getDoorID());
		if (!edit_tile || !edit_tile->isHouseTile() || !door->isRealDoor()) {
			door_id_field->Disable();
		}
		subsizer->Add(door_id_field, wxSizerFlags(1).Expand());
	}
}

void ItemPropertiesWindow::createTeleportFields(wxFlexGridSizer* subsizer, wxWindow* parent) {
	if (Teleport* teleport = dynamic_cast<Teleport*>(edit_item)) {
		subsizer->Add(new wxStaticText(parent, wxID_ANY, "Destination"));

		wxSizer* possizer = new wxBoxSizer(wxHORIZONTAL);
		x_field = new wxSpinCtrl(parent, wxID_ANY, std::to_string(teleport->getX()), wxDefaultPosition, FromDIP(wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, edit_map->getWidth(), teleport->getX());
		x_field->Bind(wxEVT_CHAR, &ItemPropertiesWindow::OnChar, this);
		possizer->Add(x_field, wxSizerFlags(3).Expand());
		y_field = new wxSpinCtrl(parent, wxID_ANY, std::to_string(teleport->getY()), wxDefaultPosition, FromDIP(wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, edit_map->getHeight(), teleport->getY());
		y_field->Bind(wxEVT_CHAR, &ItemPropertiesWindow::OnChar, this);
		possizer->Add(y_field, wxSizerFlags(3).Expand());
		z_field = new wxSpinCtrl(parent, wxID_ANY, std::to_string(teleport->getZ()), wxDefaultPosition, FromDIP(wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, MAP_MAX_LAYER, teleport->getZ());
		z_field->Bind(wxEVT_CHAR, &ItemPropertiesWindow::OnChar, this);
		possizer->Add(z_field, wxSizerFlags(2).Expand());

		subsizer->Add(possizer, wxSizerFlags(1).Expand());
	}
}

ItemPropertiesWindow::~ItemPropertiesWindow() {
	//
}

void ItemPropertiesWindow::OnFocusChange(wxFocusEvent& event) {
	wxWindow* win = event.GetWindow();
	if (wxSpinCtrl* spin = dynamic_cast<wxSpinCtrl*>(win)) {
		spin->SetSelection(-1, -1);
	} else if (wxTextCtrl* text = dynamic_cast<wxTextCtrl*>(win)) {
		text->SetSelection(-1, -1);
	}
}

void ItemPropertiesWindow::OnChar(wxKeyEvent& evt) {
	if (evt.GetKeyCode() == WXK_CONTROL_V) {
		int x, y, z;
		const Editor* const editor = g_gui.GetCurrentEditor();
		if (TeleportService::handlePositionPaste(x, y, z, editor->map.getWidth(), editor->map.getHeight())) {
			x_field->SetValue(x);
			y_field->SetValue(y);
			z_field->SetValue(z);
			return;
		}
	}

	evt.Skip();
}

void ItemPropertiesWindow::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	if (edit_item) {
		// Normal item
		Door* door = dynamic_cast<Door*>(edit_item);
		Teleport* teleport = dynamic_cast<Teleport*>(edit_item);

		int new_uid = unique_id_field->GetValue();
		int new_aid = action_id_field->GetValue();
		int new_count = count_field ? count_field->GetValue() : 1;
		int new_tier = tier_field ? tier_field->GetValue() : 0;

		Position new_dest;
		if (teleport) {
			new_dest = Position(x_field->GetValue(), y_field->GetValue(), z_field->GetValue());
		}
		uint8_t new_door_id = 0;
		if (door) {
			new_door_id = door_id_field->GetValue();
		}

		if (!PropertyValidator::validateItemProperties(this, new_uid, new_aid, new_tier)) {
			return;
		}

		if (door && !PropertyValidator::validateDoorProperties(this, edit_map, edit_tile, door, new_door_id)) {
			return;
		}

		if (teleport && !PropertyValidator::validateTeleportProperties(this, edit_map, new_dest)) {
			return;
		}

		// Done validating, set the values.
		PropertyApplier::applyItemProperties(edit_item, new_count, new_uid, new_aid, new_tier);
		if (door) {
			PropertyApplier::applyDoorProperties(door, new_door_id);
		}
		if (teleport) {
			PropertyApplier::applyTeleportProperties(teleport, new_dest);
		}
	}
	EndModal(1);
}

void ItemPropertiesWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(0);
}
