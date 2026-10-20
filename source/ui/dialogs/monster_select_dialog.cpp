#include "app/main.h"
#include "ui/dialogs/monster_select_dialog.h"
#include "game/creatures.h"
#include "util/image_manager.h"
#include <wx/sizer.h>
#include <wx/button.h>
#include <algorithm>

MonsterSelectDialog::MonsterSelectDialog(wxWindow* parent, const std::string& current_selection) :
	wxDialog(parent, wxID_ANY, "Select Monster", wxDefaultPosition, FROM_DIP(parent, wxSize(320, 420)), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {

	for (auto it = g_creatures.begin(); it != g_creatures.end(); ++it) {
		const auto* ct = it->second;
		if (ct && !ct->isNpc && !ct->name.empty()) {
			all_monsters.push_back(ct->name);
		}
	}
	std::ranges::sort(all_monsters, [](const std::string& a, const std::string& b) {
		return as_lower_str(a) < as_lower_str(b);
	});
	all_monsters.erase(
		std::unique(all_monsters.begin(), all_monsters.end(), [](const std::string& a, const std::string& b) {
			return as_lower_str(a) == as_lower_str(b);
		}),
		all_monsters.end()
	);

	wxBoxSizer* main_sizer = newd wxBoxSizer(wxVERTICAL);

	search_ctrl = newd wxSearchCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	search_ctrl->SetDescriptiveText("Filter monsters...");
	main_sizer->Add(search_ctrl, 0, wxEXPAND | wxALL, 8);

	monster_list = newd wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SINGLE | wxLB_NEEDED_SB);
	main_sizer->Add(monster_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	PopulateList(current_selection);
	if (!current_selection.empty()) {
		int sel = monster_list->FindString(current_selection);
		if (sel != wxNOT_FOUND) {
			monster_list->SetSelection(sel);
		}
	}

	wxBoxSizer* btn_sizer = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* ok_btn = newd wxButton(this, wxID_OK, "OK");
	ok_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CHECK, wxSize(16, 16)));
	wxButton* cancel_btn = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancel_btn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_XMARK, wxSize(16, 16)));

	btn_sizer->AddStretchSpacer();
	btn_sizer->Add(ok_btn, 0, wxRIGHT, 6);
	btn_sizer->Add(cancel_btn, 0);
	main_sizer->Add(btn_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	SetSizer(main_sizer);
	CenterOnParent();

	search_ctrl->Bind(wxEVT_TEXT, &MonsterSelectDialog::OnSearchText, this);
	search_ctrl->Bind(wxEVT_TEXT_ENTER, &MonsterSelectDialog::OnOK, this);
	monster_list->Bind(wxEVT_LISTBOX_DCLICK, &MonsterSelectDialog::OnListDClick, this);
	ok_btn->Bind(wxEVT_BUTTON, &MonsterSelectDialog::OnOK, this);
}

void MonsterSelectDialog::PopulateList(const std::string& filter) {
	monster_list->Freeze();
	monster_list->Clear();

	std::string lower_filter = as_lower_str(filter);
	for (const auto& name : all_monsters) {
		if (lower_filter.empty() || as_lower_str(name).find(lower_filter) != std::string::npos) {
			monster_list->Append(name);
		}
	}
	monster_list->Thaw();
}

void MonsterSelectDialog::OnSearchText(wxCommandEvent& WXUNUSED(event)) {
	PopulateList(search_ctrl->GetValue().ToStdString());
}

void MonsterSelectDialog::OnListDClick(wxCommandEvent& WXUNUSED(event)) {
	int sel = monster_list->GetSelection();
	if (sel != wxNOT_FOUND) {
		selected_monster = monster_list->GetString(sel).ToStdString();
		EndModal(wxID_OK);
	}
}

void MonsterSelectDialog::OnOK(wxCommandEvent& WXUNUSED(event)) {
	wxString typed = search_ctrl->GetValue().Trim();
	const std::string typed_name = typed.ToStdString();

	if (!typed_name.empty()) {
		auto it = std::ranges::find_if(all_monsters, [&typed_name](const std::string& m) {
			return as_lower_str(m) == as_lower_str(typed_name);
		});
		if (it != all_monsters.end()) {
			selected_monster = *it;
			EndModal(wxID_OK);
			return;
		}

		int sel = monster_list->GetSelection();
		if (sel != wxNOT_FOUND) {
			selected_monster = monster_list->GetString(sel).ToStdString();
			EndModal(wxID_OK);
			return;
		}

		CreatureType* type = g_creatures[typed_name];
		if (!type || !type->isNpc) {
			selected_monster = typed_name;
			EndModal(wxID_OK);
			return;
		}
		return;
	}

	int sel = monster_list->GetSelection();
	if (sel != wxNOT_FOUND) {
		selected_monster = monster_list->GetString(sel).ToStdString();
		EndModal(wxID_OK);
		return;
	}
}
