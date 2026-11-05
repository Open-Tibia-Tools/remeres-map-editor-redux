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
#include "rendering/ui/brush_selector.h"
#include "ui/gui.h"
#include "brushes/managers/brush_manager.h"
#include "editor/editor.h"
#include "map/tile.h"
#include "game/item.h"
#include "brushes/brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/door/door_brush.h"

void BrushSelector::SelectRAWBrush(Selection& selection) {
	if (selection.size() != 1) {
		return;
	}
	Tile* tile = selection.getSelectedTile();
	if (!tile) {
		return;
	}
	Item* item = TileOperations::getTopSelectedItem(tile);

	if (item && item->getRAWBrush()) {
		const auto sizeState = g_brush_manager.GetBrushSizeState();
		g_gui.SelectBrush(item->getRAWBrush());
		g_gui.RestoreBrushSizeState(sizeState);
	}
}

void BrushSelector::SelectGroundBrush(Selection& selection) {
	if (selection.size() != 1) {
		return;
	}
	Tile* tile = selection.getSelectedTile();
	if (!tile) {
		return;
	}
	GroundBrush* bb = tile->getGroundBrush();

	if (bb) {
		const auto sizeState = g_brush_manager.GetBrushSizeState();
		g_gui.SelectBrush(bb);
		g_gui.RestoreBrushSizeState(sizeState);
	}
}

void BrushSelector::SelectDoodadBrush(Selection& selection) {
	if (selection.size() != 1) {
		return;
	}
	Tile* tile = selection.getSelectedTile();
	if (!tile) {
		return;
	}
	Item* item = TileOperations::getTopSelectedItem(tile);

	if (item) {
		const auto sizeState = g_brush_manager.GetBrushSizeState();
		g_gui.SelectBrush(item->getDoodadBrush());
		g_gui.RestoreBrushSizeState(sizeState);
	}
}

void BrushSelector::SelectDoorBrush(Selection& selection) {
	if (selection.size() != 1) {
		return;
	}
	Tile* tile = selection.getSelectedTile();
	if (!tile) {
		return;
	}
	Item* item = TileOperations::getTopSelectedItem(tile);

	if (item) {
		const auto sizeState = g_brush_manager.GetBrushSizeState();
		g_gui.SelectBrush(item->getDoorBrush());
		g_gui.RestoreBrushSizeState(sizeState);
	}
}

void BrushSelector::SelectWallBrush(Selection& selection) {
	if (selection.size() != 1) {
		return;
	}
	Tile* tile = selection.getSelectedTile();
	if (!tile) {
		return;
	}
	Item* wall = tile->getWall();
	WallBrush* wb = wall->getWallBrush();

	if (wb) {
		const auto sizeState = g_brush_manager.GetBrushSizeState();
		g_gui.SelectBrush(wb);
		g_gui.RestoreBrushSizeState(sizeState);
	}
}

void BrushSelector::SelectCarpetBrush(Selection& selection) {
	if (selection.size() != 1) {
		return;
	}
	Tile* tile = selection.getSelectedTile();
	if (!tile) {
		return;
	}
	Item* wall = tile->getCarpet();
	CarpetBrush* cb = wall->getCarpetBrush();

	if (cb) {
		const auto sizeState = g_brush_manager.GetBrushSizeState();
		g_gui.SelectBrush(cb);
		g_gui.RestoreBrushSizeState(sizeState);
	}
}

void BrushSelector::SelectTableBrush(Selection& selection) {
	if (selection.size() != 1) {
		return;
	}
	Tile* tile = selection.getSelectedTile();
	if (!tile) {
		return;
	}
	Item* wall = tile->getTable();
	TableBrush* tb = wall->getTableBrush();

	if (tb) {
		const auto sizeState = g_brush_manager.GetBrushSizeState();
		g_gui.SelectBrush(tb);
		g_gui.RestoreBrushSizeState(sizeState);
	}
}

