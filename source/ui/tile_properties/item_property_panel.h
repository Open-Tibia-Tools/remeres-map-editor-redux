//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_ITEM_PROPERTY_PANEL_H_
#define RME_UI_ITEM_PROPERTY_PANEL_H_

#include "ui/tile_properties/custom_property_panel.h"
#include "game/srv_item_attributes.h"
#include <array>
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
	void OnSrvAttributeChange(rme::srv::ItemAttribute attribute);

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

	// Server item attributes (sector maps); a row shows when the item has the
	// attribute, or can have it and the loaded client reads objects.srv.
	void UpdateSrvAttributes(Item* item);
	wxStaticBoxSizer* srv_sizer = nullptr;
	wxFlexGridSizer* srv_grid = nullptr;
	std::array<wxStaticText*, rme::srv::kItemAttributes.size()> srv_labels {};
	std::array<wxSpinCtrl*, rme::srv::kItemAttributes.size()> srv_spins {};
};


#endif
