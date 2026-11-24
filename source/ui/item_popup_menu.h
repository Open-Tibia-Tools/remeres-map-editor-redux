#ifndef RME_UI_ITEM_POPUP_MENU_H_
#define RME_UI_ITEM_POPUP_MENU_H_

#include <wx/wx.h>

#include "ui/item_context_menu_model.h"

// Item-scoped context menu for the Tile Properties list. Entries come from ItemContextMenu::Build
// and actions run through ItemContextActions; it never touches the map canvas menu.
class ItemPopupMenu : public wxMenu {
public:
	ItemPopupMenu();
	virtual ~ItemPopupMenu();

	void Update(const ItemContextMenu::Facts& facts);
};

#endif
