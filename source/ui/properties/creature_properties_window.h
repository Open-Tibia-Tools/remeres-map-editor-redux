//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_CREATURE_PROPERTIES_WINDOW_H_
#define RME_CREATURE_PROPERTIES_WINDOW_H_

#include "app/main.h"
#include "ui/properties/object_properties_base.h"

class CreaturePropertiesWindow : public ObjectPropertiesWindowBase {
public:
	// `tile` is the copy the dialog edits; a monster home's radius can give it a spawn.
	CreaturePropertiesWindow(wxWindow* parent, const Map* map, Tile* tile, Creature* creature, wxPoint pos = wxDefaultPosition);
	virtual ~CreaturePropertiesWindow();

	void OnClickOK(wxCommandEvent&);
	void OnClickCancel(wxCommandEvent&);

protected:
	wxSpinCtrl* count_field;
	wxSpinCtrl* amount_field = nullptr; // sector map monster homes only
	wxSpinCtrl* radius_field = nullptr; // sector map monster homes only
	int initial_radius = 0;
	wxChoice* direction_field;
	Tile* home_tile;
};

#endif
