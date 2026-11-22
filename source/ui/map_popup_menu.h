//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_POPUP_MENU_H_
#define RME_MAP_POPUP_MENU_H_

#include <wx/wx.h>

class Editor;

// Tile context menu for the map canvas. Entries come from TileContextMenu::Build and actions run
// through TileContextActions. The Tile Properties list uses its own ItemPopupMenu/ItemContextActions.
class MapPopupMenu : public wxMenu {
public:
	MapPopupMenu(Editor& editor);
	virtual ~MapPopupMenu();

	void Update();

protected:
	Editor& editor;
};

#endif
