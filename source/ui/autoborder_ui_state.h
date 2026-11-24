#ifndef RME_UI_AUTOBORDER_UI_STATE_H_
#define RME_UI_AUTOBORDER_UI_STATE_H_

#include <string_view>

namespace rme::ui {

	[[nodiscard]] constexpr std::string_view AutoborderStatusLabel(bool enabled) noexcept {
		return enabled ? "Autoborder: ON (A)" : "Autoborder: off (A)";
	}

	[[nodiscard]] constexpr std::string_view AutoborderToggleStatusMessage(bool enabled) noexcept {
		return enabled ? "Autoborder enabled." : "Autoborder disabled.";
	}

	[[nodiscard]] constexpr bool ShouldShowAutoborderControls(bool brush_supports_autoborder) noexcept {
		return brush_supports_autoborder;
	}

	[[nodiscard]] constexpr bool ShouldRunAutoborderPreview(
		bool is_drawing_mode,
		bool brush_supports_autoborder,
		bool is_doodad_brush,
		bool autoborder_enabled,
		bool preview_enabled
	) noexcept {
		return is_drawing_mode && brush_supports_autoborder && !is_doodad_brush && autoborder_enabled && preview_enabled;
	}

	[[nodiscard]] constexpr bool ShouldCleanWallsInAutoborderPreview(bool is_wall_brush, bool is_door_brush) noexcept {
		return is_wall_brush && !is_door_brush;
	}

	[[nodiscard]] constexpr bool ShouldApplyPreviewBordersToDrawnTiles(bool is_door_brush) noexcept {
		return !is_door_brush;
	}

} // namespace rme::ui

#endif