void BrushSelector::SelectHouseBrush(Editor& editor, Selection& selection) {
	Tile* tile = selection.getSelectedTile();
	if (!tile) {
		return;
	}

	if (tile->isHouseTile()) {
		House* house = editor.map.houses.getHouse(tile->getHouseID());
		if (house) {
			g_brush_manager.house_brush->setHouse(house);
			const auto sizeState = g_brush_manager.GetBrushSizeState();
			g_gui.SelectBrush(g_brush_manager.house_brush);
			g_gui.RestoreBrushSizeState(sizeState);
		}
	}
}

void BrushSelector::SelectCollectionBrush(Selection& selection) {
	Tile* tile = selection.getSelectedTile();
	if (!tile) {
		return;
	}

	const auto sizeState = g_brush_manager.GetBrushSizeState();
	for (const auto& item : tile->items) {
		if (item->isWall()) {
			WallBrush* wb = item->getWallBrush();
			if (wb && wb->visibleInPalette() && wb->hasCollection()) {
				g_gui.SelectBrush(wb);
				g_gui.RestoreBrushSizeState(sizeState);
				return;
			}
		}
		if (item->isTable()) {
			TableBrush* tb = item->getTableBrush();
			if (tb && tb->visibleInPalette() && tb->hasCollection()) {
				g_gui.SelectBrush(tb);
				g_gui.RestoreBrushSizeState(sizeState);
				return;
			}
		}
		if (item->isCarpet()) {
			CarpetBrush* cb = item->getCarpetBrush();
			if (cb && cb->visibleInPalette() && cb->hasCollection()) {
				g_gui.SelectBrush(cb);
				g_gui.RestoreBrushSizeState(sizeState);
				return;
			}
		}
		if (Brush* db = item->getDoodadBrush()) {
			if (db && db->visibleInPalette() && db->hasCollection()) {
				g_gui.SelectBrush(db);
				g_gui.RestoreBrushSizeState(sizeState);
				return;
			}
		}
		if (item->isSelected()) {
			RAWBrush* rb = item->getRAWBrush();
			if (rb && rb->hasCollection()) {
				g_gui.SelectBrush(rb);
				g_gui.RestoreBrushSizeState(sizeState);
				return;
			}
		}
	}
	GroundBrush* gb = tile->getGroundBrush();
	if (gb && gb->visibleInPalette() && gb->hasCollection()) {
		g_gui.SelectBrush(gb);
		g_gui.RestoreBrushSizeState(sizeState);
		return;
	}
}

void BrushSelector::SelectCreatureBrush(Selection& selection) {
	Tile* tile = selection.getSelectedTile();
	if (!tile) {
		return;
	}

	if (tile->creature) {
		const auto sizeState = g_brush_manager.GetBrushSizeState();
		g_gui.SelectBrush(tile->creature->getBrush());
		g_gui.RestoreBrushSizeState(sizeState);
	}
}

void BrushSelector::SelectSpawnBrush() {
	const auto sizeState = g_brush_manager.GetBrushSizeState();
	g_gui.SelectBrush(g_brush_manager.spawn_brush);
	g_gui.RestoreBrushSizeState(sizeState);
}

void BrushSelector::SelectSmartBrush(Editor&, Tile* tile) {
	if (tile && tile->size() > 0) {
		// Select visible creature
		if (tile->creature && g_settings.getInteger(Config::SHOW_CREATURES)) {
			CreatureBrush* brush = tile->creature->getBrush();
			if (brush) {
				const auto sizeState = g_brush_manager.GetBrushSizeState();
				g_gui.SelectBrush(brush);
				g_gui.RestoreBrushSizeState(sizeState);
				return;
			}
		}
		// Fall back to item selection
		Item* item = tile->getTopItem();
		if (item && item->getRAWBrush()) {
			const auto sizeState = g_brush_manager.GetBrushSizeState();
			g_gui.SelectBrush(item->getRAWBrush());
			g_gui.RestoreBrushSizeState(sizeState);
		}
	}
}
