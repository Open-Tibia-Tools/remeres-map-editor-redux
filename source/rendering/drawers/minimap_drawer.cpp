#include "rendering/drawers/minimap_drawer.h"

#include "app/definitions.h"
#include "editor/editor.h"
#include "map/map.h"
#include "rendering/core/map_view_math.h"
#include "rendering/core/floor_visibility_mode.h"
#include "rendering/core/primitive_renderer.h"
#include "rendering/core/render_view.h"
#include "ui/managers/minimap_manager.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace {

[[nodiscard]] double clampMapCoordinate(double value, int limit) {
	if (limit <= 0) {
		return 0.0;
	}
	return std::clamp(value, 0.0, static_cast<double>(limit - 1));
}

struct MinimapFloorRenderRange {
	int start_floor = GROUND_LAYER;
	int current_floor = GROUND_LAYER;
	int end_floor = GROUND_LAYER;
	bool draw_all_visited_floors = false;
};

[[nodiscard]] MinimapFloorRenderRange getFloorRenderRange(const MinimapViewportState& viewport_state, FloorVisibilityMode mode) {
	const auto range = BuildFloorVisibilityRange(viewport_state.floor, viewport_state.show_all_floors, mode);
	return {
		.start_floor = range.start_floor,
		.current_floor = MinimapViewport::ClampFloor(viewport_state.floor),
		.end_floor = range.end_floor,
		.draw_all_visited_floors = range.draw_all_visited_floors,
	};
}

} // namespace

MinimapDrawer::MinimapDrawer() :
	renderer(std::make_unique<MinimapRenderer>()),
	primitive_renderer(std::make_unique<PrimitiveRenderer>()) {
}

MinimapDrawer::~MinimapDrawer() {
}

void MinimapDrawer::ReleaseGL() {
	if (renderer) {
		renderer->releaseGL();
	}
	primitive_renderer.reset();
	renderer.reset();
	initialized_ = false;
}

MinimapDrawer::VisibleWorldRect MinimapDrawer::BuildVisibleWorldRect(const glm::ivec2& size, const MinimapViewportState& viewport_state) {
	const double zoom_factor = MinimapViewport::GetZoomFactor(viewport_state.zoom_step);
	const double visible_map_width = std::max(1.0, size.x * zoom_factor);
	const double visible_map_height = std::max(1.0, size.y * zoom_factor);

	return {
		.start_x = viewport_state.center_x - visible_map_width / 2.0,
		.start_y = viewport_state.center_y - visible_map_height / 2.0,
		.width = visible_map_width,
		.height = visible_map_height,
	};
}

void MinimapDrawer::DrawMainCameraBox(const glm::mat4& projection, const glm::ivec2& size, const ViewportParameters& camera_viewport, const VisibleWorldRect& visible_rect) {
	const MainMapVisibleRect camera_rect = MainMapViewMath::GetVisibleRect({
		.view_scroll_x = camera_viewport.view_scroll_x,
		.view_scroll_y = camera_viewport.view_scroll_y,
		.pixel_width = camera_viewport.screensize_x,
		.pixel_height = camera_viewport.screensize_y,
		.zoom = camera_viewport.zoom,
		.floor = camera_viewport.floor,
		.scale_factor = camera_viewport.content_scale_factor,
	});

	const float scale_x = static_cast<float>(size.x / visible_rect.width);
	const float scale_y = static_cast<float>(size.y / visible_rect.height);
	const float x = static_cast<float>((camera_rect.start_x - visible_rect.start_x) * scale_x);
	const float y = static_cast<float>((camera_rect.start_y - visible_rect.start_y) * scale_y);
	const float w = static_cast<float>(camera_rect.width * scale_x);
	const float h = static_cast<float>(camera_rect.height * scale_y);

	primitive_renderer->setProjectionMatrix(projection);
	const glm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);
	primitive_renderer->drawLine(glm::vec2(x, y), glm::vec2(x + w, y), color);
	primitive_renderer->drawLine(glm::vec2(x, y + h), glm::vec2(x + w, y + h), color);
	primitive_renderer->drawLine(glm::vec2(x, y), glm::vec2(x, y + h), color);
	primitive_renderer->drawLine(glm::vec2(x + w, y), glm::vec2(x + w, y + h), color);
	primitive_renderer->flush();
}

void MinimapDrawer::DrawFloorShade(const glm::mat4& projection, const glm::ivec2& size) {
	primitive_renderer->setProjectionMatrix(projection);
	primitive_renderer->drawRect(
		glm::vec4(0.0f, 0.0f, static_cast<float>(size.x), static_cast<float>(size.y)),
		glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));
	primitive_renderer->flush();
}

