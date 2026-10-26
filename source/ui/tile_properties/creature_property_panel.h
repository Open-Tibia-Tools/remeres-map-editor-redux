//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_CREATURE_PROPERTY_PANEL_H_
#define RME_UI_CREATURE_PROPERTY_PANEL_H_

#include "ui/tile_properties/custom_property_panel.h"
#include <wx/spinctrl.h>
#include <wx/choice.h>

class Creature;
class SpawnAlternativesEditor;

class CreaturePropertyPanel : public CustomPropertyPanel {
public:
	CreaturePropertyPanel(wxWindow* parent);
	virtual ~CreaturePropertyPanel();

	void SetItem(Item* item, Tile* tile, Map* map) override;
	void SetCreature(Creature* creature, Tile* tile, Map* map);

	void OnSpawnTimeChange(wxSpinEvent& event);
	void OnDirectionChange(wxCommandEvent& event);

protected:
	Creature* current_creature;
	Tile* current_tile;
	Map* current_map;

	wxStaticBoxSizer* time_sizer;
	wxSpinCtrl* spawntime_spin;

	wxStaticBoxSizer* dir_sizer;
	wxChoice* direction_choice;
	SpawnAlternativesEditor* alternatives_editor;
	bool updating_from_model = false;
};

#endif
