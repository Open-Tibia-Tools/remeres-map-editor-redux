//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_POPUP_MENU_H_
#define RME_MAP_POPUP_MENU_H_

#include <wx/wx.h>

class Editor;

// Tile context menu for the current editor selection. Shared by the map canvas and the
// Tile Properties item list; entries come from TileContextMenu::Build, actions run through TileContextActions.
class MapPopupMenu : public wxMenu {
public:
	MapPopupMenu(Editor& editor);
	virtual ~MapPopupMenu();

	void Update();

protected:
	Editor& editor;
};

#endif
