#include "rendering/drawers/overlays/marker_label_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "map/map.h"
#include "game/waypoints.h"
#include "game/town.h"
#include "game/house.h"
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
	HouseActive,
	HouseInactive
};

struct VisibleMarkerLabel {
	float x;
	float y;
	float width;
	float height;
	std::string text;
	MarkerLabelType type;
};

} // namespace

MarkerLabelDrawer::MarkerLabelDrawer() = default;
MarkerLabelDrawer::~MarkerLabelDrawer() = default;

void MarkerLabelDrawer::draw(NVGcontext* vg, const Map& map, const RenderView& view, const DrawingOptions& options) {
	if (!vg || options.ingame) {
		return;
	}

	const bool show_wp = options.show_waypoints;
	const bool show_towns = options.show_towns;
	const bool show_houses = options.show_houses;
	if (!show_wp && !show_towns && !show_houses) {
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

	constexpr float fontSize = 16.5f;
	constexpr float paddingX = 7.5f;
	constexpr float paddingY = 3.0f;
	constexpr float cornerRadius = 4.0f;

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
			if (pos.z != view.floor) {
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
			if (pos.z != view.floor) {
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

	// 3. Collect Houses
	if (show_houses) {
		for (const auto& [house_id, house_ptr] : map.houses) {
			if (!house_ptr) {
				continue;
			}
			const auto& all_tiles = house_ptr->getTiles();
			if (all_tiles.empty()) {
				continue;
			}

			const int check_z = view.floor;

			std::vector<Position> floor_tiles;
			floor_tiles.reserve(all_tiles.size());
			for (const auto& p : all_tiles) {
				if (p.z == check_z) {
					floor_tiles.push_back(p);
				}
			}
			if (floor_tiles.empty()) {
				continue;
			}

			// Centroid of the room tiles on the active floor
			double sum_x = 0.0;
			double sum_y = 0.0;
			for (const auto& p : floor_tiles) {
				sum_x += p.x;
				sum_y += p.y;
			}
			const double avg_x = sum_x / static_cast<double>(floor_tiles.size());
			const double avg_y = sum_y / static_cast<double>(floor_tiles.size());

			// Choose the real room tile closest to centroid to guarantee it sits inside the room
			Position best_pos = floor_tiles[0];
			double min_dist_sq = 1e18;
			for (const auto& p : floor_tiles) {
				const double dx = static_cast<double>(p.x) - avg_x;
				const double dy = static_cast<double>(p.y) - avg_y;
				const double dsq = dx * dx + dy * dy;
				if (dsq < min_dist_sq) {
					min_dist_sq = dsq;
					best_pos = p;
				}
			}

			int unscaled_x = 0, unscaled_y = 0;
			if (!view.IsTileVisible(best_pos.x, best_pos.y, check_z, unscaled_x, unscaled_y)) {
				continue;
			}

			const float screen_x = static_cast<float>(unscaled_x) * inv_zoom;
			const float screen_y = static_cast<float>(unscaled_y) * inv_zoom;
			if (screen_x < -64.0f || screen_x > screen_max_x || screen_y < -64.0f || screen_y > screen_max_y) {
				continue;
			}

			std::string label_text = house_ptr->name;
			if (label_text.empty()) {
				label_text = "HOUSE";
			}

			const CachedMetrics m = getMetrics(label_text);
			const float labelX = screen_x + tile_size_screen * 0.5f;
			const float labelY = resolveCollision(labelX, screen_y + tile_size_screen * 0.5f + (m.height * 0.5f) - paddingY, m.width, m.height);

			const bool is_active = (options.current_house_id > 0 && house_ptr->getID() == options.current_house_id);

			visible_labels.push_back(VisibleMarkerLabel {
				.x = labelX,
				.y = labelY,
				.width = m.width,
				.height = m.height,
				.text = std::move(label_text),
				.type = is_active ? MarkerLabelType::HouseActive : MarkerLabelType::HouseInactive
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
			nvgFillColor(vg, nvgRGBA(15, 15, 22, 242));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(0, 229, 255, 245)); // Vivid Cyan #00E5FF
			nvgStrokeWidth(vg, 1.2f);
			nvgStroke(vg);
		} else if (vl.type == MarkerLabelType::Town) {
			nvgFillColor(vg, nvgRGBA(15, 15, 22, 242));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(255, 217, 0, 245)); // Gold #FFD900
			nvgStrokeWidth(vg, 1.2f);
			nvgStroke(vg);
		} else if (vl.type == MarkerLabelType::HouseActive) {
			nvgFillColor(vg, nvgRGBA(15, 15, 22, 242));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(180, 235, 31, 245)); // Lime Green #B4EB1F (same as ENTRY)
			nvgStrokeWidth(vg, 1.2f);
			nvgStroke(vg);
		} else { // MarkerLabelType::HouseInactive
			nvgFillColor(vg, nvgRGBA(15, 15, 22, 242));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(143, 127, 196, 245)); // Muted Violet #8F7FC4
			nvgStrokeWidth(vg, 1.2f);
			nvgStroke(vg);
		}
	}

	// Pass 2: Sharp text typography
	for (const auto& vl : visible_labels) {
		if (vl.type == MarkerLabelType::Waypoint) {
			nvgFillColor(vg, nvgRGBA(0, 229, 255, 255));
		} else if (vl.type == MarkerLabelType::Town) {
			nvgFillColor(vg, nvgRGBA(255, 217, 0, 255));
		} else if (vl.type == MarkerLabelType::HouseActive) {
			nvgFillColor(vg, nvgRGBA(180, 235, 31, 255));
		} else { // MarkerLabelType::HouseInactive
			nvgFillColor(vg, nvgRGBA(143, 127, 196, 255));
		}
		nvgText(vg, vl.x, vl.y - paddingY, vl.text.c_str(), nullptr);
	}
}

} // namespace rme::rendering
