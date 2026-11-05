//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/tile_properties/creature_property_panel.h"
#include "game/creature.h"
#include "map/tile.h"
#include "map/map.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"

CreaturePropertyPanel::CreaturePropertyPanel(wxWindow* parent) :
	CustomPropertyPanel(parent), current_creature(nullptr), current_tile(nullptr), current_map(nullptr) {

	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

	// Spawn time
	time_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Spawn Time");
	spawntime_spin = new wxSpinCtrl(time_sizer->GetStaticBox(), wxID_ANY);
	spawntime_spin->SetRange(1, 86400); // Max 24 hours
	time_sizer->Add(spawntime_spin, wxSizerFlags(1).Expand().Border(wxALL, 5));
	main_sizer->Add(time_sizer, wxSizerFlags().Expand().Border(wxALL, 5));

	// Direction
	dir_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Direction");
	direction_choice = new wxChoice(dir_sizer->GetStaticBox(), wxID_ANY);
	for (Direction dir = DIRECTION_FIRST; dir <= DIRECTION_LAST; ++dir) {
		direction_choice->Append(wxstr(Creature::DirID2Name(dir)), (void*)(intptr_t)(dir));
	}
	dir_sizer->Add(direction_choice, wxSizerFlags(1).Expand().Border(wxALL, 5));
	main_sizer->Add(dir_sizer, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 5));

	SetSizer(main_sizer);

	spawntime_spin->Bind(wxEVT_SPINCTRL, &CreaturePropertyPanel::OnSpawnTimeChange, this);
	direction_choice->Bind(wxEVT_CHOICE, &CreaturePropertyPanel::OnDirectionChange, this);
}

CreaturePropertyPanel::~CreaturePropertyPanel() {
}

void CreaturePropertyPanel::SetItem(Item* /*item*/, Tile* tile, Map* map) {
	// Not used for items
	SetCreature(nullptr, tile, map);
}

void CreaturePropertyPanel::SetCreature(Creature* creature, Tile* tile, Map* map) {
	current_creature = creature;
	current_tile = tile;
	current_map = map;

	if (creature) {
		GetSizer()->Show(time_sizer, true);
		GetSizer()->Show(dir_sizer, true);
		spawntime_spin->SetValue(creature->getSpawnTime());

		Direction dir = creature->getDirection();
		for (size_t i = 0; i < direction_choice->GetCount(); ++i) {
			if ((int)(intptr_t)direction_choice->GetClientData(i) == (int)dir) {
				direction_choice->SetSelection(i);
				break;
			}
		}
	} else {
		GetSizer()->Show(time_sizer, false);
		GetSizer()->Show(dir_sizer, false);
	}
	Layout();
	GetParent()->Layout();
}

void CreaturePropertyPanel::OnSpawnTimeChange(wxSpinEvent& event) {
	if (current_creature && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		if (new_tile->creature) {
			new_tile->creature->setSpawnTime(spawntime_spin->GetValue());

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
		}
	}
}

void CreaturePropertyPanel::OnDirectionChange(wxCommandEvent& event) {
	if (current_creature && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		if (new_tile->creature) {
			int new_dir = (int)(intptr_t)direction_choice->GetClientData(direction_choice->GetSelection());
			new_tile->creature->setDirection((Direction)new_dir);

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
			g_gui.RefreshView();
		}
	}
}
