//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/tile_properties/item_property_panel.h"
#include "ui/gui.h"
#include "game/item.h"
#include "map/tile.h"
#include "map/map.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"

ItemPropertyPanel::ItemPropertyPanel(wxWindow* parent) :
	CustomPropertyPanel(parent), current_item(nullptr), current_tile(nullptr), current_map(nullptr) {

	wxBoxSizer* main_sizer = newd wxBoxSizer(wxVERTICAL);

	item_info_label = newd wxStaticText(this, wxID_ANY, "");
	item_info_label->SetFont(item_info_label->GetFont().Bold());
	main_sizer->Add(item_info_label, 0, wxEXPAND | wxALL, 5);

	wxStaticBoxSizer* action_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Action");

	wxFlexGridSizer* action_grid = newd wxFlexGridSizer(2, 2, 5, 5);
	action_grid->AddGrowableCol(1);

	action_grid->Add(newd wxStaticText(action_sizer->GetStaticBox(), wxID_ANY, "Action ID:"), 0, wxALIGN_CENTER_VERTICAL);
	action_id_spin = newd wxSpinCtrl(action_sizer->GetStaticBox(), wxID_ANY);
	action_id_spin->SetRange(0, 65535);
	action_grid->Add(action_id_spin, 1, wxEXPAND);

	action_grid->Add(newd wxStaticText(action_sizer->GetStaticBox(), wxID_ANY, "Unique ID:"), 0, wxALIGN_CENTER_VERTICAL);
	unique_id_spin = newd wxSpinCtrl(action_sizer->GetStaticBox(), wxID_ANY);
	unique_id_spin->SetRange(0, 65535);
	action_grid->Add(unique_id_spin, 1, wxEXPAND);

	action_sizer->Add(action_grid, 1, wxEXPAND | wxALL, 5);
	main_sizer->Add(action_sizer, 0, wxEXPAND | wxALL, 5);

	count_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Quantity");
	count_spin = newd wxSpinCtrl(count_sizer->GetStaticBox(), wxID_ANY);
	count_sizer->Add(count_spin, 0, wxEXPAND | wxALL, 5);
	splash_type_choice = newd wxChoice(count_sizer->GetStaticBox(), wxID_ANY);
	count_sizer->Add(splash_type_choice, 0, wxEXPAND | wxALL, 5);
	main_sizer->Add(count_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	text_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Text Description");
	text_ctrl = newd wxTextCtrl(text_sizer->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, FromDIP(wxSize(-1, 160)), wxTE_MULTILINE);
	text_sizer->Add(text_ctrl, 1, wxEXPAND | wxALL, 5);
	main_sizer->Add(text_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	SetSizer(main_sizer);

	action_id_spin->Bind(wxEVT_SPINCTRL, &ItemPropertyPanel::OnActionIdChange, this);
	unique_id_spin->Bind(wxEVT_SPINCTRL, &ItemPropertyPanel::OnUniqueIdChange, this);
	count_spin->Bind(wxEVT_SPINCTRL, &ItemPropertyPanel::OnCountChange, this);
	splash_type_choice->Bind(wxEVT_CHOICE, &ItemPropertyPanel::OnSplashTypeChange, this);
	text_ctrl->Bind(wxEVT_KILL_FOCUS, &ItemPropertyPanel::OnTextChange, this);
}

ItemPropertyPanel::~ItemPropertyPanel() { }

void ItemPropertyPanel::SetItem(Item* item, Tile* tile, Map* map) {
	current_item = item;
	current_tile = tile;
	current_map = map;

	if (item) {
		if (item->getClientID() != 0 && item->getClientID() != item->getID()) {
			item_info_label->SetLabel(wxstr(std::format("SID: {}   CID: {} - {}", item->getID(), item->getClientID(), item->getName())));
		} else {
			item_info_label->SetLabel(wxstr(std::format("{} - {}", item->getID(), item->getName())));
		}
		action_id_spin->SetValue(item->getActionID());
		unique_id_spin->SetValue(item->getUniqueID());
		action_id_spin->Enable(true);
		unique_id_spin->Enable(true);

		if (item->isStackable() || item->isFluidContainer() || item->isSplash() || item->isCharged()) {
			GetSizer()->Show(count_sizer, true);

			if (item->isSplash() || item->isFluidContainer()) {
				count_sizer->GetStaticBox()->SetLabel("Fluid Type");
				count_spin->Show(false);
				splash_type_choice->Show(true);

				splash_type_choice->Clear();
				if (item->isFluidContainer()) {
					splash_type_choice->Append(wxstr(Item::LiquidID2Name(LIQUID_NONE)), (void*)(intptr_t)(LIQUID_NONE));
				}
				for (SplashType splashType = LIQUID_FIRST; splashType != LIQUID_LAST; ++splashType) {
					splash_type_choice->Append(wxstr(Item::LiquidID2Name(splashType)), (void*)(intptr_t)(splashType));
				}

				if (item->getSubtype()) {
					const std::string_view text = Item::LiquidID2Name(item->getSubtype());
					splash_type_choice->SetStringSelection(wxstr(std::string(text)));
				} else {
					splash_type_choice->SetSelection(0);
				}
			} else {
				if (item->isCharged()) {
					count_sizer->GetStaticBox()->SetLabel("Charges");
				} else {
					count_sizer->GetStaticBox()->SetLabel("Count");
				}
				count_spin->Show(true);
				splash_type_choice->Show(false);
				count_spin->Enable(true);
				int max_val = item->isStackable() ? 100 : 7;
				if (item->isCharged()) {
					max_val = item->isClientCharged() ? 250 : max_val;
				}
				if (item->isExtraCharged()) {
					max_val = 65500;
				}
				count_spin->SetRange(0, max_val);
				count_spin->SetValue(item->getCount());
			}
		} else {
			GetSizer()->Show(count_sizer, false);
			count_spin->Show(false);
			splash_type_choice->Show(false);
		}

		if (item->canHoldText() || item->canHoldDescription()) {
			GetSizer()->Show(text_sizer, true);
			text_ctrl->Show(true);
			text_ctrl->Enable(true);
			text_ctrl->ChangeValue(wxstr(item->getText()));
		} else {
			GetSizer()->Show(text_sizer, false);
			text_ctrl->Show(false);
			text_ctrl->Enable(false);
		}
	} else {
		item_info_label->SetLabel("");
		action_id_spin->SetValue(0);
		unique_id_spin->SetValue(0);
		action_id_spin->Enable(false);
		unique_id_spin->Enable(false);
		GetSizer()->Show(count_sizer, false);
		count_spin->Show(false);
		splash_type_choice->Show(false);
		GetSizer()->Show(text_sizer, false);
		text_ctrl->Show(false);
	}
	Layout();
	GetParent()->Layout();
}

void ItemPropertyPanel::OnActionIdChange(wxSpinEvent& event) {
	if (current_item && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		int index = current_tile->getIndexOf(current_item);
		if (index != -1) {
			Item* new_item = new_tile->getItemAt(index);
			new_item->setActionID(action_id_spin->GetValue());

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
		}
	}
}

void ItemPropertyPanel::OnUniqueIdChange(wxSpinEvent& event) {
	if (current_item && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		int index = current_tile->getIndexOf(current_item);
		if (index != -1) {
			Item* new_item = new_tile->getItemAt(index);
			new_item->setUniqueID(unique_id_spin->GetValue());

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
		}
	}
}

void ItemPropertyPanel::OnCountChange(wxSpinEvent& event) {
	if (current_item && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		int index = current_tile->getIndexOf(current_item);
		if (index != -1) {
			Item* new_item = new_tile->getItemAt(index);
			new_item->setSubtype(count_spin->GetValue());

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
		}
	}
}

void ItemPropertyPanel::OnSplashTypeChange(wxCommandEvent& event) {
	if (current_item && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		int index = current_tile->getIndexOf(current_item);
		if (index != -1) {
			Item* new_item = new_tile->getItemAt(index);
			int new_type = (int)(intptr_t)splash_type_choice->GetClientData(splash_type_choice->GetSelection());
			new_item->setSubtype(new_type);

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
			g_gui.RefreshView();
		}
	}
}

void ItemPropertyPanel::OnTextChange(wxEvent& event) {
	if (current_item && current_tile && current_map) {
		if (text_ctrl->GetValue() == wxstr(current_item->getText())) {
			return;
		}

		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		int index = current_tile->getIndexOf(current_item);
		if (index != -1) {
			Item* new_item = new_tile->getItemAt(index);
			new_item->setText(nstr(text_ctrl->GetValue()));

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
		}
	}
	event.Skip();
}
