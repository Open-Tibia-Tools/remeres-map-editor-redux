#include "rendering/drawers/overlays/marker_label_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "map/map.h"
#include "game/waypoints.h"
#include "game/town.h"
#include "game/spawn.h"
#include <nanovg.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <format>
#include <cmath>
#include <algorithm>

namespace rme::rendering {

namespace {

struct CachedMetrics {
	float width = 0.0f;
	float height = 0.0f;
};

enum class MarkerLabelType {
	Waypoint,
	Town,
	Spawn
};

struct VisibleMarkerLabel {
	float x;
	float y;
	float width;
	float height;
	std::string text;
	MarkerLabelType type;
};

bool isFloorVisible(int z, const RenderView& view, const DrawingOptions& options) {
	if (z == view.floor) {
		return true;
	}
	if (options.show_all_floors && view.floor <= GROUND_LAYER && z >= view.start_z && z <= view.end_z) {
		return true;
	}
	return false;
}

} // namespace

MarkerLabelDrawer::MarkerLabelDrawer() = default;
MarkerLabelDrawer::~MarkerLabelDrawer() = default;

void MarkerLabelDrawer::draw(NVGcontext* vg, const Map& map, const RenderView& view, const DrawingOptions& options) {
	if (!vg || options.ingame) {
		return;
	}

	const bool show_wp = options.show_waypoints;
	const bool show_towns = options.show_towns;
	const bool show_spawns = options.show_spawns;
	if (!show_wp && !show_towns && !show_spawns) {
		return;
	}

	// Beyond LOD threshold (when zoomed out beyond 10% zoom, text is sub-legible)
	if (view.zoom > 10.0f) {
		return;
	}

	const float inv_zoom = 1.0f / view.zoom;
	const float tile_size_screen = 32.0f * inv_zoom;
	const float screen_max_x = static_cast<float>(view.screensize_x) + 64.0f;
	const float screen_max_y = static_cast<float>(view.screensize_y) + 64.0f;

	constexpr float fontSize = 11.0f;
	constexpr float paddingX = 5.0f;
	constexpr float paddingY = 2.0f;
	constexpr float cornerRadius = 3.0f;

	nvgFontSize(vg, fontSize);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

	static thread_local std::unordered_map<std::string, CachedMetrics> s_metrics_cache;
	static thread_local std::vector<VisibleMarkerLabel> visible_labels;
	visible_labels.clear();

	auto getMetrics = [&](const std::string& str) -> CachedMetrics {
		auto it = s_metrics_cache.find(str);
		if (it != s_metrics_cache.end()) {
			return it->second;
		}
		float bounds[4];
		nvgTextBounds(vg, 0.0f, 0.0f, str.c_str(), nullptr, bounds);
		CachedMetrics m {
			.width = bounds[2] - bounds[0],
			.height = bounds[3] - bounds[1]
		};
		s_metrics_cache.emplace(str, m);
		return m;
	};

	auto overlaps = [&](float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) noexcept {
		const float l1 = x1 - w1 * 0.5f - paddingX;
		const float r1 = x1 + w1 * 0.5f + paddingX;
		const float t1 = y1 - h1 - paddingY * 2.0f;
		const float b1 = y1;

		const float l2 = x2 - w2 * 0.5f - paddingX;
		const float r2 = x2 + w2 * 0.5f + paddingX;
		const float t2 = y2 - h2 - paddingY * 2.0f;
		const float b2 = y2;

		return (l1 < r2 && r1 > l2 && t1 < b2 && b1 > t2);
	};

	auto resolveCollision = [&](float x, float initialY, float w, float h) -> float {
		float y = initialY;
		bool shifted = true;
		int iterations = 0;
		while (shifted && iterations < 8) {
			shifted = false;
			++iterations;
			for (const auto& existing : visible_labels) {
				if (overlaps(x, y, w, h, existing.x, existing.y, existing.width, existing.height)) {
					y = existing.y - existing.height - paddingY * 2.0f - 2.0f;
					shifted = true;
					break;
				}
			}
		}
		return y;
	};

	// 1. Collect Waypoints
	if (show_wp) {
		for (const auto& [wp_name, wp_ptr] : map.waypoints) {
			if (!wp_ptr || wp_ptr->name.empty()) {
				continue;
			}
			const Position& pos = wp_ptr->pos;
			if (!isFloorVisible(pos.z, view, options)) {
				continue;
			}

			int unscaled_x = 0, unscaled_y = 0;
			if (!view.IsTileVisible(pos.x, pos.y, pos.z, unscaled_x, unscaled_y)) {
				continue;
			}

			const float screen_x = static_cast<float>(unscaled_x) * inv_zoom;
			const float screen_y = static_cast<float>(unscaled_y) * inv_zoom;
			if (screen_x < -64.0f || screen_x > screen_max_x || screen_y < -64.0f || screen_y > screen_max_y) {
				continue;
			}

			const CachedMetrics m = getMetrics(wp_ptr->name);
			const float labelX = screen_x + tile_size_screen * 0.5f;
			const float labelY = resolveCollision(labelX, screen_y - 3.0f, m.width, m.height);

			visible_labels.push_back(VisibleMarkerLabel {
				.x = labelX,
				.y = labelY,
				.width = m.width,
				.height = m.height,
				.text = wp_ptr->name,
				.type = MarkerLabelType::Waypoint
			});
		}
	}

	// 2. Collect Towns
	if (show_towns) {
		for (const auto& [town_id, town_ptr] : map.towns) {
			if (!town_ptr) {
				continue;
			}
			const Position& pos = town_ptr->getTemplePosition();
			if (!isFloorVisible(pos.z, view, options)) {
				continue;
			}

			int unscaled_x = 0, unscaled_y = 0;
			if (!view.IsTileVisible(pos.x, pos.y, pos.z, unscaled_x, unscaled_y)) {
				continue;
			}

			const float screen_x = static_cast<float>(unscaled_x) * inv_zoom;
			const float screen_y = static_cast<float>(unscaled_y) * inv_zoom;
			if (screen_x < -64.0f || screen_x > screen_max_x || screen_y < -64.0f || screen_y > screen_max_y) {
				continue;
			}

			std::string label_text = town_ptr->getName();
			if (label_text.empty()) {
				label_text = std::format("Town {}", town_id);
			}

			const CachedMetrics m = getMetrics(label_text);
			const float labelX = screen_x + tile_size_screen * 0.5f;
			const float labelY = resolveCollision(labelX, screen_y - 3.0f, m.width, m.height);

			visible_labels.push_back(VisibleMarkerLabel {
				.x = labelX,
				.y = labelY,
				.width = m.width,
				.height = m.height,
				.text = std::move(label_text),
				.type = MarkerLabelType::Town
			});
		}
	}

	// 3. Collect Spawns
	if (show_spawns) {
		for (const Position& spos : map.spawns) {
			if (!isFloorVisible(spos.z, view, options)) {
				continue;
			}
			const Tile* st = map.getTile(spos);
			if (!st || !st->spawn) {
				continue;
			}

			int unscaled_x = 0, unscaled_y = 0;
			if (!view.IsTileVisible(spos.x, spos.y, spos.z, unscaled_x, unscaled_y)) {
				continue;
			}

			const float screen_x = static_cast<float>(unscaled_x) * inv_zoom;
			const float screen_y = static_cast<float>(unscaled_y) * inv_zoom;
			if (screen_x < -64.0f || screen_x > screen_max_x || screen_y < -64.0f || screen_y > screen_max_y) {
				continue;
			}

			std::string label_text = std::format("R: {}", st->spawn->getSize());
			const CachedMetrics m = getMetrics(label_text);
			const float labelX = screen_x + tile_size_screen * 0.5f;
			const float labelY = resolveCollision(labelX, screen_y - 3.0f, m.width, m.height);

			visible_labels.push_back(VisibleMarkerLabel {
				.x = labelX,
				.y = labelY,
				.width = m.width,
				.height = m.height,
				.text = std::move(label_text),
				.type = MarkerLabelType::Spawn
			});
		}
	}

	if (visible_labels.empty()) {
		return;
	}

	// Pass 1: Background pills and crisp 1px borders
	for (const auto& vl : visible_labels) {
		const float rx = vl.x - vl.width * 0.5f - paddingX;
		const float ry = vl.y - vl.height - paddingY * 2.0f;
		const float rw = vl.width + paddingX * 2.0f;
		const float rh = vl.height + paddingY * 2.0f;

		nvgBeginPath(vg);
		nvgRoundedRect(vg, rx, ry, rw, rh, cornerRadius);

		if (vl.type == MarkerLabelType::Waypoint) {
			nvgFillColor(vg, nvgRGBA(10, 24, 32, 220));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(0, 220, 245, 230));
			nvgStrokeWidth(vg, 1.0f);
			nvgStroke(vg);
		} else if (vl.type == MarkerLabelType::Town) {
			nvgFillColor(vg, nvgRGBA(32, 24, 10, 220));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(255, 190, 20, 230));
			nvgStrokeWidth(vg, 1.0f);
			nvgStroke(vg);
		} else { // MarkerLabelType::Spawn
			nvgFillColor(vg, nvgRGBA(32, 10, 32, 220));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(255, 60, 255, 230));
			nvgStrokeWidth(vg, 1.0f);
			nvgStroke(vg);
		}
	}

	// Pass 2: Sharp text typography
	for (const auto& vl : visible_labels) {
		if (vl.type == MarkerLabelType::Waypoint) {
			nvgFillColor(vg, nvgRGBA(200, 245, 255, 255));
		} else if (vl.type == MarkerLabelType::Town) {
			nvgFillColor(vg, nvgRGBA(255, 240, 190, 255));
		} else { // MarkerLabelType::Spawn
			nvgFillColor(vg, nvgRGBA(255, 220, 255, 255));
		}
		nvgText(vg, vl.x, vl.y - paddingY, vl.text.c_str(), nullptr);
	}
}

} // namespace rme::rendering
