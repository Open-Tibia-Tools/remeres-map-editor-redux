//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/definitions.h"

#include "brushes/brush_utility.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "brushes/brush.h"
#include "brushes/ground/ground_brush.h"
#include "map/map.h"
#include "map/tile.h"

bool BrushUtility::processed[BrushUtility::BLOCK_SIZE * BrushUtility::BLOCK_SIZE] = { false };
int BrushUtility::countMaxFills = 0;

void BrushUtility::GetTilesToDraw(int mouse_map_x, int mouse_map_y, int floor, std::vector<Position>* tilestodraw, std::vector<Position>* tilestoborder, bool fill) {
	if (fill) {
		Brush* brush = g_gui.GetCurrentBrush();
		if (!brush || !brush->is<GroundBrush>()) {
			return;
		}

		GroundBrush* newBrush = brush->as<GroundBrush>();
		Position position(mouse_map_x, mouse_map_y, floor);

		Tile* tile = g_gui.GetCurrentMap().getTile(position);
		GroundBrush* oldBrush = nullptr;
		if (tile) {
			oldBrush = tile->getGroundBrush();
		}

		if (oldBrush && oldBrush->getID() == newBrush->getID()) {
			return;
		}

		if ((tile && tile->ground && !oldBrush) || (!tile && oldBrush)) {
			return;
		}

		if (tile && oldBrush) {
			GroundBrush* groundBrush = tile->getGroundBrush();
			if (!groundBrush || groundBrush->getID() != oldBrush->getID()) {
				return;
			}
		}

		std::fill(std::begin(processed), std::end(processed), false);
		countMaxFills = 0;
		FloodFill(&g_gui.GetCurrentMap(), position, BLOCK_SIZE / 2, BLOCK_SIZE / 2, oldBrush, tilestodraw);

	} else {
		const BrushFootprint footprint = g_gui.GetBrushFootprint();
		for (int y = footprint.min_offset_y - 1; y <= footprint.max_offset_y + 1; ++y) {
			for (int x = footprint.min_offset_x - 1; x <= footprint.max_offset_x + 1; ++x) {
				if (footprint.containsOffset(x, y)) {
					if (tilestodraw) {
						tilestodraw->push_back(Position(mouse_map_x + x, mouse_map_y + y, floor));
					}
				}

				for (int check_y = y - 1; check_y <= y + 1; ++check_y) {
					for (int check_x = x - 1; check_x <= x + 1; ++check_x) {
						if (!footprint.containsOffset(check_x, check_y)) {
							continue;
						}
						if (tilestoborder) {
							tilestoborder->push_back(Position(mouse_map_x + x, mouse_map_y + y, floor));
						}
						check_y = y + 2;
						break;
					}
				}
			}
		}
	}
}

bool BrushUtility::FloodFill(Map* map, const Position& center, int x, int y, GroundBrush* brush, std::vector<Position>* positions) {
	countMaxFills++;
	if (countMaxFills > (BLOCK_SIZE * 4 * 4)) {
		countMaxFills = 0;
		return true;
	}

	if (x <= 0 || y <= 0 || x >= BLOCK_SIZE || y >= BLOCK_SIZE) {
		return false;
	}

	processed[GetFillIndex(x, y)] = true;

	int px = (center.x + x) - (BLOCK_SIZE / 2);
	int py = (center.y + y) - (BLOCK_SIZE / 2);
	if (px <= 0 || py <= 0 || px >= map->getWidth() || py >= map->getHeight()) {
		return false;
	}

	Tile* tile = map->getTile(px, py, center.z);
	if ((tile && tile->ground && !brush) || (!tile && brush)) {
		return false;
	}

	if (tile && brush) {
		GroundBrush* groundBrush = tile->getGroundBrush();
		if (!groundBrush || groundBrush->getID() != brush->getID()) {
			return false;
		}
	}

	positions->push_back(Position(px, py, center.z));

	bool deny = false;
	if (!processed[GetFillIndex(x - 1, y)]) {
		deny = FloodFill(map, center, x - 1, y, brush, positions);
	}

	if (!deny && !processed[GetFillIndex(x, y - 1)]) {
		deny = FloodFill(map, center, x, y - 1, brush, positions);
	}

	if (!deny && !processed[GetFillIndex(x + 1, y)]) {
		deny = FloodFill(map, center, x + 1, y, brush, positions);
	}

	if (!deny && !processed[GetFillIndex(x, y + 1)]) {
		deny = FloodFill(map, center, x, y + 1, brush, positions);
	}

	return deny;
}
