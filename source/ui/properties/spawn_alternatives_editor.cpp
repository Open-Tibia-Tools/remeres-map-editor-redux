#include "app/main.h"
#include "ui/properties/spawn_alternatives_editor.h"
#include "ui/dialogs/monster_select_dialog.h"
#include "game/creature.h"
#include "util/image_manager.h"
#include <wx/sizer.h>
#include <wx/settings.h>
#include <algorithm>
#include <format>

SpawnAlternativesEditor::SpawnAlternativesEditor(wxWindow* parent) :
	wxPanel(parent, wxID_ANY),
	list_ctrl(nullptr),
	add_btn(nullptr),
	remove_btn(nullptr),
	chance_spin(nullptr),
	total_chance_label(nullptr) {

	wxStaticBoxSizer* box_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Varying Spawn");

	list_ctrl = newd wxListCtrl(box_sizer->GetStaticBox(), wxID_ANY, wxDefaultPosition, FROM_DIP(this, wxSize(280, 120)), wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_THEME);
	list_ctrl->InsertColumn(0, "Monster", wxLIST_FORMAT_LEFT, FROM_DIP(this, 170));
	list_ctrl->InsertColumn(1, "Chance", wxLIST_FORMAT_RIGHT, FROM_DIP(this, 80));
	box_sizer->Add(list_ctrl, 1, wxEXPAND | wxALL, 4);

	wxBoxSizer* controls_sizer = newd wxBoxSizer(wxHORIZONTAL);
	add_btn = newd wxButton(box_sizer->GetStaticBox(), wxID_ANY, "Add...", wxDefaultPosition, wxDefaultSize);
	add_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	controls_sizer->Add(add_btn, 0, wxRIGHT, 4);

	remove_btn = newd wxButton(box_sizer->GetStaticBox(), wxID_ANY, "Remove", wxDefaultPosition, wxDefaultSize);
	remove_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	controls_sizer->Add(remove_btn, 0, wxRIGHT, 8);

	controls_sizer->AddStretchSpacer(1);

	wxStaticText* chance_lbl = newd wxStaticText(box_sizer->GetStaticBox(), wxID_ANY, "Chance:");
	controls_sizer->Add(chance_lbl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

	chance_spin = newd wxSpinCtrl(box_sizer->GetStaticBox(), wxID_ANY, "100", wxDefaultPosition, FROM_DIP(this, wxSize(70, -1)), wxSP_ARROW_KEYS, 1, 100, 100);
	controls_sizer->Add(chance_spin, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);

	wxStaticText* percent_lbl = newd wxStaticText(box_sizer->GetStaticBox(), wxID_ANY, "%");
	controls_sizer->Add(percent_lbl, 0, wxALIGN_CENTER_VERTICAL);

	box_sizer->Add(controls_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

	total_chance_label = newd wxStaticText(box_sizer->GetStaticBox(), wxID_ANY, "Total chance: 100%");
	box_sizer->Add(total_chance_label, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);

	wxBoxSizer* root_sizer = newd wxBoxSizer(wxVERTICAL);
	root_sizer->Add(box_sizer, 1, wxEXPAND);
	SetSizer(root_sizer);

	list_ctrl->Bind(wxEVT_LIST_ITEM_SELECTED, &SpawnAlternativesEditor::OnItemSelected, this);
	list_ctrl->Bind(wxEVT_LIST_ITEM_DESELECTED, &SpawnAlternativesEditor::OnItemDeselected, this);
	chance_spin->Bind(wxEVT_SPINCTRL, &SpawnAlternativesEditor::OnChanceChanged, this);
	add_btn->Bind(wxEVT_BUTTON, &SpawnAlternativesEditor::OnClickAdd, this);
	remove_btn->Bind(wxEVT_BUTTON, &SpawnAlternativesEditor::OnClickRemove, this);

	UpdateButtonStates();
	UpdateTotalChanceLabel();
}

void SpawnAlternativesEditor::SetCreature(Creature* creature) {
	variants.clear();
	selected_index = -1;

	if (!creature || creature->isNpc()) {
		Hide();
		return;
	}
	Show();

	uint32_t primaryChance = (creature->getAlternatives().empty() && creature->getWeight() == 0) ? 100 : creature->getWeight();
	variants.push_back({ creature->getName(), primaryChance });

	for (const auto& alt : creature->getAlternatives()) {
		variants.push_back({ alt.name, alt.weight });
	}

	RefreshList();
	if (!variants.empty()) {
		selected_index = 0;
		list_ctrl->SetItemState(0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
	}
	UpdateButtonStates();
	UpdateTotalChanceLabel();
}

void SpawnAlternativesEditor::ApplyToCreature(Creature* creature) {
	if (!creature || variants.empty()) {
		return;
	}

	if (variants.size() == 1) {
		creature->setTypeName(variants[0].name);
		creature->setWeight(0);
		creature->clearAlternatives();
	} else {
		creature->setTypeName(variants[0].name);
		creature->setWeight(variants[0].chance);
		creature->setDirection(NORTH); // TFS varying monster spawns are locked to NORTH
		creature->clearAlternatives();

		for (size_t i = 1; i < variants.size(); ++i) {
			creature->addAlternative({
				variants[i].name,
				creature->getSpawnTime(),
				NORTH,
				variants[i].chance
			});
		}
	}
}

void SpawnAlternativesEditor::RefreshList() {
	list_ctrl->Freeze();
	list_ctrl->DeleteAllItems();

	for (size_t i = 0; i < variants.size(); ++i) {
		long idx = list_ctrl->InsertItem(static_cast<long>(i), wxString(variants[i].name));
		list_ctrl->SetItem(idx, 1, wxString::Format("%u%%", variants[i].chance));
	}
	list_ctrl->Thaw();
}

void SpawnAlternativesEditor::UpdateTotalChanceLabel() {
	uint32_t total = 0;
	for (const auto& v : variants) {
		total += v.chance;
	}

	if (variants.size() <= 1) {
		total_chance_label->SetLabelText("Total chance: 100%");
		total_chance_label->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
	} else if (total == 100) {
		total_chance_label->SetLabelText("Total chance: 100%");
		total_chance_label->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
	} else {
		total_chance_label->SetLabelText(wxString::Format("Total chance: %u%% (Warning: must equal 100%%)", total));
		total_chance_label->SetForegroundColour(*wxRED);
	}
	total_chance_label->Refresh();
}

void SpawnAlternativesEditor::UpdateButtonStates() {
	if (selected_index >= 0 && selected_index < static_cast<int>(variants.size())) {
		chance_spin->Enable(true);
		chance_spin->SetValue(static_cast<int>(variants[selected_index].chance));
		remove_btn->Enable(variants.size() > 1);
	} else {
		chance_spin->Enable(false);
		remove_btn->Enable(false);
	}
}

void SpawnAlternativesEditor::OnItemSelected(wxListEvent& event) {
	selected_index = static_cast<int>(event.GetIndex());
	UpdateButtonStates();
}

void SpawnAlternativesEditor::OnItemDeselected(wxListEvent& WXUNUSED(event)) {
	selected_index = -1;
	UpdateButtonStates();
}

void SpawnAlternativesEditor::OnChanceChanged(wxSpinEvent& WXUNUSED(event)) {
	if (selected_index < 0 || selected_index >= static_cast<int>(variants.size())) {
		return;
	}

	variants[selected_index].chance = static_cast<uint32_t>(chance_spin->GetValue());
	list_ctrl->SetItem(selected_index, 1, wxString::Format("%u%%", variants[selected_index].chance));

	UpdateTotalChanceLabel();

	if (on_data_changed_cb) {
		on_data_changed_cb();
	}
}

void SpawnAlternativesEditor::OnClickAdd(wxCommandEvent& WXUNUSED(event)) {
	MonsterSelectDialog dlg(this);
	if (dlg.ShowModal() == wxID_OK) {
		std::string monsterName = dlg.GetSelectedMonster();
		if (monsterName.empty()) {
			return;
		}

		for (size_t i = 0; i < variants.size(); ++i) {
			if (variants[i].name == monsterName) {
				selected_index = static_cast<int>(i);
				list_ctrl->SetItemState(selected_index, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
				UpdateButtonStates();
				return;
			}
		}

		uint32_t currentTotal = 0;
		for (const auto& v : variants) {
			currentTotal += v.chance;
		}

		uint32_t newChance = 0;
		if (variants.size() == 1 && currentTotal == 100) {
			variants[0].chance = 50;
			newChance = 50;
		} else if (currentTotal < 100) {
			newChance = 100 - currentTotal;
		} else {
			newChance = std::max<uint32_t>(1, 100 / static_cast<uint32_t>(variants.size() + 1));
		}

		variants.push_back({ monsterName, newChance });
		RefreshList();

		selected_index = static_cast<int>(variants.size() - 1);
		list_ctrl->SetItemState(selected_index, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);

		UpdateButtonStates();
		UpdateTotalChanceLabel();

		if (on_alternatives_changed_cb) {
			on_alternatives_changed_cb(variants.size() > 1);
		}
		if (on_data_changed_cb) {
			on_data_changed_cb();
		}
	}
}

void SpawnAlternativesEditor::OnClickRemove(wxCommandEvent& WXUNUSED(event)) {
	if (variants.size() <= 1 || selected_index < 0 || selected_index >= static_cast<int>(variants.size())) {
		return;
	}

	variants.erase(variants.begin() + selected_index);

	if (variants.size() == 1) {
		variants[0].chance = 100;
	}

	RefreshList();
	selected_index = std::clamp(selected_index, 0, static_cast<int>(variants.size() - 1));
	list_ctrl->SetItemState(selected_index, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);

	UpdateButtonStates();
	UpdateTotalChanceLabel();

	if (on_alternatives_changed_cb) {
		on_alternatives_changed_cb(variants.size() > 1);
	}
	if (on_data_changed_cb) {
		on_data_changed_cb();
	}
}
