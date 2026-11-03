#include "rendering/drawers/overlays/server_marker_drawer.h"

#include "game/srv_markers.h"
#include "map/map.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/render_view.h"
#include "ui/theme.h"
#include "util/nvg_utils.h"

#include <nanovg.h>

#include <array>
#include <string_view>

namespace rme::rendering {

	using rme::srv::MarkerKind;

	namespace {

		constexpr float kMaxZoom = 10.0f; // labels are not readable further out
		constexpr float kBadgeMaxZoom = 2.0f;
		constexpr float kCullMargin = 64.0f;
		constexpr float kFrameInset = 1.5f;
		constexpr float kFrameWidth = 1.5f;
		constexpr float kFrameRadius = 3.0f;
		constexpr float kBadgeFontSize = 10.0f;
		constexpr float kBadgePaddingX = 3.0f;
		constexpr float kBadgeHeight = 12.0f;
		constexpr float kBadgeRadius = 2.0f;
		constexpr unsigned char kAreaFillAlpha = 28;
		constexpr unsigned char kAreaStrokeAlpha = 150;

		// Badge text per MarkerKind, in enum order.
		constexpr std::array<std::string_view, 4> kBadgeTexts { "start", "depot", "raid", "script" };

		NVGcolor withAlpha(NVGcolor color, unsigned char alpha) {
			color.a = static_cast<float>(alpha) / 255.0f;
			return color;
		}

	} // namespace

	Theme::Role serverMarkerRole(MarkerKind kind) {
		switch (kind) {
			case MarkerKind::Start:
				return Theme::Role::ServerMarkerStart;
			case MarkerKind::Depot:
				return Theme::Role::ServerMarkerDepot;
			case MarkerKind::Raid:
				return Theme::Role::ServerMarkerRaid;
			case MarkerKind::Script:
				break;
		}
		return Theme::Role::ServerMarkerScript;
	}

	void ServerMarkerDrawer::draw(NVGcontext* vg, const Map& map, const RenderView& view, const DrawingOptions& options) const {
		if (!vg || options.ingame || !options.show_server_markers || !map.server_markers || view.zoom > kMaxZoom) {
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
		const bool badges = view.zoom <= kBadgeMaxZoom;
		const NVGcolor badge_text = NvgUtils::ToNvColor(Theme::Get(Theme::Role::TooltipBg));

		nvgFontSize(vg, kBadgeFontSize);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

		for (const rme::srv::Marker& marker : markers) {
			const Position& position = marker.position;
			int unscaled_x = 0;
			int unscaled_y = 0;
			view.getScreenPosition(position.x, position.y, position.z, unscaled_x, unscaled_y);
			const float x = static_cast<float>(unscaled_x) * inv_zoom;
			const float y = static_cast<float>(unscaled_y) * inv_zoom;
			const float spread = static_cast<float>(marker.raid_spread) * tile;
			if (x + tile + spread < -kCullMargin || y + tile + spread < -kCullMargin || x - spread > max_x || y - spread > max_y) {
				continue;
			}

			const NVGcolor color = NvgUtils::ToNvColor(Theme::Get(serverMarkerRole(marker.kind)));

			// The square a raid's monsters can appear in.
			if (marker.raid_spread > 0) {
				const NVGcolor raid = NvgUtils::ToNvColor(Theme::Get(Theme::Role::ServerMarkerRaid));
				nvgBeginPath(vg);
				nvgRect(vg, x - spread, y - spread, tile + spread * 2.0f, tile + spread * 2.0f);
				nvgFillColor(vg, withAlpha(raid, kAreaFillAlpha));
				nvgFill(vg);
				nvgStrokeColor(vg, withAlpha(raid, kAreaStrokeAlpha));
				nvgStrokeWidth(vg, 1.0f);
				nvgStroke(vg);
			}

			nvgBeginPath(vg);
			nvgRoundedRect(vg, x + kFrameInset, y + kFrameInset, tile - kFrameInset * 2.0f, tile - kFrameInset * 2.0f, kFrameRadius);
			nvgStrokeColor(vg, color);
			nvgStrokeWidth(vg, kFrameWidth);
			nvgStroke(vg);

			if (!badges) {
				continue;
			}
			const std::string_view text = kBadgeTexts[static_cast<size_t>(marker.kind)];
			float bounds[4];
			nvgTextBounds(vg, 0.0f, 0.0f, text.data(), text.data() + text.size(), bounds);
			const float width = bounds[2] - bounds[0] + kBadgePaddingX * 2.0f;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x + kFrameInset, y + kFrameInset, width, kBadgeHeight, kBadgeRadius);
			nvgFillColor(vg, color);
			nvgFill(vg);
			nvgFillColor(vg, badge_text);
			nvgText(vg, x + kFrameInset + kBadgePaddingX, y + kFrameInset + kBadgeHeight * 0.5f, text.data(), text.data() + text.size());
		}
	}

} // namespace rme::rendering
