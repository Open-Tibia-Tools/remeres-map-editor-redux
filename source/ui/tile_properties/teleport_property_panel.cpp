//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/tile_properties/teleport_property_panel.h"
#include "game/complexitem.h"
#include "map/tile.h"
#include "map/map.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"

TeleportPropertyPanel::TeleportPropertyPanel(wxWindow* parent) :
	ItemPropertyPanel(parent) {

	dest_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Destination");
	wxFlexGridSizer* dest_grid = new wxFlexGridSizer(3, 2, FromDIP(5), FromDIP(5));
	dest_grid->AddGrowableCol(1);

	dest_grid->Add(new wxStaticText(dest_sizer->GetStaticBox(), wxID_ANY, "X:"), wxSizerFlags().CenterVertical());
	x_spin = new wxSpinCtrl(dest_sizer->GetStaticBox(), wxID_ANY);
	x_spin->SetRange(0, 65535);
	dest_grid->Add(x_spin, wxSizerFlags(1).Expand());

	dest_grid->Add(new wxStaticText(dest_sizer->GetStaticBox(), wxID_ANY, "Y:"), wxSizerFlags().CenterVertical());
	y_spin = new wxSpinCtrl(dest_sizer->GetStaticBox(), wxID_ANY);
	y_spin->SetRange(0, 65535);
	dest_grid->Add(y_spin, wxSizerFlags(1).Expand());

	dest_grid->Add(new wxStaticText(dest_sizer->GetStaticBox(), wxID_ANY, "Z:"), wxSizerFlags().CenterVertical());
	z_spin = new wxSpinCtrl(dest_sizer->GetStaticBox(), wxID_ANY);
	z_spin->SetRange(0, 15);
	dest_grid->Add(z_spin, wxSizerFlags(1).Expand());

	dest_sizer->Add(dest_grid, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(5)));
	GetSizer()->Add(dest_sizer, wxSizerFlags().Expand().Border(wxALL, FromDIP(5)));

	x_spin->Bind(wxEVT_SPINCTRL, &TeleportPropertyPanel::OnDestChange, this);
	y_spin->Bind(wxEVT_SPINCTRL, &TeleportPropertyPanel::OnDestChange, this);
	z_spin->Bind(wxEVT_SPINCTRL, &TeleportPropertyPanel::OnDestChange, this);
}

TeleportPropertyPanel::~TeleportPropertyPanel() {
}

void TeleportPropertyPanel::SetItem(Item* item, Tile* tile, Map* map) {
	ItemPropertyPanel::SetItem(item, tile, map);

	if (item && item->asTeleport()) {
		Teleport* teleport = static_cast<Teleport*>(item);
		GetSizer()->Show(dest_sizer, true);
		x_spin->SetValue(teleport->getX());
		y_spin->SetValue(teleport->getY());
		z_spin->SetValue(teleport->getZ());
	} else {
		GetSizer()->Show(dest_sizer, false);
	}
	Layout();
	GetParent()->Layout();
}

void TeleportPropertyPanel::OnDestChange(wxSpinEvent& event) {
	if (current_item && current_tile && current_map && current_item->asTeleport()) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return;
		}

		std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
		int index = current_tile->getIndexOf(current_item);
		if (index != -1) {
			Item* new_item = new_tile->getItemAt(index);
			if (new_item->asTeleport()) {
				Teleport* teleport = static_cast<Teleport*>(new_item);
				Position dest(x_spin->GetValue(), y_spin->GetValue(), z_spin->GetValue());
				teleport->setDestination(dest);

				std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
				editor->addAction(std::move(action));
			}
		}
	}
}
