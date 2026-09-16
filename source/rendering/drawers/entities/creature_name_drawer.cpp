//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/drawers/entities/creature_name_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/text_renderer.h"
#include <nanovg.h>
#include "rendering/core/coordinate_mapper.h"
#include "game/creature.h"

CreatureNameDrawer::CreatureNameDrawer() {
}

CreatureNameDrawer::~CreatureNameDrawer() {
	clear();
}

void CreatureNameDrawer::clear() {
	labels.clear();
}

void CreatureNameDrawer::addLabel(const Position& pos, std::string_view name, const Creature* c) {
	if (name.empty()) {
		return;
	}
	labels.push_back({ pos, name, c });
}

void CreatureNameDrawer::draw(NVGcontext* vg, const RenderView& view) {
	if (!vg) {
		return;
	}

	if (labels.empty()) {
		return;
	}

	const float zoom = view.zoom;
	const float inv_zoom = 1.0f / zoom;
	const float tile_size_screen = 32.0f * inv_zoom;
	constexpr float fontSize = 11.0f;

	nvgFontSize(vg, fontSize);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

	struct VisibleLabel {
		float x;
		float y;
		float width;
		float height;
		const char* text;
		const char* text_end;
	};
	static thread_local std::vector<VisibleLabel> visible_labels;
	visible_labels.clear();

	for (const auto& label : labels) {
		if (label.pos.z != view.camera_pos.z) {
			continue;
		}

		int unscaled_x, unscaled_y;
		view.getScreenPosition(label.pos.x, label.pos.y, label.pos.z, unscaled_x, unscaled_y);

		const float screen_x = static_cast<float>(unscaled_x) * inv_zoom;
		const float screen_y = static_cast<float>(unscaled_y) * inv_zoom;

		// Center on tile, position slightly above the creature head
		const float labelX = screen_x + tile_size_screen * 0.5f;
		const float labelY = screen_y - 2.0f; // slight gap above tile top

		float textBounds[4];
		const char* text_start = label.name.data();
		const char* text_end = text_start + label.name.size();
		nvgTextBounds(vg, 0, 0, text_start, text_end, textBounds);
		const float textWidth = textBounds[2] - textBounds[0];
		const float textHeight = textBounds[3] - textBounds[1];

		visible_labels.push_back(VisibleLabel {
			.x = labelX,
			.y = labelY,
			.width = textWidth,
			.height = textHeight,
			.text = text_start,
			.text_end = text_end
		});
	}

	if (visible_labels.empty()) {
		return;
	}

	constexpr float paddingX = 4.0f;
	constexpr float paddingY = 2.0f;

	// Pass 1: Draw all backgrounds in a single batched path
	nvgBeginPath(vg);
	for (const auto& vl : visible_labels) {
		nvgRoundedRect(vg, vl.x - vl.width * 0.5f - paddingX, vl.y - vl.height - paddingY * 2.0f, vl.width + paddingX * 2.0f, vl.height + paddingY * 2.0f, 3.0f);
	}
	nvgFillColor(vg, nvgRGBA(0, 0, 0, 160)); // Transparent black
	nvgFill(vg);

	// Pass 2: Draw all text labels with single color state
	nvgFillColor(vg, nvgRGBA(255, 255, 255, 255)); // White text
	for (const auto& vl : visible_labels) {
		nvgText(vg, vl.x, vl.y - paddingY, vl.text, vl.text_end);
	}
}
