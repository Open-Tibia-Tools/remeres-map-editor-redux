#include "rendering/drawers/overlays/selection_drawer.h"
#include "rendering/core/primitive_renderer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include <glm/glm.hpp>
#include <algorithm>

namespace {
	constexpr float SELECTION_OUTLINE_ALPHA = 0.95f;
}

void SelectionDrawer::draw(PrimitiveRenderer& primitive_renderer, const RenderView& view, const DrawingOptions& options) {
	if (options.ingame) {
		return;
	}

	if (!options.boundbox_selection || !options.transient_selection_bounds.has_value()) {
		return;
	}

	const MapBounds& bounds = *options.transient_selection_bounds;
	int start_x = 0;
	int start_y = 0;
	int end_x = 0;
	int end_y = 0;
	view.getScreenPosition(bounds.x1, bounds.y1, view.floor, start_x, start_y);
	view.getScreenPosition(bounds.x2 + 1, bounds.y2 + 1, view.floor, end_x, end_y);

	const float x = static_cast<float>(start_x);
	const float y = static_cast<float>(start_y);
	const float w = static_cast<float>(end_x - start_x);
	const float h = static_cast<float>(end_y - start_y);
	const glm::vec4 rect(x, y, w, h);

	primitive_renderer.drawRect(rect, glm::vec4(1.0f, 1.0f, 1.0f, 0.2f));

	const float outline_thickness = view.zoom;
	const glm::vec4 outline_color(1.0f, 1.0f, 1.0f, SELECTION_OUTLINE_ALPHA);
	const auto drawDashedLine = [&](float start_line_x, float start_line_y, float length, bool horizontal) {
		const float dash_length = 3.0f * outline_thickness;
		const float gap_length = 2.0f * outline_thickness;
		for (float dash_offset = 0.0f; dash_offset < length; dash_offset += dash_length + gap_length) {
			const float dash_extent = std::min(dash_length, length - dash_offset);
			const glm::vec4 dash_rect = horizontal
				? glm::vec4(start_line_x + dash_offset, start_line_y, dash_extent, outline_thickness)
				: glm::vec4(start_line_x, start_line_y + dash_offset, outline_thickness, dash_extent);
			primitive_renderer.drawRect(dash_rect, outline_color);
		}
	};

	drawDashedLine(x, y, w, true);
	drawDashedLine(x, y + h - outline_thickness, w, true);
	drawDashedLine(x, y, h, false);
	drawDashedLine(x + w - outline_thickness, y, h, false);
}
