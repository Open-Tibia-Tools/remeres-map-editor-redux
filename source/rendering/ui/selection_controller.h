//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_UI_SELECTION_CONTROLLER_H_
#define RME_RENDERING_UI_SELECTION_CONTROLLER_H_

#include <wx/event.h>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include "map/position.h"
#include "rendering/ui/lasso_gesture.h"
#include "rendering/ui/selection_picker.h"

class MapCanvas;
class Editor;
class Tile;
class SelectionThread;
class LassoPolygon;
struct SelectionFloorScope;
struct SelectionTileRect;

[[nodiscard]] inline LassoModifiers ToLassoModifiers(const wxKeyboardState& state) {
	return LassoModifiers {
		.alt = state.AltDown(),
		.shift = state.ShiftDown(),
		.ctrl = state.ControlDown(),
	};
}

class SelectionController {
public:
	SelectionController(MapCanvas* canvas, Editor& editor);
	~SelectionController();

	void HandleClick(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, bool shift_down, bool ctrl_down, bool alt_down);
	void HandleDrag(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down);
	void HandleRelease(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down);
	void HandlePropertiesClick(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, bool shift_down, bool ctrl_down, bool alt_down);
	void HandlePropertiesRelease(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down);
	void HandleDoubleClick(const Position& mouse_map_pos);

	bool IsDragging() const {
		return dragging;
	}
	bool IsBoundboxSelection() const {
		return boundbox_selection;
	}

	void StartDragging(const Position& start_pos) {
		dragging = true;
		drag_start_pos = start_pos;
	}

	Position GetDragStartPosition() const {
		return drag_start_pos;
	}

	void Reset() {
		dragging = false;
		boundbox_selection = false;
		last_click_pixel = {};
	}

	// Alt-held lasso selection (see LassoGesture). Each handler returns true when the lasso
	// consumed the input and the canvas must not run its normal handling.
	bool HandleLassoButtonDown(LassoButton button, LassoScreenPoint point, LassoModifiers mods);
	bool HandleLassoButtonUp(LassoButton button, LassoScreenPoint point, LassoModifiers mods);
	bool HandleLassoMotion(LassoScreenPoint point, LassoModifiers mods, bool left_down);
	bool HandleLassoKeyDown(int key_code, LassoModifiers mods);
	bool HandleLassoKeyUp(int key_code, LassoModifiers mods);
	void CancelLasso();
	const LassoGesture& GetLasso() const {
		return lasso;
	}

private:
	// View state the screen-space lasso points depend on; navigation is frozen while drawing.
	struct LassoViewport {
		int view_x = 0;
		int view_y = 0;
		// A resize or DPI change maps the same screen points to other tiles.
		int screen_width = 0;
		int screen_height = 0;
		double content_scale = 1.0;
		double zoom = 1.0;
		int floor = 0;

		friend bool operator==(const LassoViewport&, const LassoViewport&) = default;
	};

	[[nodiscard]] bool CanStartLasso() const;
	[[nodiscard]] LassoViewport CaptureLassoViewport() const;
	void ApplyLassoEvent(LassoEvent event);
	void SyncLassoMouseCapture();
	void ExecuteLassoSelection(std::span<const LassoScreenPoint> points, LassoMode mode);

	[[nodiscard]] SelectionFloorScope ComputeFloorScope(const SelectionTileRect& rect, int floor) const;
	[[nodiscard]] std::vector<std::unique_ptr<SelectionThread>> CreateSelectionThreads(const SelectionFloorScope& scope, const LassoPolygon* polygon, int polygon_offset, bool deselect) const;
	void RunSelectionThreads(std::vector<std::unique_ptr<SelectionThread>> threads);

	[[nodiscard]] Tile* FindSelectedTileUnderCursor(const Position& mouse_map_pos) const;
	[[nodiscard]] SelectionHit PickAt(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset) const;
	[[nodiscard]] bool IsHitSelected(const SelectionHit& hit) const;
	void ApplyHit(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, const SelectionHit& hit, bool toggle);
	void ClearSelection();
	void ExecuteBoundboxSelection(const Position& start_pos, const Position& end_pos, int floor);

	MapCanvas* canvas;
	Editor& editor;

	bool dragging;
	bool boundbox_selection;

	Position drag_start_pos;
	SelectionPixelOffset last_click_pixel;

	LassoGesture lasso;
	LassoViewport lasso_viewport;
	std::string lasso_status; // Last status text shown while drawing
};

#endif
