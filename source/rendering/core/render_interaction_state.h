#ifndef RME_RENDERING_RENDER_INTERACTION_STATE_H_
#define RME_RENDERING_RENDER_INTERACTION_STATE_H_

#include <optional>
#include "map/position.h"

class BaseMap;
class Brush;

struct BrushOverlayDragState {
	bool is_dragging_draw = false;
	int last_click_map_x = 0;
	int last_click_map_y = 0;
};

struct InteractionRenderState {
	std::optional<MapBounds> selection_bounds;
	std::optional<Position> drag_start_position;
	BrushOverlayDragState brush_drag_state;
	BaseMap* secondary_map = nullptr;
	bool is_pasting = false;
	Brush* current_brush = nullptr;
};

#endif
