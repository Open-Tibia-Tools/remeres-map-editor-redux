//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "ui/tile_properties/map_flags_panel.h"
#include "map/tile.h"
#include "map/map.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"
#include "ui/gui.h"

MapFlagsPanel::MapFlagsPanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY), current_tile(nullptr), current_map(nullptr) {

	wxStaticBoxSizer* sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Map Flags");

	chk_pz = newd wxCheckBox(sizer->GetStaticBox(), wxID_ANY, "Protection Zone");
	chk_nopvp = newd wxCheckBox(sizer->GetStaticBox(), wxID_ANY, "No PvP");
	chk_nologout = newd wxCheckBox(sizer->GetStaticBox(), wxID_ANY, "No Logout");
	chk_pvpzone = newd wxCheckBox(sizer->GetStaticBox(), wxID_ANY, "PvP Zone");

	sizer->Add(chk_pz, wxSizerFlags(0).Left().Border(wxALL, 2));
	sizer->Add(chk_nopvp, wxSizerFlags(0).Left().Border(wxALL, 2));
	sizer->Add(chk_nologout, wxSizerFlags(0).Left().Border(wxALL, 2));
	sizer->Add(chk_pvpzone, wxSizerFlags(0).Left().Border(wxALL, 2));

	SetSizer(sizer);

	chk_pz->Bind(wxEVT_CHECKBOX, &MapFlagsPanel::OnToggleFlag, this);
	chk_nopvp->Bind(wxEVT_CHECKBOX, &MapFlagsPanel::OnToggleFlag, this);
	chk_nologout->Bind(wxEVT_CHECKBOX, &MapFlagsPanel::OnToggleFlag, this);
	chk_pvpzone->Bind(wxEVT_CHECKBOX, &MapFlagsPanel::OnToggleFlag, this);
}

MapFlagsPanel::~MapFlagsPanel() {
}

void MapFlagsPanel::SetTile(Tile* tile, Map* map) {
	current_tile = tile;
	current_map = map;

	if (tile) {
		chk_pz->SetValue(tile->isPZ());
		chk_nopvp->SetValue(tile->getMapFlags() & TILESTATE_NOPVP);
		chk_nologout->SetValue(tile->getMapFlags() & TILESTATE_NOLOGOUT);
		chk_pvpzone->SetValue(tile->getMapFlags() & TILESTATE_PVPZONE);

		chk_pz->Enable(true);
		chk_nopvp->Enable(true);
		chk_nologout->Enable(true);
		chk_pvpzone->Enable(true);
	} else {
		chk_pz->SetValue(false);
		chk_nopvp->SetValue(false);
		chk_nologout->SetValue(false);
		chk_pvpzone->SetValue(false);

		chk_pz->Enable(false);
		chk_nopvp->Enable(false);
		chk_nologout->Enable(false);
		chk_pvpzone->Enable(false);
	}
}

void MapFlagsPanel::OnToggleFlag(wxCommandEvent& event) {
	if (!current_tile || !current_map) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}

	std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
	new_tile->setPZ(chk_pz->GetValue());

	if (chk_nopvp->GetValue()) {
		new_tile->addMapFlags(TILESTATE_NOPVP);
	} else {
		new_tile->unsetMapFlags(TILESTATE_NOPVP);
	}

	if (chk_nologout->GetValue()) {
		new_tile->addMapFlags(TILESTATE_NOLOGOUT);
	} else {
		new_tile->unsetMapFlags(TILESTATE_NOLOGOUT);
	}

	if (chk_pvpzone->GetValue()) {
		new_tile->addMapFlags(TILESTATE_PVPZONE);
	} else {
		new_tile->unsetMapFlags(TILESTATE_PVPZONE);
	}

	Tile* old_ptr = new_tile.get();
	std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
	action->addChange(std::make_unique<Change>(std::move(new_tile)));
	editor->addAction(std::move(action));
	current_tile = editor->map.getTile(old_ptr->getPosition());
}
