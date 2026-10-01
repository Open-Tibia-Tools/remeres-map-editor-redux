#ifndef RME_RENDERING_INDICATORS_INDICATOR_DRAWING_UTILS_H_
#define RME_RENDERING_INDICATORS_INDICATOR_DRAWING_UTILS_H_

#include "rendering/indicators/technical_item_registry.h"
#include <nanovg.h>
#include <algorithm>

namespace rme::rendering {

/**
 * @brief Renders a crisp NanoVG tile indicator badge with background wash, border, and shadowed typography.
 *
 * Used uniformly across palette brush grids, item selection grids, and tool options surfaces.
 */
inline void DrawNanoVGIndicatorBadge(
	NVGcontext* vg,
	TileIndicatorType type,
	float bx,
	float by,
	float bsize,
	bool force_short_label = false
) {
	if (!vg || type == TileIndicatorType::None) {
		return;
	}

	const auto style = GetIndicatorBadgeStyle(type);

	// 1. Outer 2px solid black border
	nvgBeginPath(vg);
	nvgRoundedRect(vg, bx, by, bsize, bsize, 2.0f);
	nvgFillColor(vg, nvgRGBA(13, 13, 18, 255));
	nvgFill(vg);

	// 2. Interior Wash Fill
	nvgBeginPath(vg);
	nvgRect(vg, bx + 2.0f, by + 2.0f, bsize - 4.0f, bsize - 4.0f);
	nvgFillColor(vg, nvgRGBA(style.bg_r, style.bg_g, style.bg_b, style.bg_a));
	nvgFill(vg);

	// 3. 3D Bevel inside the 2px black border
	// Top & Left: 1px light highlight
	nvgBeginPath(vg);
	nvgMoveTo(vg, bx + 2.0f, by + bsize - 2.0f);
	nvgLineTo(vg, bx + 2.0f, by + 2.0f);
	nvgLineTo(vg, bx + bsize - 2.0f, by + 2.0f);
	nvgStrokeColor(vg, nvgRGBA(style.light_r, style.light_g, style.light_b, 220));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	// Bottom & Right: 2px dark shadow
	const float shadowThickness = bsize >= 24.0f ? 2.0f : 1.0f;
	nvgBeginPath(vg);
	nvgMoveTo(vg, bx + 2.0f, by + bsize - 2.0f - shadowThickness * 0.5f);
	nvgLineTo(vg, bx + bsize - 2.0f - shadowThickness * 0.5f, by + bsize - 2.0f - shadowThickness * 0.5f);
	nvgLineTo(vg, bx + bsize - 2.0f - shadowThickness * 0.5f, by + 2.0f);
	nvgStrokeColor(vg, nvgRGBA(style.dark_r, style.dark_g, style.dark_b, 240));
	nvgStrokeWidth(vg, shadowThickness);
	nvgStroke(vg);

	// 4. Centered bold badge pill with border and sharp typography
	const bool use_short = force_short_label || bsize < 28.0f;
	const char* label = (use_short && style.short_text && style.short_text[0] != '\0') ? style.short_text : style.text;
	if (label && label[0] != '\0') {
		const float font_size = std::clamp(bsize * (use_short ? 0.38f : 0.32f), 8.0f, 13.0f);
		nvgFontSize(vg, font_size);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		float tb[4];
		nvgTextBounds(vg, 0.0f, 0.0f, label, nullptr, tb);
		const float tw = tb[2] - tb[0];
		const float th = tb[3] - tb[1];

		const float pill_pad_x = std::max(2.0f, bsize * 0.08f);
		const float pill_pad_y = 1.5f;
		const float pill_w = std::min(bsize - 6.0f, tw + pill_pad_x * 2.0f);
		const float pill_h = std::min(bsize - 6.0f, th + pill_pad_y * 2.0f);
		const float pill_x = bx + (bsize - pill_w) * 0.5f;
		const float pill_y = by + (bsize - pill_h) * 0.5f;

		// Pill background (#0F0F16 dark pill)
		nvgBeginPath(vg);
		nvgRoundedRect(vg, pill_x, pill_y, pill_w, pill_h, 3.0f);
		nvgFillColor(vg, nvgRGBA(15, 15, 22, 245));
		nvgFill(vg);

		// Pill 1px stroke matching indicator border color
		nvgStrokeColor(vg, nvgRGBA(style.border_r, style.border_g, style.border_b, 245));
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		// Sharp centered text
		nvgFillColor(vg, nvgRGBA(style.border_r, style.border_g, style.border_b, 255));
		nvgText(vg, bx + bsize * 0.5f, by + bsize * 0.5f, label, nullptr);
	}
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_INDICATOR_DRAWING_UTILS_H_
