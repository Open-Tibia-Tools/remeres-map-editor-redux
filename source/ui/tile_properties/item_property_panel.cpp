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
#include "game/srv_liquids.h"
#include "ui/properties/srv_attribute_fields.h"

#include <limits>

namespace {
	using SrvAttribute = rme::srv::ItemAttribute;
	using rme::srv::isSrvClient;
}

ItemPropertyPanel::ItemPropertyPanel(wxWindow* parent) :
	CustomPropertyPanel(parent), current_item(nullptr), current_tile(nullptr), current_map(nullptr) {

	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

	item_info_label = new wxStaticText(this, wxID_ANY, "");
	item_info_label->SetFont(item_info_label->GetFont().Bold());
	main_sizer->Add(item_info_label, wxSizerFlags().Expand().Border(wxALL, 5));

	wxStaticBoxSizer* action_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Action");

	wxFlexGridSizer* action_grid = new wxFlexGridSizer(2, 2, 5, 5);
	action_grid->AddGrowableCol(1);

	action_grid->Add(new wxStaticText(action_sizer->GetStaticBox(), wxID_ANY, "Action ID:"), wxSizerFlags().CenterVertical());
	action_id_spin = new wxSpinCtrl(action_sizer->GetStaticBox(), wxID_ANY);
	action_id_spin->SetRange(0, 65535);
	action_grid->Add(action_id_spin, wxSizerFlags(1).Expand());

	action_grid->Add(new wxStaticText(action_sizer->GetStaticBox(), wxID_ANY, "Unique ID:"), wxSizerFlags().CenterVertical());
	unique_id_spin = new wxSpinCtrl(action_sizer->GetStaticBox(), wxID_ANY);
	unique_id_spin->SetRange(0, 65535);
	action_grid->Add(unique_id_spin, wxSizerFlags(1).Expand());

	action_sizer->Add(action_grid, wxSizerFlags(1).Expand().Border(wxALL, 5));
	main_sizer->Add(action_sizer, wxSizerFlags().Expand().Border(wxALL, 5));

	srv_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Server");
	srv_grid = new wxFlexGridSizer(2, 5, 5);
	srv_grid->AddGrowableCol(1);
	for (size_t i = 0; i < rme::srv::kItemAttributes.size(); ++i) {
		const auto attribute = static_cast<SrvAttribute>(i);
		srv_labels[i] = new wxStaticText(srv_sizer->GetStaticBox(), wxID_ANY, wxstr(rme::srv::info(attribute).label) + ":");
		srv_spins[i] = new wxSpinCtrl(srv_sizer->GetStaticBox(), wxID_ANY);
		srv_spins[i]->SetRange(0, std::numeric_limits<int>::max());
		srv_grid->Add(srv_labels[i], wxSizerFlags().CenterVertical());
		srv_grid->Add(srv_spins[i], wxSizerFlags(1).Expand());
		srv_spins[i]->Bind(wxEVT_SPINCTRL, [this, attribute](wxSpinEvent&) { OnSrvAttributeChange(attribute); });
	}
	srv_sizer->Add(srv_grid, wxSizerFlags(1).Expand().Border(wxALL, 5));
	main_sizer->Add(srv_sizer, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 5));

	count_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Quantity");
	count_spin = new wxSpinCtrl(count_sizer->GetStaticBox(), wxID_ANY);
	count_sizer->Add(count_spin, wxSizerFlags().Expand().Border(wxALL, 5));
	splash_type_choice = new wxChoice(count_sizer->GetStaticBox(), wxID_ANY);
	count_sizer->Add(splash_type_choice, wxSizerFlags().Expand().Border(wxALL, 5));
	main_sizer->Add(count_sizer, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 5));

	text_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Text Description");
	text_ctrl = new wxTextCtrl(text_sizer->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, FromDIP(wxSize(-1, 160)), wxTE_MULTILINE);
	text_sizer->Add(text_ctrl, wxSizerFlags(1).Expand().Border(wxALL, 5));
	main_sizer->Add(text_sizer, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 5));

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
			item_info_label->SetLabelText(wxstr(std::format("SID: {}   CID: {} - {}", item->getID(), item->getClientID(), item->getName())));
		} else {
			item_info_label->SetLabelText(wxstr(std::format("{} - {}", item->getID(), item->getName())));
		}
		action_id_spin->SetValue(item->getActionID());
		unique_id_spin->SetValue(item->getUniqueID());
		action_id_spin->Enable(true);
		unique_id_spin->Enable(true);

		if (item->isStackable() || item->isFluidContainer() || item->isSplash() || item->isCharged()) {
			GetSizer()->Show(count_sizer, true);

			if (item->isSplash() || item->isFluidContainer()) {
				count_sizer->GetStaticBox()->SetLabelText("Fluid Type");
				count_spin->Show(false);
				splash_type_choice->Show(true);

				splash_type_choice->Clear();
				if (isSrvClient()) {
					// Only the liquids the original server knows; pools can be empty too.
					for (const SplashType liquid : rme::srv::kLiquids) {
						splash_type_choice->Append(wxstr(Item::LiquidID2Name(liquid)), (void*)(intptr_t)(liquid));
					}
					splash_type_choice->SetSelection(0);
					for (unsigned int i = 0; i < splash_type_choice->GetCount(); ++i) {
						if ((intptr_t)splash_type_choice->GetClientData(i) == item->getSubtype()) {
							splash_type_choice->SetSelection(static_cast<int>(i));
						}
					}
				} else {
					if (item->isFluidContainer()) {
						splash_type_choice->Append(wxstr(Item::LiquidID2Name(LIQUID_NONE)), (void*)(intptr_t)(LIQUID_NONE));
					}
					for (SplashType splashType : kValidSplashTypes) {
						splash_type_choice->Append(wxstr(Item::LiquidID2Name(splashType)), (void*)(intptr_t)(splashType));
					}

					if (item->getSubtype()) {
						int selection = wxNOT_FOUND;
						for (unsigned int i = 0; i < splash_type_choice->GetCount(); ++i) {
							if (reinterpret_cast<intptr_t>(splash_type_choice->GetClientData(i)) == static_cast<intptr_t>(item->getSubtype())) {
								selection = static_cast<int>(i);
								break;
							}
						}
						if (selection != wxNOT_FOUND) {
							splash_type_choice->SetSelection(selection);
						} else {
							splash_type_choice->Append(wxstr(std::format("Unknown ({})", item->getSubtype())), reinterpret_cast<void*>(static_cast<intptr_t>(item->getSubtype())));
							splash_type_choice->SetSelection(static_cast<int>(splash_type_choice->GetCount()) - 1);
						}
					} else {
						splash_type_choice->SetSelection(0);
					}
				}
			} else {
				if (item->isCharged()) {
					count_sizer->GetStaticBox()->SetLabelText("Charges");
				} else {
					count_sizer->GetStaticBox()->SetLabelText("Count");
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
		item_info_label->SetLabelText("");
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
	UpdateSrvAttributes(item);
	Layout();
	GetParent()->Layout();
}

void ItemPropertyPanel::UpdateSrvAttributes(Item* item) {
	bool any_shown = false;
	for (size_t i = 0; i < rme::srv::kItemAttributes.size(); ++i) {
		const auto attribute = static_cast<SrvAttribute>(i);
		const bool show = item && rme::srv::showsAttribute(*item, attribute);
		srv_grid->Show(srv_labels[i], show);
		srv_grid->Show(srv_spins[i], show);
		if (show) {
			srv_spins[i]->SetValue(rme::srv::attributeValue(*item, attribute));
		}
		any_shown = any_shown || show;
	}
	GetSizer()->Show(srv_sizer, any_shown);
}

void ItemPropertyPanel::OnSrvAttributeChange(SrvAttribute attribute) {
	if (!current_item || !current_tile || !current_map) {
		return;
	}
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}

	std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
	const int index = current_tile->getIndexOf(current_item);
	if (index == -1) {
		return;
	}
	Item* new_item = new_tile->getItemAt(index);
	rme::srv::setAttributeValue(*new_item, attribute, srv_spins[static_cast<size_t>(attribute)]->GetValue());

	std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
	action->addChange(std::make_unique<Change>(std::move(new_tile)));
	editor->addAction(std::move(action));
	g_gui.RefreshView();
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
			int sel = splash_type_choice->GetSelection();
			if (sel != wxNOT_FOUND) {
				int new_type = static_cast<int>(reinterpret_cast<intptr_t>(splash_type_choice->GetClientData(sel)));
				new_item->setSubtype(new_type);

				std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
				editor->addAction(std::move(action));
				g_gui.RefreshView();
			}
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