void MinimapDrawer::DrawMapBoundsBorder(const glm::mat4& projection, const glm::ivec2& size, const Editor& editor, const VisibleWorldRect& visible_rect) {
	const float scale_x = static_cast<float>(size.x / visible_rect.width);
	const float scale_y = static_cast<float>(size.y / visible_rect.height);
	const float x = static_cast<float>((0.0 - visible_rect.start_x) * scale_x);
	const float y = static_cast<float>((0.0 - visible_rect.start_y) * scale_y);
	const float w = static_cast<float>(editor.map.getWidth() * scale_x);
	const float h = static_cast<float>(editor.map.getHeight() * scale_y);

	primitive_renderer->setProjectionMatrix(projection);
	const glm::vec4 color(0.5f, 0.5f, 0.5f, 1.0f);
	primitive_renderer->drawLine(glm::vec2(x, y), glm::vec2(x + w, y), color);
	primitive_renderer->drawLine(glm::vec2(x, y + h), glm::vec2(x + w, y + h), color);
	primitive_renderer->drawLine(glm::vec2(x, y), glm::vec2(x, y + h), color);
	primitive_renderer->drawLine(glm::vec2(x + w, y), glm::vec2(x + w, y + h), color);
	primitive_renderer->flush();
}

void MinimapDrawer::Draw(const glm::ivec2& size, Editor& editor, const ViewportParameters* camera_viewport, const MinimapViewportState& viewport_state, MinimapDrawOptions options) {
	const int window_width = size.x;
	const int window_height = size.y;
	if (window_width <= 0 || window_height <= 0) {
		last_viewport_.valid = false;
		return;
	}

	if (!initialized_) {
		if (renderer->initialize()) {
			primitive_renderer->initialize();
			initialized_ = true;
		}
	}

	if (!initialized_) {
		last_viewport_.valid = false;
		return;
	}

	glViewport(0, 0, window_width, window_height);
	const glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(window_width), static_cast<float>(window_height), 0.0f, -1.0f, 1.0f);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	const VisibleWorldRect visible_rect = BuildVisibleWorldRect(size, viewport_state);
	const double start_x = visible_rect.start_x;
	const double start_y = visible_rect.start_y;

	last_viewport_ = {
		.start_x = start_x,
		.start_y = start_y,
		.map_width = visible_rect.width,
		.map_height = visible_rect.height,
		.screen_width = window_width,
		.screen_height = window_height,
		.map_limit_x = editor.map.getWidth(),
		.map_limit_y = editor.map.getHeight(),
		.valid = true
	};

	renderer->bindMap(editor.map.getGeneration(), editor.map.getWidth(), editor.map.getHeight());

	const PendingMinimapInvalidation pending = g_minimap.TakePendingInvalidation(editor.map);
	if (pending.invalidate_all) {
		renderer->invalidateAll();
	}
	for (int floor = 0; floor < MAP_LAYERS; ++floor) {
		if (pending.floor_rects[floor].has_value()) {
			renderer->markDirty(floor, *pending.floor_rects[floor]);
		}
	}

	const auto floor_range = getFloorRenderRange(viewport_state, options.floor_visibility_mode);
	const MinimapDirtyRect visible_rect_pixels = {
		.x = static_cast<int>(std::floor(visible_rect.start_x)),
		.y = static_cast<int>(std::floor(visible_rect.start_y)),
		.width = std::max(1, static_cast<int>(std::ceil(visible_rect.start_x + visible_rect.width)) - static_cast<int>(std::floor(visible_rect.start_x))),
		.height = std::max(1, static_cast<int>(std::ceil(visible_rect.start_y + visible_rect.height)) - static_cast<int>(std::floor(visible_rect.start_y))),
	};

	const int last_render_floor = floor_range.draw_all_visited_floors ? floor_range.end_floor : floor_range.current_floor;
	for (int floor = floor_range.start_floor; floor >= last_render_floor; --floor) {
		if (floor == floor_range.current_floor && floor_range.start_floor > floor_range.current_floor) {
			DrawFloorShade(projection, size);
		}

		renderer->flushVisible(editor.map, floor, visible_rect_pixels);
		renderer->renderVisible(projection, 0, 0, window_width, window_height, floor, visible_rect_pixels);
	}
	if (options.drawBoundsBorder) {
		DrawMapBoundsBorder(projection, size, editor, visible_rect);
	}
	if (options.drawCameraBox && camera_viewport != nullptr) {
		DrawMainCameraBox(projection, size, *camera_viewport, visible_rect);
	}
}

void MinimapDrawer::ScreenToMap(int screen_x, int screen_y, int& map_x, int& map_y) {
	if (!last_viewport_.valid) {
		map_x = 0;
		map_y = 0;
		return;
	}

	const double normalized_x = std::clamp(screen_x / static_cast<double>(std::max(1, last_viewport_.screen_width)), 0.0, 1.0);
	const double normalized_y = std::clamp(screen_y / static_cast<double>(std::max(1, last_viewport_.screen_height)), 0.0, 1.0);

	const double raw_map_x = last_viewport_.start_x + normalized_x * last_viewport_.map_width;
	const double raw_map_y = last_viewport_.start_y + normalized_y * last_viewport_.map_height;

	map_x = static_cast<int>(std::floor(clampMapCoordinate(raw_map_x, last_viewport_.map_limit_x)));
	map_y = static_cast<int>(std::floor(clampMapCoordinate(raw_map_y, last_viewport_.map_limit_y)));
}
