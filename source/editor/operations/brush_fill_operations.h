//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_OPERATIONS_BRUSH_FILL_OPERATIONS_H
#define RME_EDITOR_OPERATIONS_BRUSH_FILL_OPERATIONS_H

#include "editor/connected_positions.h"
#include "map/position.h"
#include "map/position_rect.h"

#include <span>

class Brush;
class Editor;

enum class BrushFillResult {
	Drawn,
	// A door brush found no wall among the positions.
	NoDrawableTile,
};

// Draws a ground, wall, door, doodad, carpet, table or RAW brush on an explicit list of
// positions on one floor, through DrawOperations, exactly like the editor's own fills: border
// targets (patch + 8-neighbour ring) for bordered brushes, one re-rolled composite per tile for
// doodads. The brush and automagic are explicit, so the GUI's brush, size, mode and selection are
// never touched. One fill is one undoable ActionQueue entry.
namespace BrushFillOperations {

	// Tiles whose borders a fill of `patch` recomputes: the patch itself, then its 8-neighbour ring
	// inside `bounds` (the map). The same set DrawingController's rectangle fill builds. Pure.
	[[nodiscard]] inline PositionVector borderTargets(std::span<const Position> patch, const PositionRect& bounds) {
		PositionVector targets(patch.begin(), patch.end());
		const PositionVector ring = ConnectedPositions::borderRing(patch, &bounds);
		targets.insert(targets.end(), ring.begin(), ring.end());
		return targets;
	}

	[[nodiscard]] BrushFillResult fill(Editor& editor, Brush& brush, std::span<const Position> positions, bool automagic);

} // namespace BrushFillOperations

#endif
