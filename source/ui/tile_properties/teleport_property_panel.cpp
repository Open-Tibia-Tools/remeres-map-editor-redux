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
#include "ui/numbertextctrl.h"
#include "ui/position_paste.h"

#include <wx/weakref.h>

#include <algorithm>

TeleportPropertyPanel::TeleportPropertyPanel(wxWindow* parent) :
	ItemPropertyPanel(parent) {

	dest_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Destination");
	wxFlexGridSizer* dest_grid = new wxFlexGridSizer(3, 2, FromDIP(5), FromDIP(5));
	dest_grid->AddGrowableCol(1);

	// Plain text fields, not wxSpinCtrl: the native spin control on wxMSW does not pass
	// WM_PASTE on, so a pasted position could not fill all three fields.
	wxWindow* box = dest_sizer->GetStaticBox();
	dest_grid->Add(new wxStaticText(box, wxID_ANY, "X:"), wxSizerFlags().CenterVertical());
	x_field = new NumberTextCtrl(box, wxID_ANY, 0, 0, MAP_MAX_WIDTH - 1, wxTE_PROCESS_ENTER, "X");
	dest_grid->Add(x_field, wxSizerFlags(1).Expand());

	dest_grid->Add(new wxStaticText(box, wxID_ANY, "Y:"), wxSizerFlags().CenterVertical());
	y_field = new NumberTextCtrl(box, wxID_ANY, 0, 0, MAP_MAX_HEIGHT - 1, wxTE_PROCESS_ENTER, "Y");
	dest_grid->Add(y_field, wxSizerFlags(1).Expand());

	dest_grid->Add(new wxStaticText(box, wxID_ANY, "Z:"), wxSizerFlags().CenterVertical());
	z_field = new NumberTextCtrl(box, wxID_ANY, 0, 0, MAP_MAX_LAYER, wxTE_PROCESS_ENTER, "Z");
	dest_grid->Add(z_field, wxSizerFlags(1).Expand());

	dest_sizer->Add(dest_grid, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(5)));
	GetSizer()->Add(dest_sizer, wxSizerFlags().Expand().Border(wxALL, FromDIP(5)));

	for (NumberTextCtrl* field : { x_field, y_field, z_field }) {
        field->Bind(wxEVT_TEXT, &TeleportPropertyPanel::OnFieldText, this);
        field->Bind(wxEVT_TEXT_ENTER, &TeleportPropertyPanel::OnFieldEnter, this);
        field->Bind(wxEVT_KILL_FOCUS, &TeleportPropertyPanel::OnFieldKillFocus, this);
    }
    const wxWeakRef<TeleportPropertyPanel> self(this);
    BindPositionPaste(
        x_field, y_field, z_field,
        [self] {
            const Editor* editor = g_gui.GetCurrentEditor();
            return (self && editor && self->current_map == &editor->map) ? PositionBounds {editor->map.getWidth(), editor->map.getHeight()}
                                                                         : PositionBounds {};
        },
        [self](const Position&) {
            if (self) {
                self->CommitDestination(true);
            }
        }
    );
}

TeleportPropertyPanel::~TeleportPropertyPanel() {
    for (NumberTextCtrl* field : {x_field, y_field, z_field}) {
        if (field) {
            field->Unbind(wxEVT_TEXT, &TeleportPropertyPanel::OnFieldText, this);
            field->Unbind(wxEVT_TEXT_ENTER, &TeleportPropertyPanel::OnFieldEnter, this);
            field->Unbind(wxEVT_KILL_FOCUS, &TeleportPropertyPanel::OnFieldKillFocus, this);
        }
    }
}

void TeleportPropertyPanel::OnFieldText(wxCommandEvent& event)
{
    user_edited = true;
    event.Skip();
}

void TeleportPropertyPanel::OnFieldEnter(wxCommandEvent& event)
{
    event.Skip();
    CommitDestination(false);
}

void TeleportPropertyPanel::OnFieldKillFocus(wxFocusEvent& event)
{
    event.Skip();
    CommitDestination(false);
}

void TeleportPropertyPanel::SetItem(Item* item, Tile* tile, Map* map) {
	ItemPropertyPanel::SetItem(item, tile, map);

	if (item && item->asTeleport()) {
		Teleport* teleport = static_cast<Teleport*>(item);
		GetSizer()->Show(dest_sizer, true);
		if (map) {
			x_field->SetMaxValue(std::max(map->getWidth() - 1, 0));
			y_field->SetMaxValue(std::max(map->getHeight() - 1, 0));
		}
		x_field->SetIntValue(teleport->getX());
		y_field->SetIntValue(teleport->getY());
		z_field->SetIntValue(teleport->getZ());
		tile_position = tile ? tile->getPosition() : Position();
	} else {
		GetSizer()->Show(dest_sizer, false);
	}
	// SetIntValue sends wxEVT_TEXT; loading a teleport is not an edit.
	user_edited = false;
	Layout();
	GetParent()->Layout();
}

void TeleportPropertyPanel::CommitDestination(bool force) {
	if (!force && !user_edited) {
		return;
	}
	// The panel keeps its last tile after the selection, the editor or the map changed, and a
	// focus change can arrive after that tile was freed. Only touch it while it is still the
	// live tile at its position on the current editor's map.
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor || !current_tile || !current_item || current_map != &editor->map || editor->map.getTile(tile_position) != current_tile) {
		return;
	}
	if (current_tile->getIndexOf(current_item) == -1 || !current_item->asTeleport()) {
		return;
	}
	user_edited = false;

	x_field->CheckRange();
	y_field->CheckRange();
	z_field->CheckRange();
	const Position dest(x_field->GetIntValue(), y_field->GetIntValue(), z_field->GetIntValue());
	const Teleport* current = static_cast<const Teleport*>(current_item);
	if (current->getX() == dest.x && current->getY() == dest.y && current->getZ() == dest.z) {
		return;
	}

	std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(current_tile, *current_map);
	int index = current_tile->getIndexOf(current_item);
	if (index != -1) {
		Item* new_item = new_tile->getItemAt(index);
		if (new_item->asTeleport()) {
			Teleport* teleport = static_cast<Teleport*>(new_item);
			teleport->setDestination(dest);

			std::unique_ptr<Action> action = editor->actionQueue->createAction(ACTION_CHANGE_PROPERTIES);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
			editor->addAction(std::move(action));
		}
	}
}
