//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_UI_SELECTION_CONTROLLER_H_
#define RME_RENDERING_UI_SELECTION_CONTROLLER_H_

#include <wx/event.h>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>
#include "editor/lasso_tile_outline.h"
#include "map/position.h"
#include "rendering/ui/lasso_gesture.h"
#include "rendering/ui/selection_picker.h"

class MapCanvas;
class Editor;
class Tile;
class SelectionThread;
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
	void HandleWandClick(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, bool ctrl_down);
	bool ConsumeWandRelease();
	void HandleDrag(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down);
	void HandleRelease(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down);
	void HandlePropertiesClick(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, bool shift_down, bool ctrl_down, bool alt_down);
	void HandlePropertiesRelease(const Position& mouse_map_pos, bool shift_down, bool ctrl_down, bool alt_down);
	void HandleDoubleClick(const Position& mouse_map_pos, bool shift_down, bool alt_down);

	bool IsDragging() const {
		return dragging;
	}
	bool IsBoundboxSelection() const {
		return boundbox_selection;
	}

	// Clears the whole selection (every floor) as one undoable step. The keyboard ESC path
	// shares it with the click handlers.
	void ClearSelection();

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
		magic_wand_click = false;
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

	// Rebuilds the lasso tile outline on input events. No mask, no chunk-cache interaction.
	void UpdateLassoPreview();
	// Cheap render-loop guard: drop a polygon built for a different view without allocating.
	void ValidateLassoPreviewView();
	[[nodiscard]] std::span<const LassoTileSegment> GetLassoOutlineSegments() const noexcept {
		return outline_scratch.segments;
	}
	[[nodiscard]] bool HasLassoOutline() const noexcept {
		return lasso_preview_polygon.has_value();
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
	void FillLassoPolygonPoints(std::span<const LassoScreenPoint> points);
	[[nodiscard]] LassoPoint ConvertLassoPoint(const LassoScreenPoint& point) const;
	// Converts only the points appended since the last call; a cursor move costs O(delta).
	void UpdateLassoTilePoints(std::span<const LassoScreenPoint> points, bool append_cursor, const LassoScreenPoint& cursor);
	void ClearLassoPreview();	void ExecuteLassoSelection(std::span<const LassoScreenPoint> points, LassoMode mode);

	[[nodiscard]] SelectionFloorScope ComputeFloorScope(const SelectionTileRect& rect, int floor) const;
	[[nodiscard]] std::vector<std::unique_ptr<SelectionThread>> CreateSelectionThreads(const SelectionFloorScope& scope, const LassoPolygon* polygon, int polygon_offset, bool deselect) const;
	void RunSelectionThreads(std::vector<std::unique_ptr<SelectionThread>> threads);

	[[nodiscard]] Tile* FindSelectedTileUnderCursor(const Position& mouse_map_pos) const;
	[[nodiscard]] SelectionHit PickAt(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset) const;
	[[nodiscard]] bool IsHitSelected(const SelectionHit& hit) const;
	void ApplyHit(const Position& mouse_map_pos, const SelectionPixelOffset& pixel_offset, const SelectionHit& hit, bool toggle);
	void ExecuteBoundboxSelection(const Position& start_pos, const Position& end_pos, int floor);

	MapCanvas* canvas;
	Editor& editor;

	bool dragging;
	bool boundbox_selection;
	bool magic_wand_click = false; // The press was consumed by the magic wand; the release does nothing.

	Position drag_start_pos;
	SelectionPixelOffset last_click_pixel;

	LassoGesture lasso;
	LassoViewport lasso_viewport;
	std::optional<LassoPolygon> lasso_preview_polygon;
	std::size_t lasso_preview_points = 0;
	LassoScreenPoint lasso_preview_cursor {};
	std::vector<LassoPoint> lasso_tile_points; // Reused screen -> polygon-tile conversion
	std::size_t lasso_tile_points_converted = 0; // Sampled points already converted this gesture
	LassoOutline::Scratch outline_scratch; // Reused boundary computation buffers

	// Cached status key; the formatted string is only rebuilt when one of these changes.
	LassoMode lasso_status_mode = LassoMode::Replace;
	std::size_t lasso_status_points = 0;
	bool lasso_status_at_limit = false;
	bool lasso_status_valid = false;
};

#endif
