//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "map/tile_operations.h"
#include "app/main.h"
#include "rendering/ui/popup_action_handler.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "map/tile.h"
#include "game/item.h"
#include "ui/gui.h"
#include "ui/browse_tile_window.h"
#include "ui/dialog_helper.h"
#include "brushes/brush.h"
#include "brushes/door/door_brush.h"

#include <optional>

namespace {

	void CommitTileChange(Editor& editor, std::unique_ptr<Tile> tile, ActionIdentifier type) {
		std::unique_ptr<Action> action = editor.actionQueue->createAction(type);
		action->addChange(std::make_unique<Change>(std::move(tile)));
		editor.actionQueue->addAction(std::move(action));
		g_gui.RefreshView();
	}

	void ApplyItemRotation(Item* item, bool backwards) {
		if (backwards) {
			item->doRotateBackwards();
		} else {
			item->doRotate();
		}
	}

}

void PopupActionHandler::RotateItem(Editor& editor, bool backwards) {
	if (editor.selection.size() != 1) {
		return;
	}

	Tile* tile = editor.selection.getSelectedTile();
	if (!tile) {
		return;
	}

	std::unique_ptr<Tile> new_tile(TileOperations::deepCopy(tile, editor.map));
	ItemVector selected_items = TileOperations::getSelectedItems(new_tile.get());

	Item* target = nullptr;
	for (Item* item : selected_items) {
		if (item && item->isRoteable()) {
			target = item;
			break;
		}
	}
	if (!target) {
		return;
	}

	ApplyItemRotation(target, backwards);
	CommitTileChange(editor, std::move(new_tile), ACTION_ROTATE_ITEM);
}

void PopupActionHandler::RotateItemAt(Editor& editor, const Position& pos, bool backwards) {
	Tile* tile = editor.map.getTile(pos);
	if (!tile) {
		return;
	}

	std::optional<size_t> target_index;
	bool target_is_door = false;
	for (size_t i = tile->items.size(); i-- > 0;) {
		Item* item = tile->items[i].get();
		if (!item) {
			continue;
		}
		if (item->isBrushDoor()) {
			target_index = i;
			target_is_door = true;
			break;
		}
		if (item->isRoteable()) {
			target_index = i;
			break;
		}
	}
	if (!target_index.has_value()) {
		return;
	}

	std::unique_ptr<Tile> new_tile(TileOperations::deepCopy(tile, editor.map));
	if (*target_index >= new_tile->items.size()) {
		return;
	}

	Item* target = new_tile->items[*target_index].get();
	if (!target) {
		return;
	}

	if (target_is_door) {
		DoorBrush::switchDoor(target);
		CommitTileChange(editor, std::move(new_tile), ACTION_SWITCHDOOR);
	} else {
		ApplyItemRotation(target, backwards);
		CommitTileChange(editor, std::move(new_tile), ACTION_ROTATE_ITEM);
	}
}

void PopupActionHandler::GotoDestination(Editor& editor) {
	Tile* tile = editor.selection.getSelectedTile();
	ItemVector selected_items = TileOperations::getSelectedItems(tile);
	ASSERT(!selected_items.empty());
	Teleport* teleport = dynamic_cast<Teleport*>(selected_items.front());
	if (teleport) {
		Position pos = teleport->getDestination();
		g_gui.SetScreenCenterPosition(pos);
	}
}

void PopupActionHandler::SwitchDoor(Editor& editor) {
	if (editor.selection.size() != 1) {
		return;
	}

	Tile* tile = editor.selection.getSelectedTile();
	if (!tile) {
		return;
	}

	std::unique_ptr<Tile> new_tile(TileOperations::deepCopy(tile, editor.map));
	Item* door = nullptr;
	for (Item* item : TileOperations::getSelectedItems(new_tile.get())) {
		if (item && item->isBrushDoor()) {
			door = item;
			break;
		}
	}
	if (!door) {
		return;
	}

	DoorBrush::switchDoor(door);
	CommitTileChange(editor, std::move(new_tile), ACTION_SWITCHDOOR);
}

void PopupActionHandler::BrowseTile(Editor& editor, int cursor_x, int cursor_y) {
	if (editor.selection.size() != 1) {
		return;
	}

	Tile* tile = editor.selection.getSelectedTile();
	if (!tile) {
		return;
	}
	ASSERT(tile->isSelected());
	std::unique_ptr<Tile> new_tile(TileOperations::deepCopy(tile, editor.map));

	wxDialog* w = new BrowseTileWindow(g_gui.root, new_tile.get(), wxPoint(cursor_x, cursor_y));

	int ret = w->ShowModal();
	if (ret != 0) {
		std::unique_ptr<Action> action = editor.actionQueue->createAction(ACTION_DELETE_TILES);
		action->addChange(std::make_unique<Change>(std::move(new_tile)));
		editor.addAction(std::move(action));
	}

	w->Destroy();
}

void PopupActionHandler::OpenProperties(Editor& editor) {
	if (editor.selection.size() != 1) {
		return;
	}

	Tile* tile = editor.selection.getSelectedTile();
	if (tile) {
		DialogHelper::OpenProperties(editor, tile);
	}
}
