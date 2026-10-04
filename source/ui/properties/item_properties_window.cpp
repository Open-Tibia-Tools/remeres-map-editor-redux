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

namespace {
	bool supportsClassificationTiers() {
		return g_item_definitions.MajorVersion > 3 || (g_item_definitions.MajorVersion == 3 && g_item_definitions.MinorVersion >= 60);
	}
}

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

	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);

	wxString description;
	if (dynamic_cast<Door*>(edit_item)) {
		ASSERT(tile_parent);
		description = "Door Properties";
	} else if (dynamic_cast<Teleport*>(edit_item)) {
		description = "Teleport Properties";
	} else {
		description = "Item Properties";
	}

	wxStaticBoxSizer* boxsizer = newd wxStaticBoxSizer(wxVERTICAL, this, description);
	wxFlexGridSizer* subsizer = newd wxFlexGridSizer(2, 10, 10);
	subsizer->AddGrowableCol(1);

	wxWindow* static_box = boxsizer->GetStaticBox();
	createHeaderFields(subsizer, static_box);
	createGenericFields(subsizer, static_box);
	createClassificationFields(subsizer, static_box);
	createDoorFields(subsizer, static_box);
	createTeleportFields(subsizer, static_box);

	boxsizer->Add(subsizer, wxSizerFlags(1).Expand());
	topsizer->Add(boxsizer, wxSizerFlags(0).Expand().Border(wxLEFT | wxRIGHT, 20));

	wxSizer* subsizer_btn = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* okBtn = newd wxButton(this, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CHECK, wxSize(16, 16)));
	subsizer_btn->Add(okBtn, wxSizerFlags(1).Center().Border(wxTOP | wxBOTTOM, 10));
	wxButton* cancelBtn = newd wxButton(this, wxID_CANCEL, "Cancel");
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
	subsizer->Add(newd wxStaticText(parent, wxID_ANY, "ID " + i2ws(edit_item->getID())));
	subsizer->Add(newd wxStaticText(parent, wxID_ANY, "\"" + wxstr(edit_item->getName()) + "\""));
}

void ItemPropertiesWindow::createGenericFields(wxFlexGridSizer* subsizer, wxWindow* parent) {
	subsizer->Add(newd wxStaticText(parent, wxID_ANY, (edit_item->isCharged() ? "Charges" : "Count")));
	int max_count = 100;
	if (edit_item->isClientCharged()) {
		max_count = 250;
	}
	if (edit_item->isExtraCharged()) {
		max_count = 65500;
	}
	count_field = newd wxSpinCtrl(parent, wxID_ANY, i2ws(edit_item->getCount()), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, max_count, edit_item->getCount());
	if (!edit_item->isStackable() && !edit_item->isCharged()) {
		count_field->Enable(false);
	}
	subsizer->Add(count_field, wxSizerFlags(1).Expand());

	subsizer->Add(newd wxStaticText(parent, wxID_ANY, "Action ID"));
	action_id_field = newd wxSpinCtrl(parent, wxID_ANY, i2ws(edit_item->getActionID()), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getActionID());
	subsizer->Add(action_id_field, wxSizerFlags(1).Expand());

	subsizer->Add(newd wxStaticText(parent, wxID_ANY, "Unique ID"));
	unique_id_field = newd wxSpinCtrl(parent, wxID_ANY, i2ws(edit_item->getUniqueID()), wxDefaultPosition, FROM_DIP(this, wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, 0xFFFF, edit_item->getUniqueID());
	subsizer->Add(unique_id_field, wxSizerFlags(1).Expand());
}

void ItemPropertiesWindow::createClassificationFields(wxFlexGridSizer* subsizer, wxWindow* parent) {
	// item classification (12.81+)
	if (supportsClassificationTiers() && (edit_item->getClassification() > 0 || edit_item->isWeapon() || edit_item->isWearableEquipment())) {
		subsizer->Add(newd wxStaticText(parent, wxID_ANY, "Classification"));
		subsizer->Add(newd wxStaticText(parent, wxID_ANY, i2ws(edit_item->getClassification())));

		// item iter
		subsizer->Add(newd wxStaticText(parent, wxID_ANY, "Tier"));
		tier_field = newd wxSpinCtrl(parent, wxID_ANY, i2ws(edit_item->getTier()), wxDefaultPosition, FROM_DIP(this, wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, 0xFF, edit_item->getTier());
		subsizer->Add(tier_field, wxSizerFlags(1).Expand());
	}
}

void ItemPropertiesWindow::createDoorFields(wxFlexGridSizer* subsizer, wxWindow* parent) {
	if (Door* door = dynamic_cast<Door*>(edit_item)) {
		subsizer->Add(newd wxStaticText(parent, wxID_ANY, "Door ID"));
		door_id_field = newd wxSpinCtrl(parent, wxID_ANY, i2ws(door->getDoorID()), wxDefaultPosition, FROM_DIP(this, wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, 0xFF, door->getDoorID());
		if (!edit_tile || !edit_tile->isHouseTile() || !door->isRealDoor()) {
			door_id_field->Disable();
		}
		subsizer->Add(door_id_field, wxSizerFlags(1).Expand());
	}
}

void ItemPropertiesWindow::createTeleportFields(wxFlexGridSizer* subsizer, wxWindow* parent) {
	if (Teleport* teleport = dynamic_cast<Teleport*>(edit_item)) {
		subsizer->Add(newd wxStaticText(parent, wxID_ANY, "Destination"));

		wxSizer* possizer = newd wxBoxSizer(wxHORIZONTAL);
		x_field = newd wxSpinCtrl(parent, wxID_ANY, i2ws(teleport->getX()), wxDefaultPosition, FROM_DIP(this, wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, edit_map->getWidth(), teleport->getX());
		x_field->Bind(wxEVT_CHAR, &ItemPropertiesWindow::OnChar, this);
		possizer->Add(x_field, wxSizerFlags(3).Expand());
		y_field = newd wxSpinCtrl(parent, wxID_ANY, i2ws(teleport->getY()), wxDefaultPosition, FROM_DIP(this, wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, edit_map->getHeight(), teleport->getY());
		y_field->Bind(wxEVT_CHAR, &ItemPropertiesWindow::OnChar, this);
		possizer->Add(y_field, wxSizerFlags(3).Expand());
		z_field = newd wxSpinCtrl(parent, wxID_ANY, i2ws(teleport->getZ()), wxDefaultPosition, FROM_DIP(this, wxSize(-1, 20)), wxSP_ARROW_KEYS, 0, MAP_MAX_LAYER, teleport->getZ());
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
