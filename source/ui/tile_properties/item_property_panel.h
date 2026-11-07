//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_ITEM_PROPERTY_PANEL_H_
#define RME_UI_ITEM_PROPERTY_PANEL_H_

#include "ui/tile_properties/custom_property_panel.h"
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/sizer.h>

class ItemPropertyPanel : public CustomPropertyPanel {
public:
	ItemPropertyPanel(wxWindow* parent);
	virtual ~ItemPropertyPanel();

	void SetItem(Item* item, Tile* tile, Map* map) override;

	void OnActionIdChange(wxSpinEvent& event);
	void OnUniqueIdChange(wxSpinEvent& event);
	void OnCountChange(wxSpinEvent& event);
	void OnSplashTypeChange(wxCommandEvent& event);
	void OnTextChange(wxEvent& event);

protected:
	Item* current_item;
	Tile* current_tile;
	Map* current_map;

	wxSpinCtrl* action_id_spin;
	wxSpinCtrl* unique_id_spin;

	wxStaticBoxSizer* count_sizer;
	wxSpinCtrl* count_spin;
	wxChoice* splash_type_choice;

	wxStaticBoxSizer* text_sizer;
	wxTextCtrl* text_ctrl;
	wxStaticText* item_info_label = nullptr;
};


#endif
