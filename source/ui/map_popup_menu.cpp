//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"

#include "ui/map_popup_menu.h"

#include <algorithm>
#include <optional>

#include "editor/editor.h"
#include "ui/context_menu_item_facts.h"
#include "ui/context_menu_presentation.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/tile_context_menu_model.h"
#include "brushes/brush.h"
#include "game/item.h"
#include "map/map.h"
#include "map/tile.h"
#include "util/image_manager.h"

#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/table/table_brush.h"
#include "lua/lua_script_manager.h"

namespace {

	void CollectStackFacts(const Tile* tile, TileContextMenu::Facts& facts) {
		const auto note_brush = [&facts](const Brush* brush, bool& has_kind) {
			if (brush && brush->visibleInPalette()) {
				has_kind = true;
				facts.stack_has_collection = facts.stack_has_collection || brush->hasCollection();
			}
		};

		for (const auto& item : tile->items) {
			if (item->isWall()) {
				note_brush(item->getWallBrush(), facts.has_wall_brush);
			}
			if (item->isTable()) {
				note_brush(item->getTableBrush(), facts.has_table_brush);
			}
			if (item->isCarpet()) {
				note_brush(item->getCarpetBrush(), facts.has_carpet_brush);
			}
			if (const Brush* doodad = item->getDoodadBrush()) {
				facts.stack_has_collection = facts.stack_has_collection || doodad->hasCollection();
			}
			if (item->isSelected()) {
				facts.has_selected_stack_item = true;
			}
		}
	}

	TileContextMenu::Facts CollectFacts(Editor& editor) {
		TileContextMenu::Facts facts;
		facts.selected_tile_count = editor.selection.size();
		facts.can_paste = editor.copybuffer.canPaste();

		if (facts.selected_tile_count != 1) {
			return facts;
		}

		Tile* tile = editor.selection.getSelectedTile();
		if (!tile) {
			return facts;
		}

		const GroundBrush* ground_brush = tile->getGroundBrush();
		facts.has_ground = tile->hasGround();
		facts.has_visible_ground_brush = facts.has_ground && ground_brush && ground_brush->visibleInPalette();
		facts.ground_brush_has_collection = ground_brush && ground_brush->hasCollection();
		facts.has_creature = tile->creature != nullptr;
		facts.has_spawn = tile->spawn != nullptr;
		facts.is_house_tile = tile->isHouseTile();
		CollectStackFacts(tile, facts);

		const ItemVector selected_items = TileOperations::getSelectedItems(tile);
		if (selected_items.size() == 1) {
			facts.selected_item = ContextMenu::CollectItemFacts(*selected_items.back());
		}

		if (g_luaScripts.isInitialized()) {
			constexpr std::size_t script_slots = MAP_POPUP_MENU_SCRIPT_LAST - MAP_POPUP_MENU_SCRIPT_FIRST + 1;
			facts.script_count = std::min(g_luaScripts.getContextMenuItems().size(), script_slots);
		}
		return facts;
	}

	std::optional<ContextMenu::EntryPresentation> PresentTile(TileContextMenu::Command command) {
		using TileContextMenu::Command;
		switch (command) {
			case Command::Cut:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_CUT);
			case Command::Copy:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_COPY);
			case Command::CopyPosition:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_COPY_POSITION);
			case Command::Paste:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_PASTE);
			case Command::Delete:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_DELETE);
			case Command::AdvancedReplace:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_ADVANCED_REPLACE);
			case Command::AddToCollection:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_ADD_TO_COLLECTION);
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
			case Command::SelectCreature:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_CREATURE_BRUSH);
			case Command::SelectSpawn:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_SPAWN_BRUSH);
			case Command::SelectRaw:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_RAW_BRUSH);
			case Command::SelectWall:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_WALL_BRUSH);
			case Command::SelectCarpet:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_CARPET_BRUSH);
			case Command::SelectTable:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_TABLE_BRUSH);
			case Command::SelectDoodad:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_DOODAD_BRUSH);
			case Command::SelectDoor:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_DOOR_BRUSH);
			case Command::SelectGround:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_GROUND_BRUSH);
			case Command::SelectCollection:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_COLLECTION_BRUSH);
			case Command::SelectHouse:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_SELECT_HOUSE_BRUSH);
			case Command::Properties:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_PROPERTIES);
			case Command::BrowseField:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_BROWSE_TILE);
			case Command::TileProperties:
				return ContextMenu::PresentMenuEntry(MAP_POPUP_MENU_TILE_PROPERTIES);
			case Command::Separator:
			case Command::Script:
				break;
		}
		return std::nullopt;
	}

} // namespace

MapPopupMenu::MapPopupMenu(Editor& editor) :
	wxMenu(""), editor(editor) {
	////
}

MapPopupMenu::~MapPopupMenu() {
	////
}

void MapPopupMenu::Update() {
	// Clear the menu of all items
	while (GetMenuItemCount() != 0) {
		wxMenuItem* m_item = FindItemByPosition(0);
		// If you add a submenu, this won't delete it.
		Delete(m_item);
	}

	for (const TileContextMenu::Entry& entry : TileContextMenu::Build(CollectFacts(editor))) {
		if (entry.command == TileContextMenu::Command::Separator) {
			AppendSeparator();
			continue;
		}

		if (entry.command == TileContextMenu::Command::Script) {
			const auto& script_items = g_luaScripts.getContextMenuItems();
			Append(MAP_POPUP_MENU_SCRIPT_FIRST + static_cast<int>(entry.script_index), wxString::FromUTF8(script_items[entry.script_index].label));
			continue;
		}

		const std::optional<ContextMenu::EntryPresentation> presentation = PresentTile(entry.command);
		if (!presentation) {
			continue;
		}

		wxMenuItem* menu_item = Append(presentation->id, presentation->label, presentation->help);
		menu_item->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(presentation->icon));
		menu_item->Enable(entry.enabled);
	}
}
