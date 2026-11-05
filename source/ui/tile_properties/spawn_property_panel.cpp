//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/tile_properties/spawn_property_panel.h"
#include "game/spawn.h"
#include "map/tile.h"
#include "map/map.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"

SpawnPropertyPanel::SpawnPropertyPanel(wxWindow* parent) :
	CustomPropertyPanel(parent), current_spawn(nullptr), current_tile(nullptr), current_map(nullptr) {

	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

	radius_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Spawn Radius");
	radius_spin = new wxSpinCtrl(radius_sizer->GetStaticBox(), wxID_ANY);
	radius_spin->SetRange(1, 99);
	radius_sizer->Add(radius_spin, wxSizerFlags(1).Expand().Border(wxALL, 5));

	main_sizer->Add(radius_sizer, wxSizerFlags().Expand().Border(wxALL, 5));
	SetSizer(main_sizer);

	radius_spin->Bind(wxEVT_SPINCTRL, &SpawnPropertyPanel::OnRadiusChange, this);
}

SpawnPropertyPanel::~SpawnPropertyPanel() {
}

void SpawnPropertyPanel::SetItem(Item* /*item*/, Tile* tile, Map* map) {
	// Not used for items
	SetSpawn(nullptr, tile, map);
}

void SpawnPropertyPanel::SetSpawn(Spawn* spawn, Tile* tile, Map* map) {
	current_spawn = spawn;
	current_tile = tile;
	current_map = map;

	if (spawn) {
		GetSizer()->Show(radius_sizer, true);
		radius_spin->SetValue(spawn->getSize());
	} else {
		GetSizer()->Show(radius_sizer, false);
	}
	Layout();
	GetParent()->Layout();
}

void SpawnPropertyPanel::OnRadiusChange(wxSpinEvent&) {
	if (current_spawn && current_tile && current_map) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		if (new_tile->spawn) {
			new_tile->spawn->setSize(radius_spin->GetValue());

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
			g_gui.RefreshView();
		}
	}
}
