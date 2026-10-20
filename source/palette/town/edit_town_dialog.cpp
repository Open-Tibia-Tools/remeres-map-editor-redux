//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "palette/town/edit_town_dialog.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/town.h"
#include "game/house.h"
#include "ui/positionctrl.h"
#include "ui/gui.h"
#include "ui/dialog_util.h"
#include "ui/map_tab.h"
#include "util/image_manager.h"
#include "ui/managers/minimap_manager.h"

#include <wx/statbmp.h>
#include <format>

// ============================================================================
// TownIdWarningDialog
// ============================================================================
TownIdWarningDialog::TownIdWarningDialog(wxWindow* parent, uint32_t old_id, uint32_t new_id, size_t house_count) :
	wxDialog(parent, wxID_ANY, "Warning: Town ID Changed", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
	auto* topsizer = newd wxBoxSizer(wxVERTICAL);

	auto* content_sizer = newd wxBoxSizer(wxHORIZONTAL);
	auto* icon = newd wxStaticBitmap(this, wxID_ANY, IMAGE_MANAGER.GetBitmap(ICON_TRIANGLE_EXCLAMATION, wxSize(32, 32)));
	content_sizer->Add(icon, 0, wxALL | wxALIGN_TOP, 15);

	wxString msg = wxString::Format(
		"Changing Town ID from %u to %u has major consequences:\n\n"
		"  - %zu houses are currently linked to this town in the OTBM map.\n"
		"  - Server database: Player spawn points ('players' table) and NPC scripts rely on Town ID.\n\n"
		"Do you want to update all %zu associated houses to use the new Town ID %u?",
		old_id, new_id, house_count, house_count, new_id
	);
	auto* text = newd wxStaticText(this, wxID_ANY, msg);
	text->Wrap(FromDIP(450));
	content_sizer->Add(text, 1, wxTOP | wxBOTTOM | wxRIGHT | wxEXPAND, 15);
	topsizer->Add(content_sizer, 1, wxEXPAND);

	auto* btn_sizer = newd wxBoxSizer(wxHORIZONTAL);

	auto* update_btn = newd wxButton(this, wxID_YES, "Update Houses");
	update_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CHECK, wxSize(16, 16)));
	update_btn->SetToolTip("Update town ID and automatically update all linked houses");
	btn_sizer->Add(update_btn, 0, wxRIGHT, 8);

	auto* keep_btn = newd wxButton(this, wxID_NO, "Keep Old IDs");
	keep_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TRIANGLE_EXCLAMATION, wxSize(16, 16)));
	keep_btn->SetToolTip("Update town ID only (houses will remain linked to the old town ID)");
	btn_sizer->Add(keep_btn, 0, wxRIGHT, 8);

	auto* cancel_btn = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancel_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_XMARK, wxSize(16, 16)));
	cancel_btn->SetToolTip("Abort changes");
	btn_sizer->Add(cancel_btn, 0);

	topsizer->Add(btn_sizer, 0, wxALIGN_RIGHT | wxALL, 15);

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);

	update_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_YES); });
	keep_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_NO); });
	cancel_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); });

	SetIcons(IMAGE_MANAGER.GetIconBundle(ICON_CITY));
}

// ============================================================================
// TownDeleteDialog
// ============================================================================
TownDeleteDialog::TownDeleteDialog(wxWindow* parent, const std::string& name, uint32_t town_id, size_t house_count) :
	wxDialog(parent, wxID_ANY, "Delete Town & Associated Houses", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
	auto* topsizer = newd wxBoxSizer(wxVERTICAL);

	auto* content_sizer = newd wxBoxSizer(wxHORIZONTAL);
	auto* icon = newd wxStaticBitmap(this, wxID_ANY, IMAGE_MANAGER.GetBitmap(ICON_TRIANGLE_EXCLAMATION, wxSize(32, 32)));
	content_sizer->Add(icon, 0, wxALL | wxALIGN_TOP, 15);

	wxString msg = wxString::Format(
		"Town '%s' (ID: %u) has %zu house(s) assigned to it.\n\n"
		"Deleting this town will permanently delete all %zu associated houses, house tiles, and house exits!\n\n"
		"Are you sure you want to delete town '%s' and wipe all of its houses?",
		wxstr(name), town_id, house_count, house_count, wxstr(name)
	);
	auto* text = newd wxStaticText(this, wxID_ANY, msg);
	text->Wrap(FromDIP(450));
	content_sizer->Add(text, 1, wxTOP | wxBOTTOM | wxRIGHT | wxEXPAND, 15);
	topsizer->Add(content_sizer, 1, wxEXPAND);

	auto* btn_sizer = newd wxBoxSizer(wxHORIZONTAL);

	auto* delete_btn = newd wxButton(this, wxID_YES, "Delete Town and Houses");
	delete_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	delete_btn->SetToolTip("Delete the town and permanently remove all associated houses");
	btn_sizer->Add(delete_btn, 0, wxRIGHT, 8);

	auto* cancel_btn = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancel_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_XMARK, wxSize(16, 16)));
	cancel_btn->SetToolTip("Cancel and keep town");
	btn_sizer->Add(cancel_btn, 0);

	topsizer->Add(btn_sizer, 0, wxALIGN_RIGHT | wxALL, 15);

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);

	delete_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_YES); });
	cancel_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); });

	SetIcons(IMAGE_MANAGER.GetIconBundle(ICON_CITY));
}

