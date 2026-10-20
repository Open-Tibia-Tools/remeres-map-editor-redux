//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/tile_properties/creature_property_panel.h"
#include "ui/properties/spawn_alternatives_editor.h"
#include "game/creature.h"
#include "map/tile.h"
#include "map/map.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"

CreaturePropertyPanel::CreaturePropertyPanel(wxWindow* parent) :
	CustomPropertyPanel(parent), current_creature(nullptr), current_tile(nullptr), current_map(nullptr), alternatives_editor(nullptr) {

	wxBoxSizer* main_sizer = newd wxBoxSizer(wxVERTICAL);

	// Spawn time
	time_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Spawn Time");
	spawntime_spin = newd wxSpinCtrl(time_sizer->GetStaticBox(), wxID_ANY);
	spawntime_spin->SetRange(10, 86400); // Max 24 hours
	time_sizer->Add(spawntime_spin, 1, wxEXPAND | wxALL, 5);
	main_sizer->Add(time_sizer, 0, wxEXPAND | wxALL, 5);

	// Direction
	dir_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Direction");
	direction_choice = newd wxChoice(dir_sizer->GetStaticBox(), wxID_ANY);
	for (Direction dir = DIRECTION_FIRST; dir <= DIRECTION_LAST; ++dir) {
		direction_choice->Append(wxstr(Creature::DirID2Name(dir)), (void*)(intptr_t)(dir));
	}
	dir_sizer->Add(direction_choice, 1, wxEXPAND | wxALL, 5);
	main_sizer->Add(dir_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Varying Spawn alternatives
	alternatives_editor = newd SpawnAlternativesEditor(this);
	main_sizer->Add(alternatives_editor, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	SetSizer(main_sizer);

	spawntime_spin->Bind(wxEVT_SPINCTRL, &CreaturePropertyPanel::OnSpawnTimeChange, this);
	direction_choice->Bind(wxEVT_CHOICE, &CreaturePropertyPanel::OnDirectionChange, this);

	alternatives_editor->SetOnAlternativesChanged([this](bool hasAlternatives) {
		direction_choice->Enable(!hasAlternatives);
		if (hasAlternatives) {
			for (size_t i = 0; i < direction_choice->GetCount(); ++i) {
				if ((int)(intptr_t)direction_choice->GetClientData(i) == (int)NORTH) {
					direction_choice->SetSelection(i);
					break;
				}
			}
		}
	});

	alternatives_editor->SetOnDataChanged([this]() {
		if (updating_from_model) {
			return;
		}
		if (current_creature && current_tile && current_map) {
			Editor* editor = g_gui.GetCurrentEditor();
			if (!editor) {
				return;
			}

			Position pos = current_tile->getPosition();
			Tile* live_tile = current_map->getTile(pos);
			if (!live_tile) {
				live_tile = current_tile;
			}

			std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(live_tile, *current_map);
			if (new_tile->creature) {
				alternatives_editor->ApplyToCreature(new_tile->creature.get());
				new_tile->creature->setSpawnTime(spawntime_spin->GetValue());
				if (!new_tile->creature->getAlternatives().empty()) {
					new_tile->creature->setDirection(NORTH);
				}
				for (auto& alt : new_tile->creature->getAlternatives()) {
					alt.spawntime = spawntime_spin->GetValue();
					alt.direction = NORTH;
				}

				std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
				editor->addAction(std::move(action));
				current_tile = current_map->getTile(pos);
				if (current_tile) {
					current_creature = current_tile->creature.get();
				}
				g_gui.RefreshView();
			}
		}
	});
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

	updating_from_model = true;

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

		if (!creature->isNpc()) {
			GetSizer()->Show(alternatives_editor, true);
			alternatives_editor->SetCreature(creature);
			direction_choice->Enable(creature->getAlternatives().empty());
			if (!creature->getAlternatives().empty()) {
				for (size_t i = 0; i < direction_choice->GetCount(); ++i) {
					if ((int)(intptr_t)direction_choice->GetClientData(i) == (int)NORTH) {
						direction_choice->SetSelection(i);
						break;
					}
				}
			}
		} else {
			GetSizer()->Show(alternatives_editor, false);
			direction_choice->Enable(true);
		}
	} else {
		GetSizer()->Show(time_sizer, false);
		GetSizer()->Show(dir_sizer, false);
		GetSizer()->Show(alternatives_editor, false);
	}

	updating_from_model = false;
	Layout();
	GetParent()->Layout();
}

void CreaturePropertyPanel::OnSpawnTimeChange(wxSpinEvent& event) {
	if (current_creature && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		Position pos = current_tile->getPosition();
		Tile* live_tile = current_map->getTile(pos);
		if (!live_tile) {
			live_tile = current_tile;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(live_tile, *current_map);
		if (new_tile->creature) {
			new_tile->creature->setSpawnTime(spawntime_spin->GetValue());
			for (auto& alt : new_tile->creature->getAlternatives()) {
				alt.spawntime = spawntime_spin->GetValue();
				alt.direction = NORTH;
			}

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
			current_tile = current_map->getTile(pos);
			if (current_tile) {
				current_creature = current_tile->creature.get();
			}
		}
	}
}

void CreaturePropertyPanel::OnDirectionChange(wxCommandEvent& event) {
	if (current_creature && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		Position pos = current_tile->getPosition();
		Tile* live_tile = current_map->getTile(pos);
		if (!live_tile) {
			live_tile = current_tile;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(live_tile, *current_map);
		if (new_tile->creature) {
			int new_dir = (int)(intptr_t)direction_choice->GetClientData(direction_choice->GetSelection());
			new_tile->creature->setDirection((Direction)new_dir);

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
			current_tile = current_map->getTile(pos);
			if (current_tile) {
				current_creature = current_tile->creature.get();
			}
			g_gui.RefreshView();
		}
	}
}
