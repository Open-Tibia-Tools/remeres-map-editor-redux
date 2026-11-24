//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "editor/editor.h"
#include "map/position_format.h"
#include "ui/dialog_util.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/position_paste.h"
#include "ui/toolbar/position_toolbar.h"
#include "util/image_manager.h"
#include <wx/artprov.h>
#include <algorithm>

const wxString PositionToolBar::PANE_NAME = "position_toolbar";

PositionToolBar::PositionToolBar(wxWindow* parent) {
	wxSize icon_size = parent->FromDIP(wxSize(16, 16));

	toolbar = new wxAuiToolBar(parent, TOOLBAR_POSITION, wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_HORZ_TEXT);
	toolbar->SetToolBitmapSize(icon_size);

	x_control = new NumberTextCtrl(toolbar, wxID_ANY, 0, 0, MAP_MAX_WIDTH - 1, wxTE_PROCESS_ENTER, "X", wxDefaultPosition, parent->FromDIP(wxSize(60, 20)));
	x_control->SetToolTip("X Coordinate");

	y_control = new NumberTextCtrl(toolbar, wxID_ANY, 0, 0, MAP_MAX_HEIGHT - 1, wxTE_PROCESS_ENTER, "Y", wxDefaultPosition, parent->FromDIP(wxSize(60, 20)));
	y_control->SetToolTip("Y Coordinate");

	z_control = new NumberTextCtrl(toolbar, wxID_ANY, 0, 0, MAP_MAX_LAYER, wxTE_PROCESS_ENTER, "Z", wxDefaultPosition, parent->FromDIP(wxSize(35, 20)));
	z_control->SetToolTip("Z Coordinate");

	go_button = new wxButton(toolbar, TOOLBAR_POSITION_GO, wxEmptyString, wxDefaultPosition, parent->FromDIP(wxSize(22, 20)));
	go_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_LOCATION_ARROW));
	go_button->SetToolTip("Go To Position");

	toolbar->AddControl(x_control);
	toolbar->AddControl(y_control);
	toolbar->AddControl(z_control);
	toolbar->AddControl(go_button);
	toolbar->Realize();

	x_control->Bind(wxEVT_KEY_UP, &PositionToolBar::OnKeyUp, this);
	y_control->Bind(wxEVT_KEY_UP, &PositionToolBar::OnKeyUp, this);
	z_control->Bind(wxEVT_KEY_UP, &PositionToolBar::OnKeyUp, this);
	go_button->Bind(wxEVT_BUTTON, &PositionToolBar::OnGoClick, this);

	BindPositionPaste(x_control, y_control, z_control, [] {
		const Editor* editor = g_gui.GetCurrentEditor();
		return editor ? PositionBounds { editor->map.getWidth(), editor->map.getHeight() } : PositionBounds {};
	});
}

PositionToolBar::~PositionToolBar() {
	x_control->Unbind(wxEVT_KEY_UP, &PositionToolBar::OnKeyUp, this);
	y_control->Unbind(wxEVT_KEY_UP, &PositionToolBar::OnKeyUp, this);
	z_control->Unbind(wxEVT_KEY_UP, &PositionToolBar::OnKeyUp, this);
	go_button->Unbind(wxEVT_BUTTON, &PositionToolBar::OnGoClick, this);
}

void PositionToolBar::Update() {
	Editor* editor = g_gui.GetCurrentEditor();
	bool has_map = editor != nullptr;

	toolbar->EnableTool(TOOLBAR_POSITION_GO, has_map);
	x_control->Enable(has_map);
	y_control->Enable(has_map);
	z_control->Enable(has_map);

	if (has_map) {
		x_control->SetMaxValue(std::max(editor->map.getWidth() - 1, 0));
		y_control->SetMaxValue(std::max(editor->map.getHeight() - 1, 0));
	}
	toolbar->Refresh();
}

void PositionToolBar::GoToEnteredPosition()
{
    Editor* editor = g_gui.GetCurrentEditor();
    if (!editor) {
        return;
    }

    x_control->CheckRange();
    y_control->CheckRange();
    z_control->CheckRange();
    const Position pos(x_control->GetIntValue(), y_control->GetIntValue(), z_control->GetIntValue());
    const PositionBounds bounds {editor->map.getWidth(), editor->map.getHeight()};
    if (bounds.contains(pos)) {
        g_gui.SetScreenCenterPosition(pos);
    } else {
        DialogUtil::ShowRichToolTip(
            "Invalid Coordinates", "Coordinates are out of bounds for the current map.", go_button, nullptr, wxICON_WARNING
        );
    }
}

void PositionToolBar::OnGoClick(wxCommandEvent& event)
{
    if (event.GetId() == TOOLBAR_POSITION_GO) {
        GoToEnteredPosition();
    }
}

void PositionToolBar::OnKeyUp(wxKeyEvent& event) {
	if (event.GetKeyCode() == WXK_TAB) {
		if (x_control->HasFocus()) {
			y_control->SelectAll();
			y_control->SetFocus();
		} else if (y_control->HasFocus()) {
			z_control->SelectAll();
			z_control->SetFocus();
		} else if (z_control->HasFocus()) {
			go_button->SetFocus();
		}
	} else if (event.GetKeyCode() == WXK_NUMPAD_ENTER || event.GetKeyCode() == WXK_RETURN) {
        GoToEnteredPosition();
    }
    event.Skip();
}
