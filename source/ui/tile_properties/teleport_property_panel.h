//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_TELEPORT_PROPERTY_PANEL_H_
#define RME_UI_TELEPORT_PROPERTY_PANEL_H_

#include "ui/tile_properties/item_property_panel.h"

class NumberTextCtrl;

class TeleportPropertyPanel : public ItemPropertyPanel {
public:
	TeleportPropertyPanel(wxWindow* parent);
	virtual ~TeleportPropertyPanel();

	void SetItem(Item* item, Tile* tile, Map* map) override;

protected:
    void OnFieldText(wxCommandEvent& event);
    void OnFieldEnter(wxCommandEvent& event);
    void OnFieldKillFocus(wxFocusEvent& event);

    // Applies the destination in the fields as one undoable change, if it differs from the
    // teleport's current one. Runs on Enter and when a field loses focus after the user typed
    // in it, and after a paste (force).
    void CommitDestination(bool force);

    wxStaticBoxSizer* dest_sizer = nullptr;
    NumberTextCtrl* x_field = nullptr;
    NumberTextCtrl* y_field = nullptr;
    NumberTextCtrl* z_field = nullptr;
    // Where the shown tile was when SetItem ran, to check that it is still the live tile.
    Position tile_position;
    // Set by typing in a field, cleared when SetItem loads a teleport. A focus change alone
    // must not write a destination that the fields only clamped to the map size.
    bool user_edited = false;
};

#endif
