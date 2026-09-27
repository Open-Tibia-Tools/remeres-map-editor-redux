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
inline void DrawNanoVGIndicatorBadge(NVGcontext* vg, TileIndicatorType type, float bx, float by, float bsize) {
	if (!vg || type == TileIndicatorType::None) {
		return;
	}

	const auto style = GetIndicatorBadgeStyle(type);

	// 1. Background wash
	nvgBeginPath(vg);
	nvgRoundedRect(vg, bx, by, bsize, bsize, 3.0f);
	nvgFillColor(vg, nvgRGBA(style.bg_r, style.bg_g, style.bg_b, style.bg_a));
	nvgFill(vg);

	// 2. Vibrant 1px border
	nvgBeginPath(vg);
	nvgRoundedRect(vg, bx + 0.5f, by + 0.5f, bsize - 1.0f, bsize - 1.0f, 3.0f);
	nvgStrokeColor(vg, nvgRGBA(style.border_r, style.border_g, style.border_b, 255));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	// 3. Crisp centered typography with text shadow
	if (style.text && style.text[0] != '\0') {
		const float fontSize = std::clamp(bsize * 0.28f, 9.0f, 18.0f);
		nvgFontSize(vg, fontSize);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		const float cx = bx + bsize * 0.5f;
		const float cy = by + bsize * 0.5f;

		// Dark contrast outline
		nvgFillColor(vg, nvgRGBA(style.outline_r, style.outline_g, style.outline_b, 240));
		nvgText(vg, cx + 1.0f, cy, style.text, nullptr);
		nvgText(vg, cx - 1.0f, cy, style.text, nullptr);
		nvgText(vg, cx, cy + 1.0f, style.text, nullptr);
		nvgText(vg, cx, cy - 1.0f, style.text, nullptr);

		// White foreground text
		nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
		nvgText(vg, cx, cy, style.text, nullptr);
	}
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_INDICATOR_DRAWING_UTILS_H_
