#ifndef RME_RENDERING_RENDER_VIEW_H_
#define RME_RENDERING_RENDER_VIEW_H_

class BaseMap;
struct DrawingOptions;

#include <glm/glm.hpp>
#include "map/position.h"
#include "app/definitions.h"
#include <optional>

struct ViewportParameters {
	int mouse_map_x = 0;
	int mouse_map_y = 0;
	int view_scroll_x = 0;
	int view_scroll_y = 0;
	int screensize_x = 0;
	int screensize_y = 0;
	float zoom = 1.0f;
	int floor = GROUND_LAYER;
	float content_scale_factor = 1.0f;
	Position camera_pos;
	std::optional<Position> light_origin;
};

struct ViewBounds {
	int start_x = 0;
	int start_y = 0;
	int end_x = 0;
	int end_y = 0;
};

struct RenderView {
	float zoom;
	int tile_size;
	int floor;

	int start_x, start_y, start_z;
	int end_x, end_y, end_z, superend_z;
	int view_scroll_x, view_scroll_y;
	int screensize_x, screensize_y;
	int viewport_x, viewport_y;

	Position camera_pos;

	int mouse_map_x, mouse_map_y;
	bool draw_all_visited_floors = false;

	glm::mat4 projectionMatrix;
	glm::mat4 viewMatrix;

	void Setup(const ViewportParameters& params, const DrawingOptions& options, const BaseMap* map = nullptr);
	// Negative extra_margin_tiles values are clamped to zero.
	[[nodiscard]] ViewBounds getBoundsForFloor(int map_z, int extra_margin_tiles = 0) const;
	void SetupGL();
	void ReleaseGL();
	void Clear();

	int getFloorAdjustment() const;
	bool IsTileVisible(int map_x, int map_y, int map_z, int& out_x, int& out_y) const;
	bool IsPixelVisible(int draw_x, int draw_y, int margin = PAINTERS_ALGORITHM_SAFETY_MARGIN_PIXELS) const;
	// Checks if a rectangle (e.g. a node) is visible
	bool IsRectVisible(int draw_x, int draw_y, int width, int height, int margin = PAINTERS_ALGORITHM_SAFETY_MARGIN_PIXELS) const;
	// Checks if a rectangle is fully inside the viewport (no clipping needed)
	bool IsRectFullyInside(int draw_x, int draw_y, int width, int height) const;
	void getScreenPosition(int map_x, int map_y, int map_z, int& out_x, int& out_y) const;

	// Cached logical viewport dimensions for optimization
	float logical_width = 0.0f;
	float logical_height = 0.0f;
};

#endif
