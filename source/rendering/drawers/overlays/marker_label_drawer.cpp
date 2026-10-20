#include "rendering/drawers/overlays/marker_label_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/indicators/zone_colors.h"
#include "brushes/managers/brush_manager.h"
#include "brushes/zone/zone_brush.h"
#include "map/map.h"
#include "game/waypoints.h"
#include "game/town.h"
#include "game/house.h"
#include <nanovg.h>
#include "util/phase_profiler.h"
#include <string>
#include <deque>
#include <vector>
#include <unordered_map>
#include <format>
#include <cmath>
#include <algorithm>
#include <limits>

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
	HouseInactive,
	Zone
};

struct VisibleMarkerLabel {
	float x;
	float y;
	float width;
	float height;
	std::string_view text;
	MarkerLabelType type;
	NVGcolor custom_color = nvgRGBA(255, 255, 255, 255);
	bool is_active = false;
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
	const bool show_zones = options.show_zones && (map.getVersion().otbm == MAP_OTBM_5);
	if (!show_wp && !show_towns && !show_houses && !show_zones) {
		return;
	}
	rme::prof::Scope labels_scope(rme::prof::Phase::MarkerLabels);

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
	static thread_local std::deque<std::string> s_frame_fallback_names;
	visible_labels.clear();
	s_frame_fallback_names.clear();

	auto getMetrics = [&](std::string_view str) -> CachedMetrics {
		auto it = s_metrics_cache.find(std::string(str));
		if (it != s_metrics_cache.end()) {
			return it->second;
		}
		float bounds[4];
		nvgTextBounds(vg, 0.0f, 0.0f, str.data(), str.data() + str.size(), bounds);
		CachedMetrics m {
			.width = bounds[2] - bounds[0],
			.height = bounds[3] - bounds[1]
		};
		if (s_metrics_cache.size() >= 1024) {
			s_metrics_cache.clear();
		}
		s_metrics_cache.emplace(std::string(str), m);
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
		// First try nudging upwards
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

		// If shifted off the top of the viewport, fallback to nudging downwards from initialY
		if (y - h - paddingY * 2.0f < 0.0f) {
			y = initialY;
			shifted = true;
			iterations = 0;
			while (shifted && iterations < 8) {
				shifted = false;
				++iterations;
				for (const auto& existing : visible_labels) {
					if (overlaps(x, y, w, h, existing.x, existing.y, existing.width, existing.height)) {
						y = existing.y + h + paddingY * 2.0f + 2.0f;
						shifted = true;
						break;
					}
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

			std::string_view label_text = town_ptr->getName();
			if (label_text.empty()) {
				s_frame_fallback_names.push_back(std::format("Town {}", town_id));
				label_text = s_frame_fallback_names.back();
			}

			const CachedMetrics m = getMetrics(label_text);
			const float labelX = screen_x + tile_size_screen * 0.5f;
			const float labelY = resolveCollision(labelX, screen_y - 3.0f, m.width, m.height);

			visible_labels.push_back(VisibleMarkerLabel {
				.x = labelX,
				.y = labelY,
				.width = m.width,
				.height = m.height,
				.text = label_text,
				.type = MarkerLabelType::Town
			});
		}
	}

	// 3. Collect Houses
	if (show_houses) {
		const ViewBounds floor_bounds = view.getBoundsForFloor(view.floor);
		const int check_z = view.floor;

		for (const auto& [house_id, house_ptr] : map.houses) {
			if (!house_ptr) {
				continue;
			}

			const House::FloorSummary& summary = house_ptr->getFloorSummary(check_z);
			if (summary.count == 0) {
				continue;
			}

			// AABB culling against view bounds on active floor
			if (summary.max_x < floor_bounds.start_x || summary.min_x > floor_bounds.end_x ||
			    summary.max_y < floor_bounds.start_y || summary.min_y > floor_bounds.end_y) {
				continue;
			}

			// Room tile closest to the room centroid, so the label sits inside the room.
			const Position& best_pos = summary.best_tile;

			int unscaled_x = 0, unscaled_y = 0;
			if (!view.IsTileVisible(best_pos.x, best_pos.y, check_z, unscaled_x, unscaled_y)) {
				continue;
			}

			const float screen_x = static_cast<float>(unscaled_x) * inv_zoom;
			const float screen_y = static_cast<float>(unscaled_y) * inv_zoom;
			if (screen_x < -64.0f || screen_x > screen_max_x || screen_y < -64.0f || screen_y > screen_max_y) {
				continue;
			}

			std::string_view label_text = house_ptr->name;
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
				.text = label_text,
				.type = is_active ? MarkerLabelType::HouseActive : MarkerLabelType::HouseInactive
			});
		}
	}

