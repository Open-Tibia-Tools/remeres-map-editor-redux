#ifndef RME_RENDERING_RENDER_INTERACTION_STATE_H_
#define RME_RENDERING_RENDER_INTERACTION_STATE_H_

#include <optional>
#include "map/position.h"
#include "rendering/drawers/overlays/brush_overlay_drawer.h"

class BaseMap;
class Brush;

struct InteractionRenderState {
	std::optional<MapBounds> selection_bounds;
	std::optional<Position> drag_start_position;
	BrushOverlayDragState brush_drag_state;
	BaseMap* secondary_map = nullptr;
	bool is_pasting = false;
	Brush* current_brush = nullptr;
};

#endif
