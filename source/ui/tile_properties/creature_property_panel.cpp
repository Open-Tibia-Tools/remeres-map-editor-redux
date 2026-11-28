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
#include "ui/properties/srv_attribute_fields.h"
#include "game/srv_homes.h"

namespace {
	// Amount only matters for sector map monster homes.
	bool showsAmount(const Creature& creature) {
		return !creature.isNpc() && (rme::srv::isSrvClient() || creature.getAmount() != 1);
	}

	// On a sector map every monster is a home with its own radius; an NPC
	// read from its npc/*.npc file has one too.
	bool showsHomeRadius(const Creature& creature) {
		return rme::srv::isSrvClient() && (!creature.isNpc() || creature.getHomeRadius());
	}
}

CreaturePropertyPanel::CreaturePropertyPanel(wxWindow* parent) :
	CustomPropertyPanel(parent), current_creature(nullptr), current_tile(nullptr), current_map(nullptr) {

	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

	// Spawn time
	time_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Spawn Time");
	spawntime_spin = new wxSpinCtrl(time_sizer->GetStaticBox(), wxID_ANY);
	spawntime_spin->SetRange(1, 86400); // Max 24 hours
	time_sizer->Add(spawntime_spin, wxSizerFlags(1).Expand().Border(wxALL, 5));
	main_sizer->Add(time_sizer, wxSizerFlags().Expand().Border(wxALL, 5));

	// Amount (sector map monster homes)
	amount_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Amount");
	amount_spin = new wxSpinCtrl(amount_sizer->GetStaticBox(), wxID_ANY);
	amount_spin->SetRange(1, 1000);
	amount_spin->SetToolTip("How many of this monster the server keeps alive within the spawn radius");
	amount_sizer->Add(amount_spin, wxSizerFlags(1).Expand().Border(wxALL, 5));
	main_sizer->Add(amount_sizer, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 5));

	// Radius (sector map monster homes): the spawn on the home tile
	radius_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Radius");
	radius_spin = new wxSpinCtrl(radius_sizer->GetStaticBox(), wxID_ANY);
	radius_spin->SetRange(0, 65535);
	radius_spin->SetToolTip("How far from its home the creature roams and respawns");
	radius_sizer->Add(radius_spin, wxSizerFlags(1).Expand().Border(wxALL, 5));
	main_sizer->Add(radius_sizer, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 5));

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
	amount_spin->Bind(wxEVT_SPINCTRL, &CreaturePropertyPanel::OnAmountChange, this);
	radius_spin->Bind(wxEVT_SPINCTRL, &CreaturePropertyPanel::OnRadiusChange, this);
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
		GetSizer()->Show(amount_sizer, showsAmount(*creature));
		amount_spin->SetValue(creature->getAmount());
		const bool shows_radius = showsHomeRadius(*creature);
		GetSizer()->Show(radius_sizer, shows_radius);
		if (shows_radius) {
			radius_spin->SetValue(creature->getHomeRadius().value_or(rme::srv::kDefaultMonsterHomeRadius));
		}

		Direction dir = creature->getDirection();
		for (size_t i = 0; i < direction_choice->GetCount(); ++i) {
			if ((int)(intptr_t)direction_choice->GetClientData(i) == (int)dir) {
				direction_choice->SetSelection(i);
				break;
			}
		}
	} else {
		GetSizer()->Show(time_sizer, false);
		GetSizer()->Show(amount_sizer, false);
		GetSizer()->Show(radius_sizer, false);
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

void CreaturePropertyPanel::OnAmountChange(wxSpinEvent& event) {
	if (current_creature && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		if (new_tile->creature) {
			new_tile->creature->setAmount(amount_spin->GetValue());

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
			g_gui.RefreshView();
		}
	}
}

void CreaturePropertyPanel::OnRadiusChange(wxSpinEvent& event) {
	if (current_creature && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		if (new_tile->creature) {
			new_tile->creature->setHomeRadius(radius_spin->GetValue());

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
			g_gui.RefreshView();
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