// ============================================================================
// EditTownDialog
// ============================================================================
EditTownDialog::EditTownDialog(wxWindow* parent, Map* map, Town* town, bool is_new) :
	wxDialog(parent, wxID_ANY, is_new ? "Add Town" : "Edit Town", wxDefaultPosition, FROM_DIP(parent, wxSize(320, 290))),
	map(map),
	town(town),
	is_new_(is_new) {
	ASSERT(map);
	ASSERT(town);

	if (is_new_) {
		SetTitle("Add Town");
	} else {
		SetTitle(wxstr(std::format("Edit Town: {} (ID: {})", town->getName(), town->getID())));
	}

	auto* topsizer = newd wxBoxSizer(wxVERTICAL);

	// Town Details static box
	auto* detailsBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Town Details");
	auto* detailsGrid = newd wxFlexGridSizer(2, 5, 10);
	detailsGrid->AddGrowableCol(1);

	// Name
	town_name = wxstr(town->getName());
	detailsGrid->Add(newd wxStaticText(detailsBox->GetStaticBox(), wxID_ANY, "Name:"), wxSizerFlags(0).CenterVertical().Border(wxLEFT, 5));
	name_field = newd wxTextCtrl(detailsBox->GetStaticBox(), wxID_ANY, town_name, wxDefaultPosition, wxDefaultSize, 0, wxTextValidator(wxFILTER_ASCII, &town_name));
	name_field->SetToolTip("Town display name");
	detailsGrid->Add(name_field, wxSizerFlags(1).Expand());

	// ID
	detailsGrid->Add(newd wxStaticText(detailsBox->GetStaticBox(), wxID_ANY, "ID:"), wxSizerFlags(0).CenterVertical().Border(wxLEFT, 5));
	id_field = newd wxSpinCtrl(detailsBox->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 0xFFFF, static_cast<int>(town->getID()));
	id_field->SetToolTip("Town ID (1-65535). Warning: changing existing ID affects linked houses and server player spawn data.");
	detailsGrid->Add(id_field, wxSizerFlags(1).Expand());

	detailsBox->Add(detailsGrid, 1, wxEXPAND | wxALL, 5);
	topsizer->Add(detailsBox, 0, wxEXPAND | wxALL, 10);

	// Temple Position
	temple_position = newd PositionCtrl(this, "Temple Position", 0, 0, 0, map->getWidth(), map->getHeight());
	temple_position->SetPosition(town->getTemplePosition());

	pick_map_button = newd wxButton(temple_position->GetStaticBox(), wxID_ANY, "Pick from Map");
	pick_map_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CROSSHAIRS));
	pick_map_button->SetToolTip("Click to pick temple position directly on the map");
	temple_position->Add(pick_map_button, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	topsizer->Add(temple_position, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

	// OK / Cancel buttons
	auto* btnSizer = newd wxBoxSizer(wxHORIZONTAL);
	auto* okBtn = newd wxButton(this, wxID_OK, is_new_ ? "Create Town" : "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	okBtn->SetToolTip(is_new_ ? "Create town" : "Save changes");
	btnSizer->Add(okBtn, wxSizerFlags(1).Center());

	auto* cancelBtn = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancelBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	cancelBtn->SetToolTip("Cancel");
	btnSizer->Add(cancelBtn, wxSizerFlags(1).Center().Border(wxLEFT, 10));

	topsizer->Add(btnSizer, 0, wxCENTER | wxALL, 10);

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);

	pick_map_button->Bind(wxEVT_BUTTON, &EditTownDialog::OnClickPickFromMap, this);
	okBtn->Bind(wxEVT_BUTTON, &EditTownDialog::OnClickOK, this);
	cancelBtn->Bind(wxEVT_BUTTON, &EditTownDialog::OnClickCancel, this);

	SetIcons(IMAGE_MANAGER.GetIconBundle(ICON_CITY));
}

EditTownDialog::~EditTownDialog() = default;

std::string EditTownDialog::GetTownName() const {
	return name_field ? name_field->GetValue().ToStdString() : "";
}

uint32_t EditTownDialog::GetTownID() const {
	return id_field ? static_cast<uint32_t>(id_field->GetValue()) : 0;
}

Position EditTownDialog::GetTemplePosition() const {
	return temple_position ? temple_position->GetPosition() : Position();
}

void EditTownDialog::SetTemplePosition(const Position& pos) {
	if (temple_position && pos.isValid()) {
		temple_position->SetPosition(pos);
	}
}

void EditTownDialog::OnClickPickFromMap(wxCommandEvent& WXUNUSED(event)) {
	EndModal(DIALOG_PICK_TEMPLE);
}

void EditTownDialog::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	if (!Validate() || !TransferDataFromWindow()) {
		return;
	}

	wxString new_name = name_field->GetValue().Trim();
	if (new_name.IsEmpty()) {
		DialogUtil::PopupDialog(this, "Error", "Town name cannot be empty.", wxOK);
		return;
	}

	Position new_temple = temple_position->GetPosition();
	if (!new_temple.isValid() || new_temple.x > map->getWidth() || new_temple.y > map->getHeight()) {
		DialogUtil::PopupDialog(this, "Error", "Invalid temple position specified.", wxOK);
		return;
	}

	uint32_t new_id = static_cast<uint32_t>(id_field->GetValue());

	if (is_new_) {
		// Check for duplicate ID
		if (map->towns.getTown(new_id) != nullptr) {
			DialogUtil::PopupDialog(this, "Error", wxstr(std::format("A town with ID {} already exists.", new_id)), wxOK);
			return;
		}

		town->setName(nstr(new_name));
		town->setTemplePosition(new_temple);
		town->setID(new_id);
		EndModal(wxID_OK);
		return;
	}

	// Editing existing town
	uint32_t old_id = town->getID();
	bool cascade_houses = false;

	if (new_id != old_id) {
		if (map->towns.getTown(new_id) != nullptr) {
			DialogUtil::PopupDialog(this, "Error", wxstr(std::format("A town with ID {} already exists.", new_id)), wxOK);
			return;
		}

		size_t house_count = 0;
		for (const auto& [_, house] : map->houses) {
			if (house && house->townid == old_id) {
				++house_count;
			}
		}

		if (house_count > 0) {
			TownIdWarningDialog warn_dlg(this, old_id, new_id, house_count);
			int answer = warn_dlg.ShowModal();
			if (answer == wxID_CANCEL) {
				return;
			}
			if (answer == wxID_YES) {
				cascade_houses = true;
			}
		}

		if (!map->towns.changeId(town, new_id)) {
			DialogUtil::PopupDialog(this, "Error", "Failed to change Town ID.", wxOK);
			return;
		}

		if (cascade_houses) {
			map->updateTownIdForHouses(old_id, new_id);
		}
	}

	// Update Name
	town->setName(nstr(new_name));

	// Update Temple Position
	Position old_temple = town->getTemplePosition();
	if (old_temple != new_temple) {
		if (old_temple != Position()) {
			if (auto* tile = map->getTile(old_temple)) {
				if (auto* loc = tile->getLocation()) {
					loc->decreaseTownCount();
				}
			}
		}

		if (auto* tile = map->getOrCreateTile(new_temple)) {
			if (auto* loc = tile->getLocation()) {
				loc->increaseTownCount();
			}
		}

		town->setTemplePosition(new_temple);

		if (old_temple != Position() && old_temple.isValid()) {
			map->getChangeTracker().markTileDirty(old_temple);
			g_minimap.MarkTileDirty(*map, old_temple);
		}
		if (new_temple.isValid()) {
			map->getChangeTracker().markTileDirty(new_temple);
			g_minimap.MarkTileDirty(*map, new_temple);
		}
	}

	map->doChange();
	g_gui.RefreshPalettes();
	g_gui.RefreshView();
	EndModal(wxID_OK);
}

void EditTownDialog::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(wxID_CANCEL);
}
