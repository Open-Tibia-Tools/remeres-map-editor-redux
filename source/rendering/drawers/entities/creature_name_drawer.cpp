//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/drawers/entities/creature_name_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/text_renderer.h"
#include <nanovg.h>
#include "rendering/core/coordinate_mapper.h"
#include "game/creature.h"

#include <unordered_map>

CreatureNameDrawer::CreatureNameDrawer() {
	labels.reserve(256);
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
	if (!vg || labels.empty()) {
		return;
	}

	const float zoom = view.zoom;
	const float inv_zoom = 1.0f / zoom;
	const float tile_size_screen = 32.0f * inv_zoom;

	// When zoomed out far, text labels overlap into solid illegible noise and tank performance.
	// Matching ImguiMapEditor policy: skip text labels when tiles are smaller than 10 pixels.
	if (tile_size_screen < 10.0f) {
		return;
	}

	constexpr float fontSize = 11.0f;
	constexpr float paddingX = 4.0f;
	constexpr float paddingY = 2.0f;

	nvgFontSize(vg, fontSize);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

	struct CachedMetrics {
		float width = 0.0f;
		float height = 0.0f;
	};
	static thread_local std::unordered_map<std::string, CachedMetrics> s_metrics_cache;

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

	const float screen_max_x = static_cast<float>(view.screensize_x) + 64.0f;
	const float screen_max_y = static_cast<float>(view.screensize_y) + 64.0f;

	for (const auto& label : labels) {
		if (label.pos.z != view.camera_pos.z) {
			continue;
		}

		int unscaled_x, unscaled_y;
		if (!view.IsTileVisible(label.pos.x, label.pos.y, label.pos.z, unscaled_x, unscaled_y)) {
			continue;
		}

		const float screen_x = static_cast<float>(unscaled_x) * inv_zoom;
		const float screen_y = static_cast<float>(unscaled_y) * inv_zoom;

		// Strict frustum culling with safety margin
		if (screen_x < -64.0f || screen_x > screen_max_x || screen_y < -64.0f || screen_y > screen_max_y) {
			continue;
		}

		// Center on tile, position slightly above the creature head
		const float labelX = screen_x + tile_size_screen * 0.5f;
		const float labelY = screen_y - 2.0f;

		// Fast cached text bounds lookup (avoids CPU font glyph kerning loops per-instance)
		const std::string name_key(label.name);
		auto it = s_metrics_cache.find(name_key);
		float textWidth = 0.0f;
		float textHeight = 0.0f;

		if (it != s_metrics_cache.end()) {
			textWidth = it->second.width;
			textHeight = it->second.height;
		} else {
			float textBounds[4];
			const char* text_start = label.name.data();
			const char* text_end = text_start + label.name.size();
			nvgTextBounds(vg, 0, 0, text_start, text_end, textBounds);
			textWidth = textBounds[2] - textBounds[0];
			textHeight = textBounds[3] - textBounds[1];
			s_metrics_cache.emplace(name_key, CachedMetrics{ textWidth, textHeight });
		}

		visible_labels.push_back(VisibleLabel {
			.x = labelX,
			.y = labelY,
			.width = textWidth,
			.height = textHeight,
			.text = label.name.data(),
			.text_end = label.name.data() + label.name.size()
		});
	}

	if (visible_labels.empty()) {
		return;
	}

	// Pass 1: Draw all backgrounds with convex rounded rectangles (single path per contour bypasses stencil multipass)
	nvgFillColor(vg, nvgRGBA(0, 0, 0, 160));
	for (const auto& vl : visible_labels) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, vl.x - vl.width * 0.5f - paddingX, vl.y - vl.height - paddingY * 2.0f, vl.width + paddingX * 2.0f, vl.height + paddingY * 2.0f, 3.0f);
		nvgFill(vg);
	}

	// Pass 2: Draw all text labels with single color state
	nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
	for (const auto& vl : visible_labels) {
		nvgText(vg, vl.x, vl.y - paddingY, vl.text, vl.text_end);
	}
}
