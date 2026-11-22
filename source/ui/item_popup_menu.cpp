#include "app/main.h"

#include "ui/item_popup_menu.h"

#include <optional>

#include "ui/context_menu_presentation.h"
#include "ui/gui_ids.h"
#include "util/image_manager.h"

namespace {

	std::optional<ContextMenu::EntryPresentation> PresentItem(ItemContextMenu::Command command) {
		using ItemContextMenu::Command;
		switch (command) {
			case Command::Copy:
				return ContextMenu::EntryPresentation { MAP_POPUP_MENU_COPY, "&Copy", "Copy this item", ICON_COPY };
			case Command::Delete:
				return ContextMenu::EntryPresentation { MAP_POPUP_MENU_DELETE, "&Delete", "Remove this item from the tile", ICON_TRASH_CAN };
			case Command::CopyServerId:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_COPY_SERVER_ID);
			case Command::CopyClientId:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_COPY_CLIENT_ID);
			case Command::CopyName:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_COPY_NAME);
			case Command::Rotate:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_ROTATE);
			case Command::GotoDestination:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_GOTO);
			case Command::OpenDoor:
				return ContextMenu::EntryPresentation { MAP_POPUP_MENU_SWITCH_DOOR, "&Open door", "Open this door", ICON_DOOR_OPEN };
			case Command::CloseDoor:
				return ContextMenu::EntryPresentation { MAP_POPUP_MENU_SWITCH_DOOR, "&Close door", "Close this door", ICON_DOOR_CLOSED };
			case Command::SelectRaw:
				return ContextMenu::EntryPresentation { MAP_POPUP_MENU_SELECT_RAW_BRUSH, "Select RAW", "Uses this item as a RAW brush", ICON_CUBE };
			case Command::SelectDoodad:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_DOODAD_BRUSH);
			case Command::SelectDoor:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_DOOR_BRUSH);
			case Command::SelectWall:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_WALL_BRUSH);
			case Command::SelectCarpet:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_CARPET_BRUSH);
			case Command::SelectTable:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_TABLE_BRUSH);
			case Command::SelectGround:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_GROUND_BRUSH);
			case Command::SelectCollection:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_COLLECTION_BRUSH);
			case Command::Separator:
				break;
		}
		return std::nullopt;
	}

} // namespace

ItemPopupMenu::ItemPopupMenu() :
	wxMenu("") {
	////
}

ItemPopupMenu::~ItemPopupMenu() {
	////
}

void ItemPopupMenu::Update(const ItemContextMenu::Facts& facts) {
	while (GetMenuItemCount() != 0) {
		Delete(FindItemByPosition(0));
	}

	for (const ItemContextMenu::Entry& entry : ItemContextMenu::Build(facts)) {
		if (entry.command == ItemContextMenu::Command::Separator) {
			AppendSeparator();
			continue;
		}

		const std::optional<ContextMenu::EntryPresentation> presentation = PresentItem(entry.command);
		if (!presentation) {
			continue;
		}

		wxMenuItem* menu_item = Append(presentation->id, presentation->label, presentation->help);
		menu_item->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(presentation->icon));
		menu_item->Enable(entry.enabled);
	}
}
