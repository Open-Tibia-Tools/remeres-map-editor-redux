#include "rendering/drawers/overlays/server_marker_drawer.h"

#include "map/map.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/render_view.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include "ui/theme.h"
#include "util/nvg_utils.h"

#include <nanovg.h>

#include <glm/vec3.hpp>

namespace rme::rendering {

	using rme::srv::MarkerKind;

	namespace {

		constexpr float kMaxZoom = 10.0f; // labels are not readable further out
		constexpr float kCullMargin = 64.0f;
		constexpr float kShortLabelBelow = 28.0f; // tile size in pixels, as the tile indicators use
		constexpr float kNoLabelBelow = 12.0f;
		constexpr unsigned char kAreaFillAlpha = 28;
		constexpr unsigned char kAreaStrokeAlpha = 150;

		struct KindStyle {
			Theme::Role role;
			const char* label;
			const char* short_label;
			std::string_view source;
		};

		KindStyle styleOf(MarkerKind kind) {
			switch (kind) {
				case MarkerKind::Start:
					return { Theme::Role::ServerMarkerStart, "start", "st", "map.dat" };
				case MarkerKind::Depot:
					return { Theme::Role::ServerMarkerDepot, "depot", "dp", "moveuse.dat" };
				case MarkerKind::Raid:
					return { Theme::Role::ServerMarkerRaid, "raid", "rd", "Raid (mon/*.evt)" };
				case MarkerKind::Script:
					break;
			}
			return { Theme::Role::ServerMarkerScript, "script", "sc", "moveuse.dat" };
		}

		NVGcolor withAlpha(NVGcolor color, unsigned char alpha) {
			color.a = static_cast<float>(alpha) / 255.0f;
			return color;
		}

	} // namespace

	void drawServerMarkerBadge(NVGcontext* vg, MarkerKind kind, float x, float y, float size) {
		const KindStyle style = styleOf(kind);
		const wxColour color = Theme::Get(style.role);
		const glm::vec3 rgb(color.Red() / 255.0f, color.Green() / 255.0f, color.Blue() / 255.0f);
		const char* label = size < kNoLabelBelow ? nullptr : (size < kShortLabelBelow ? style.short_label : style.label);
		DrawNanoVGCustomIndicatorBadge(vg, x, y, size, rgb, label);
	}

	std::string_view serverMarkerSource(MarkerKind kind) {
		return styleOf(kind).source;
	}

	void ServerMarkerDrawer::draw(NVGcontext* vg, const Map& map, const RenderView& view, const DrawingOptions& options) const {
		const rme::srv::MarkerKindSet shown = options.server_marker_kinds;
		if (!vg || options.ingame || shown == 0 || !map.server_markers || view.zoom > kMaxZoom) {
			return;
		}
		const auto markers = map.server_markers->onFloor(view.floor);
		if (markers.empty()) {
			return;
		}

		const float inv_zoom = 1.0f / view.zoom;
		const float tile = 32.0f * inv_zoom;
		const float max_x = static_cast<float>(view.screensize_x) + kCullMargin;
		const float max_y = static_cast<float>(view.screensize_y) + kCullMargin;
		const bool raids_shown = (shown & rme::srv::kindBit(MarkerKind::Raid)) != 0;
		const NVGcolor raid_color = NvgUtils::ToNvColor(Theme::Get(Theme::Role::ServerMarkerRaid));

		for (const rme::srv::Marker& marker : markers) {
			const auto kind = marker.shownKind(shown);
			if (!kind) {
				continue;
			}
			const Position& position = marker.position;
			int unscaled_x = 0;
			int unscaled_y = 0;
			view.getScreenPosition(position.x, position.y, position.z, unscaled_x, unscaled_y);
			const float x = static_cast<float>(unscaled_x) * inv_zoom;
			const float y = static_cast<float>(unscaled_y) * inv_zoom;
			const float spread = raids_shown ? static_cast<float>(marker.raid_spread) * tile : 0.0f;
			if (x + tile + spread < -kCullMargin || y + tile + spread < -kCullMargin || x - spread > max_x || y - spread > max_y) {
				continue;
			}

			// The square a raid's monsters can appear in.
			if (spread > 0.0f) {
				nvgBeginPath(vg);
				nvgRect(vg, x - spread, y - spread, tile + spread * 2.0f, tile + spread * 2.0f);
				nvgFillColor(vg, withAlpha(raid_color, kAreaFillAlpha));
				nvgFill(vg);
				nvgStrokeColor(vg, withAlpha(raid_color, kAreaStrokeAlpha));
				nvgStrokeWidth(vg, 1.0f);
				nvgStroke(vg);
			}

			drawServerMarkerBadge(vg, *kind, x, y, tile);
		}
	}

} // namespace rme::rendering
