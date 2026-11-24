//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_UI_MAP_MENU_HANDLER_H_
#define RME_RENDERING_UI_MAP_MENU_HANDLER_H_

#include "app/main.h"
#include <wx/event.h>

class MapCanvas;
class Editor;

// Map canvas adapter for the shared tile context menu: forwards the popup menu commands to
// TileContextActions with the canvas floor, cursor and last right-click position.
class MapMenuHandler {
public:
	MapMenuHandler(MapCanvas* canvas, Editor& editor);
	~MapMenuHandler() = default;

	void BindEvents();

private:
	void OnMenuCommand(wxCommandEvent& event);

	MapCanvas* canvas;
	Editor& editor;
};

#endif
