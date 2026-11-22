//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "rendering/ui/map_menu_handler.h"
#include "rendering/ui/map_display.h"
#include "ui/gui_ids.h"
#include "ui/tile_context_actions.h"

MapMenuHandler::MapMenuHandler(MapCanvas* canvas, Editor& editor) :
	canvas(canvas),
	editor(editor) {
}

void MapMenuHandler::BindEvents() {
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnMenuCommand, this, MAP_POPUP_MENU_CUT, MAP_POPUP_MENU_ADD_TO_COLLECTION);
	canvas->Bind(wxEVT_MENU, &MapMenuHandler::OnMenuCommand, this, MAP_POPUP_MENU_SCRIPT_FIRST, MAP_POPUP_MENU_SCRIPT_LAST);
}

void MapMenuHandler::OnMenuCommand(wxCommandEvent& event) {
	TileContextActions::Execute(
		event.GetId(),
		TileContextTarget {
			.editor = editor,
			.floor = canvas->GetFloor(),
			.paste_position = canvas->GetCursorPosition(),
			.script_position = Position(canvas->last_click_map_x, canvas->last_click_map_y, canvas->last_click_map_z),
			.dialog_position = wxPoint(canvas->cursor_x, canvas->cursor_y),
		}
	);
}
