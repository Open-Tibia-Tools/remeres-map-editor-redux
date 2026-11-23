//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_DRAWERS_OVERLAYS_LASSO_TILE_OUTLINE_DRAWER_H_
#define RME_RENDERING_DRAWERS_OVERLAYS_LASSO_TILE_OUTLINE_DRAWER_H_

#include "editor/lasso_tile_outline.h"
#include "rendering/core/map_view_math.h"

#include <span>

struct NVGcontext;

// Draws the lasso tile-set boundary the same way SelectionDrawer draws the box selection:
// a translucent white shade over the selected tiles and dashed white lines along the grid.
class LassoTileOutlineDrawer {
public:
	void draw(NVGcontext* vg, std::span<const LassoTileSegment> segments, int floor, const MainMapViewport& viewport) const;
};

#endif