	// 4. Collect Zones
	if (show_zones) {
		const ViewBounds floor_bounds = view.getBoundsForFloor(view.floor);
		const int check_z = view.floor;
		const uint16_t active_zone = (g_brush_manager.zone_brush ? g_brush_manager.zone_brush->getZone() : 0);
		const bool recompute = !options.is_drawing;

		for (const auto& zone : map.zones) {
			if (zone.id == 0) {
				continue;
			}
			const auto& clusters = map.getZoneClusters(zone.id, recompute);
			if (clusters.empty()) {
				continue;
			}

			std::string_view label_text = zone.name;
			if (label_text.empty()) {
				s_frame_fallback_names.push_back(std::format("Zone {}", zone.id));
				label_text = s_frame_fallback_names.back();
			}
			const CachedMetrics m = getMetrics(label_text);
			const glm::vec3 rgb = getZoneColor(zone.id);
			const bool is_active = (active_zone == zone.id);

			for (const auto& cluster : clusters) {
				if (cluster.centroid_pos.z != check_z) {
					continue;
				}

				// AABB intersection against view bounds on active floor
				const int vis_min_x = std::max(cluster.min_x, floor_bounds.start_x);
				const int vis_max_x = std::min(cluster.max_x, floor_bounds.end_x);
				const int vis_min_y = std::max(cluster.min_y, floor_bounds.start_y);
				const int vis_max_y = std::min(cluster.max_y, floor_bounds.end_y);

				if (vis_min_x > vis_max_x || vis_min_y > vis_max_y) {
					continue;
				}

				// Pick anchor point: use centroid if inside visible bounds, otherwise clamp to visible center
				int anchor_x = cluster.centroid_pos.x;
				int anchor_y = cluster.centroid_pos.y;
				if (anchor_x < vis_min_x || anchor_x > vis_max_x || anchor_y < vis_min_y || anchor_y > vis_max_y) {
					anchor_x = (vis_min_x + vis_max_x) / 2;
					anchor_y = (vis_min_y + vis_max_y) / 2;
				}

				int unscaled_x = 0, unscaled_y = 0;
				if (!view.IsTileVisible(anchor_x, anchor_y, check_z, unscaled_x, unscaled_y)) {
					continue;
				}

				const float screen_x = static_cast<float>(unscaled_x) * inv_zoom;
				const float screen_y = static_cast<float>(unscaled_y) * inv_zoom;
				if (screen_x < -64.0f || screen_x > screen_max_x || screen_y < -64.0f || screen_y > screen_max_y) {
					continue;
				}

				const float labelX = screen_x + tile_size_screen * 0.5f;
				const float labelY = resolveCollision(labelX, screen_y + tile_size_screen * 0.5f + (m.height * 0.5f) - paddingY, m.width, m.height);

				visible_labels.push_back(VisibleMarkerLabel {
					.x = labelX,
					.y = labelY,
					.width = m.width,
					.height = m.height,
					.text = label_text,
					.type = MarkerLabelType::Zone,
					.custom_color = nvgRGBAf(rgb.r, rgb.g, rgb.b, 1.0f),
					.is_active = is_active
				});
			}
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
		} else if (vl.type == MarkerLabelType::Zone) {
			nvgFillColor(vg, vl.is_active ? nvgRGBA(25, 25, 38, 245) : nvgRGBA(15, 15, 22, 242));
			nvgFill(vg);
			nvgStrokeColor(vg, vl.custom_color);
			nvgStrokeWidth(vg, vl.is_active ? 1.8f : 1.2f);
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
		} else if (vl.type == MarkerLabelType::Zone) {
			nvgFillColor(vg, vl.custom_color);
		} else { // MarkerLabelType::HouseInactive
			nvgFillColor(vg, nvgRGBA(143, 127, 196, 255));
		}
		nvgText(vg, vl.x, vl.y - paddingY, vl.text.data(), vl.text.data() + vl.text.size());
	}
}

} // namespace rme::rendering
