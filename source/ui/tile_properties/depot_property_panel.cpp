//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/tile_properties/depot_property_panel.h"
#include "game/complexitem.h"
#include "game/town.h"
#include "map/map.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"

DepotPropertyPanel::DepotPropertyPanel(wxWindow* parent) :
	ItemPropertyPanel(parent) {

	depot_id_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Depot Town");
	depot_id_field = new wxChoice(depot_id_sizer->GetStaticBox(), wxID_ANY);
	depot_id_sizer->Add(depot_id_field, wxSizerFlags().Expand().Border(wxALL, FromDIP(5)));
	GetSizer()->Add(depot_id_sizer, wxSizerFlags().Expand().Border(wxALL, FromDIP(5)));

	depot_id_field->Bind(wxEVT_CHOICE, &DepotPropertyPanel::OnDepotIdChange, this);
}

DepotPropertyPanel::~DepotPropertyPanel() {
}

void DepotPropertyPanel::SetItem(Item* item, Tile* tile, Map* map) {
	ItemPropertyPanel::SetItem(item, tile, map);

	Depot* depot = item ? dynamic_cast<Depot*>(item) : nullptr;
	if (depot && map) {
		GetSizer()->Show(depot_id_sizer, true);

		depot_id_field->Clear();
		const Towns& towns = map->towns;
		int to_select_index = 0;

		if (towns.count() > 0) {
			bool found = false;
			if (towns.count() > 0) {
				for (TownMap::const_iterator town_iter = towns.begin();
					 town_iter != towns.end();
					 ++town_iter) {
					if (town_iter->second->getID() == depot->getDepotID()) {
						found = true;
					}
					depot_id_field->Append(wxstr(town_iter->second->getName()), (void*)(intptr_t)(town_iter->second->getID()));
					if (!found) {
						++to_select_index;
					}
				}
			}

			if (!found && depot->getDepotID() != 0) {
				depot_id_field->Append("Undefined Town (id:" + std::to_string(depot->getDepotID()) + ")", (void*)(intptr_t)(depot->getDepotID()));
			}
			depot_id_field->Append(wxString("No Town"), (void*)(intptr_t)(0));
			if (depot->getDepotID() == 0) {
				to_select_index = depot_id_field->GetCount() - 1;
			}
			depot_id_field->SetSelection(to_select_index);
		} else {
			GetSizer()->Show(depot_id_sizer, false);
		}
		Layout();
		GetParent()->Layout();
	}
}

void DepotPropertyPanel::OnDepotIdChange(wxCommandEvent& event) {
	if (current_item && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		int index = current_tile->getIndexOf(current_item);
		if (index != -1) {
			Item* new_item_base = new_tile->getItemAt(index);
			if (new_item_base->asDepot()) {
				Depot* depot = static_cast<Depot*>(new_item_base);
				int new_depotid = (int)(intptr_t)depot_id_field->GetClientData(depot_id_field->GetSelection());
				depot->setDepotID(new_depotid);

				std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
				editor->addAction(std::move(action));
			}
		}
	}
}
