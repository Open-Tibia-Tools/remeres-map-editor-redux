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
#include <format>

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

	// When zoomed out far, skip text labels when zoomed beyond 10% zoom (matching editor LOD policy)
	if (view.zoom > 10.0f) {
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
	struct StringHash {
		using is_transparent = void;
		size_t operator()(std::string_view sv) const noexcept {
			return std::hash<std::string_view>{}(sv);
		}
		size_t operator()(const std::string& s) const noexcept {
			return std::hash<std::string_view>{}(s);
		}
		size_t operator()(const char* s) const noexcept {
			return std::hash<std::string_view>{}(s);
		}
	};
	static thread_local std::unordered_map<std::string, CachedMetrics, StringHash, std::equal_to<>> s_metrics_cache;

	struct VisibleLabel {
		float x;
		float y;
		float width;
		float height;
		std::string_view text;
		std::string dynamic_text;
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

		const char* t_begin = label.name.data();
		const char* t_end = label.name.data() + label.name.size();
		std::string_view lookup_key = label.name;

		// Fast cached text bounds lookup for creature name (zero allocations on hit)
		auto it = s_metrics_cache.find(lookup_key);
		float textWidth = 0.0f;
		float textHeight = 0.0f;

		if (it != s_metrics_cache.end()) {
			textWidth = it->second.width;
			textHeight = it->second.height;
		} else {
			float textBounds[4];
			nvgTextBounds(vg, 0, 0, t_begin, t_end, textBounds);
			textWidth = textBounds[2] - textBounds[0];
			textHeight = textBounds[3] - textBounds[1];
			s_metrics_cache.emplace(std::string(lookup_key), CachedMetrics{ textWidth, textHeight });
		}

		visible_labels.push_back(VisibleLabel {
			.x = labelX,
			.y = labelY,
			.width = textWidth,
			.height = textHeight,
			.text = lookup_key,
			.dynamic_text = {}
		});

		// Spawn time badge placed in bottom-right corner of creature tile
		if (label.creature && !label.creature->isNpc() && label.creature->getSpawnTime() > 0) {
			std::string formatted = std::format("{}s", label.creature->getSpawnTime());
			auto it_t = s_metrics_cache.find(formatted);
			float tw = 0.0f;
			float th = 0.0f;
			if (it_t != s_metrics_cache.end()) {
				tw = it_t->second.width;
				th = it_t->second.height;
			} else {
				float tb[4];
				nvgTextBounds(vg, 0, 0, formatted.c_str(), nullptr, tb);
				tw = tb[2] - tb[0];
				th = tb[3] - tb[1];
				s_metrics_cache.emplace(formatted, CachedMetrics{ tw, th });
			}

			const float timerX = screen_x + tile_size_screen - tw * 0.5f - paddingX - 1.0f;
			const float timerY = screen_y + tile_size_screen - 1.0f;

			visible_labels.push_back(VisibleLabel {
				.x = timerX,
				.y = timerY,
				.width = tw,
				.height = th,
				.text = {},
				.dynamic_text = std::move(formatted)
			});
		}
	}

	if (visible_labels.empty()) {
		return;
	}

	// Pass 1: Draw all backgrounds with convex rounded rectangles
	nvgFillColor(vg, nvgRGBA(0, 0, 0, 160));
	for (const auto& vl : visible_labels) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, vl.x - vl.width * 0.5f - paddingX, vl.y - vl.height - paddingY * 2.0f, vl.width + paddingX * 2.0f, vl.height + paddingY * 2.0f, 3.0f);
		nvgFill(vg);
	}

	// Pass 2: Draw all text labels with single color state
	nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
	for (const auto& vl : visible_labels) {
		std::string_view sv = vl.dynamic_text.empty() ? vl.text : std::string_view(vl.dynamic_text);
		nvgText(vg, vl.x, vl.y - paddingY, sv.data(), sv.data() + sv.size());
	}
}
