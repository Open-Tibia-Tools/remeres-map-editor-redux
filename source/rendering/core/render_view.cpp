#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/floor_visibility_mode.h"
#include "app/definitions.h" // For TILE_SIZE, GROUND_LAYER, MAP_MAX_LAYER
#include "ingame_preview/floor_visibility_calculator.h"
#include <glad/glad.h>
#include <algorithm>

void RenderView::Setup(const ViewportParameters& params, const DrawingOptions& options, const BaseMap* map) {
	mouse_map_x = params.mouse_map_x;
	mouse_map_y = params.mouse_map_y;
	view_scroll_x = params.view_scroll_x;
	view_scroll_y = params.view_scroll_y;
	screensize_x = params.screensize_x;
	screensize_y = params.screensize_y;
	viewport_x = 0;
	viewport_y = 0;

	zoom = params.zoom;
	tile_size = std::max(1, static_cast<int>(TILE_SIZE / zoom)); // after zoom
	floor = params.floor;
	camera_pos = params.camera_pos;
	camera_pos.z = floor;
	draw_all_visited_floors = false;

	if (options.show_lights) {
		IngamePreview::FloorVisibilityCalculator floor_visibility;
		start_z = floor_visibility.CalcLastVisibleFloor(floor);
		const Position light_origin = params.light_origin.value_or(camera_pos);
		superend_z = map ? floor_visibility.CalcFirstVisibleFloor(*map, light_origin.x, light_origin.y, floor) : floor;
	} else {
		const auto floor_range = BuildFloorVisibilityRange(floor, options.show_all_floors, options.floor_visibility_mode);
		start_z = floor_range.start_floor;
		superend_z = floor_range.end_floor;
		draw_all_visited_floors = floor_range.draw_all_visited_floors;
	}

	end_z = floor;

	// Calculate logical dimensions
	logical_width = screensize_x * zoom;
	logical_height = screensize_y * zoom;

	const ViewBounds bounds = getBoundsForFloor(floor);
	start_x = bounds.start_x;
	start_y = bounds.start_y;
	end_x = bounds.end_x;
	end_y = bounds.end_y;
}

ViewBounds RenderView::getBoundsForFloor(int map_z, int extra_margin_tiles) const {
	int start = view_scroll_x / TILE_SIZE;
	int top = view_scroll_y / TILE_SIZE;

	// The underground shift follows the camera floor, while expansion follows map_z.
	if (floor > GROUND_LAYER) {
		start -= 2;
		top -= 2;
	}

	int end = start + screensize_x / tile_size + 2;
	int bottom = top + screensize_y / tile_size + 2;

	const int floor_expansion = std::max(0, start_z - map_z) + std::max(0, extra_margin_tiles);
	start -= floor_expansion;
	top -= floor_expansion;
	end += floor_expansion;
	bottom += floor_expansion;

	return ViewBounds {
		.start_x = start,
		.start_y = top,
		.end_x = end,
		.end_y = bottom,
	};
}

int RenderView::getFloorAdjustment() const {
	if (floor > GROUND_LAYER) { // Underground
		return 0; // No adjustment
	} else {
		return TILE_SIZE * (GROUND_LAYER - floor);
	}
}

bool RenderView::IsTileVisible(int map_x, int map_y, int map_z, int& out_x, int& out_y) const {
	int offset = (map_z <= GROUND_LAYER)
		? (GROUND_LAYER - map_z) * TILE_SIZE
		: TILE_SIZE * (floor - map_z);
	out_x = (map_x * TILE_SIZE) - view_scroll_x - offset;
	out_y = (map_y * TILE_SIZE) - view_scroll_y - offset;
	const int margin = PAINTERS_ALGORITHM_SAFETY_MARGIN_PIXELS;

	// Use cached logical dimensions
	if (out_x < -margin || out_x > logical_width + margin || out_y < -margin || out_y > logical_height + margin) {
		return false;
	}
	return true;
}

bool RenderView::IsPixelVisible(int draw_x, int draw_y, int margin) const {
	// Logic matches IsTileVisible but uses pre-calculated draw coordinates.
	// screensize_x * zoom gives the logical viewport size (since TILE_SIZE is constant 32).
	// See SetupGL: glOrtho(0, width * zoom, ...)

	// Use cached logical dimensions
	if (draw_x + TILE_SIZE + margin < 0 || draw_x - margin > logical_width || draw_y + TILE_SIZE + margin < 0 || draw_y - margin > logical_height) {
		return false;
	}
	return true;
}

bool RenderView::IsRectVisible(int draw_x, int draw_y, int width, int height, int margin) const {
	if (draw_x + width + margin < 0 || draw_x - margin > logical_width || draw_y + height + margin < 0 || draw_y - margin > logical_height) {
		return false;
	}
	return true;
}

bool RenderView::IsRectFullyInside(int draw_x, int draw_y, int width, int height) const {
	return (draw_x >= 0 && draw_x + width <= logical_width && draw_y >= 0 && draw_y + height <= logical_height);
}

void RenderView::getScreenPosition(int map_x, int map_y, int map_z, int& out_x, int& out_y) const {
	int offset = (map_z <= GROUND_LAYER)
		? (GROUND_LAYER - map_z) * TILE_SIZE
		: TILE_SIZE * (floor - map_z);
	out_x = (map_x * TILE_SIZE) - view_scroll_x - offset;
	out_y = (map_y * TILE_SIZE) - view_scroll_y - offset;
}

#include <glm/gtc/matrix_transform.hpp>

void RenderView::SetupGL() {
	glViewport(viewport_x, viewport_y, screensize_x, screensize_y);

	// Calculate Projection
	// glOrtho(0, vPort[2] * zoom, vPort[3] * zoom, 0, -1, 1);
	// Equivalent: 0 -> width*zoom, height*zoom -> 0

	int width = screensize_x;
	int height = screensize_y;

	projectionMatrix = glm::ortho(0.0f, width * zoom, height * zoom, 0.0f, -1.0f, 1.0f);

	// Calculate ModelView
	// glTranslatef(0.375f, 0.375f, 0.0f);
	viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.375f, 0.375f, 0.0f));
}

void RenderView::ReleaseGL() {
	// No legacy matrix stack to cleanup
}

void RenderView::Clear() {
	// Black Background
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// glLoadIdentity(); // Legacy
	// Blending and State management is now handled by individual renderers
}

