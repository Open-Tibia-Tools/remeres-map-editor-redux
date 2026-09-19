//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/tile_properties/spawn_creature_panel.h"
#include "map/tile.h"
#include "map/map.h"
#include "util/image_manager.h"
#include "game/spawn.h"
#include "game/creature.h"
#include "ui/gui_ids.h"
#include "ui/icons/sprite_icon_service.h"
#include "ui/gui.h"

SpawnCreaturePanel::SpawnCreaturePanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY) {

	wxStaticBoxSizer* main_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Spawn & Creature");

	// Spawn Node
	wxBoxSizer* spawn_sizer = newd wxBoxSizer(wxHORIZONTAL);
	spawn_bitmap = newd wxStaticBitmap(main_sizer->GetStaticBox(), wxID_ANY, wxNullBitmap, wxDefaultPosition, FROM_DIP(this, wxSize(32, 32)), wxBORDER_SIMPLE);
	spawn_bitmap->Hide();
	spawn_text = newd wxStaticText(main_sizer->GetStaticBox(), wxID_ANY, "No Spawn");
	spawn_sizer->Add(spawn_bitmap, wxSizerFlags(0).Center().Border(wxALL, 2));
	spawn_sizer->Add(spawn_text, wxSizerFlags(1).Center().Border(wxALL, 2));

	// Creature Node
	wxBoxSizer* creature_sizer = newd wxBoxSizer(wxHORIZONTAL);
	creature_bitmap = newd wxStaticBitmap(main_sizer->GetStaticBox(), wxID_ANY, wxNullBitmap, wxDefaultPosition, FROM_DIP(this, wxSize(32, 32)), wxBORDER_SIMPLE);
	creature_bitmap->Hide();
	creature_text = newd wxStaticText(main_sizer->GetStaticBox(), wxID_ANY, "No Creature");
	creature_sizer->Add(creature_bitmap, wxSizerFlags(0).Center().Border(wxALL, 2));
	creature_sizer->Add(creature_text, wxSizerFlags(1).Center().Border(wxALL, 2));

	main_sizer->Add(spawn_sizer, wxSizerFlags(1).Expand().Border(wxALL, 2));
	main_sizer->Add(creature_sizer, wxSizerFlags(1).Expand().Border(wxALL, 2));

	SetSizer(main_sizer);

	// Bind mouse events for clicking
	spawn_bitmap->Bind(wxEVT_LEFT_UP, &SpawnCreaturePanel::OnSpawnClick, this);
	spawn_text->Bind(wxEVT_LEFT_UP, &SpawnCreaturePanel::OnSpawnClick, this);
	creature_bitmap->Bind(wxEVT_LEFT_UP, &SpawnCreaturePanel::OnCreatureClick, this);
	creature_text->Bind(wxEVT_LEFT_UP, &SpawnCreaturePanel::OnCreatureClick, this);
}

SpawnCreaturePanel::~SpawnCreaturePanel() {
}

void SpawnCreaturePanel::SetTile(Tile* tile) {
	if (tile && tile->spawn) {
		spawn_text->SetLabelText(wxString::Format("Spawn\nR=%d", tile->spawn->getSize()));
		spawn_bitmap->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_FIRE, FROM_DIP(this, wxSize(32, 32))));
		spawn_bitmap->Show();
	} else {
		spawn_text->SetLabelText("No Spawn");
		spawn_bitmap->Hide();
	}

	if (tile && tile->creature) {
		creature_text->SetLabelText(wxString(tile->creature->getName()));
		GameSprite* spr = g_gui.gfx.getCreatureSprite(tile->creature->getLookType().lookType);
		if (spr) {
			wxBitmap bmp = SpriteIconService::Generate(spr, SPRITE_SIZE_32x32, tile->creature->getLookType(), false, SOUTH);
			creature_bitmap->SetBitmap(bmp);
		} else {
			creature_bitmap->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_DRAGON, FROM_DIP(this, wxSize(32, 32))));
		}
		creature_bitmap->Show();
	} else {
		creature_text->SetLabelText("No Creature");
		creature_bitmap->Hide();
	}
	Layout();
}

void SpawnCreaturePanel::OnSpawnClick(wxMouseEvent& event) {
	if (on_spawn_selected_cb) {
		on_spawn_selected_cb();
	}
}

void SpawnCreaturePanel::OnCreatureClick(wxMouseEvent& event) {
	if (on_creature_selected_cb) {
		on_creature_selected_cb();
	}
}
