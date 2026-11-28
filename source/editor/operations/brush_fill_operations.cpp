//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "editor/operations/brush_fill_operations.h"

#include "app/settings.h"
#include "brushes/brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/wall/wall_brush.h"
#include "editor/editor.h"
#include "editor/operations/draw_operations.h"
#include "util/common.h"

#include <vector>

namespace BrushFillOperations {

	BrushFillResult fill(Editor& editor, Brush& brush, std::span<const Position> positions, bool automagic) {
		if (brush.is<DoodadBrush>()) {
			// One composite per tile, re-rolled between tiles when the editor randomizes doodads,
			// like smearing a doodad brush; otherwise the brush's own variation, as selecting it in
			// the palette would use. All of them in one history entry.
			const bool randomize = g_settings.getBoolean(Config::RANDOMIZE_DOODADS);
			const int brush_variation = brush.as<DoodadBrush>()->getVariation();
			std::vector<DoodadPlacement> placements;
			placements.reserve(positions.size());
			for (const Position& position : positions) {
				placements.push_back({ position, randomize ? random(0, 1000000) : brush_variation });
			}
			DrawOperations::drawDoodadsWithBrush(editor, *brush.as<DoodadBrush>(), placements, false);
			return BrushFillResult::Drawn;
		}

		if (!brush.needBorders() && !brush.is<WallBrush>() && !brush.is<DoorBrush>()) {
			DrawOperations::drawWithBrush(editor, brush, PositionVector(positions.begin(), positions.end()), false);
			return BrushFillResult::Drawn;
		}

		PositionVector to_draw;
		if (brush.is<DoorBrush>()) {
			// Doors only go on walls; the other positions are skipped instead of failing the fill.
			for (const Position& position : positions) {
				if (brush.canDraw(&editor.map, position)) {
					to_draw.push_back(position);
				}
			}
			if (to_draw.empty()) {
				return BrushFillResult::NoDrawableTile;
			}
		} else {
			to_draw.assign(positions.begin(), positions.end());
		}

		const PositionRect map_bounds { 0, 0, editor.map.getWidth() - 1, editor.map.getHeight() - 1 };
		PositionVector to_border = borderTargets(to_draw, map_bounds);
		DrawOperations::drawWithBrush(editor, brush, to_draw, to_border, false, automagic);
		return BrushFillResult::Drawn;
	}

} // namespace BrushFillOperations
