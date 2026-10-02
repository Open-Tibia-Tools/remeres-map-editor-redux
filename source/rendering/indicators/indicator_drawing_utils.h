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

	// 1. Outer 1px solid black border
	nvgBeginPath(vg);
	nvgRect(vg, bx, by, bsize, bsize);
	nvgFillColor(vg, nvgRGBA(13, 13, 18, 255));
	nvgFill(vg);

	// 2. Interior Wash Fill (50% opacity)
	nvgBeginPath(vg);
	nvgRect(vg, bx + 1.0f, by + 1.0f, bsize - 2.0f, bsize - 2.0f);
	nvgFillColor(vg, nvgRGBA(style.bg_r, style.bg_g, style.bg_b, style.bg_a));
	nvgFill(vg);

	// 3. Kitchen Tile Bevel (1px inner light border)
	// Top & Left: 1px pure white highlight
	nvgBeginPath(vg);
	nvgMoveTo(vg, bx + 1.5f, by + bsize - 2.0f);
	nvgLineTo(vg, bx + 1.5f, by + 1.5f);
	nvgLineTo(vg, bx + bsize - 2.0f, by + 1.5f);
	nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 235));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	// Bottom & Right: 1px bright light tint bevel
	nvgBeginPath(vg);
	nvgMoveTo(vg, bx + 2.0f, by + bsize - 1.5f);
	nvgLineTo(vg, bx + bsize - 1.5f, by + bsize - 1.5f);
	nvgLineTo(vg, bx + bsize - 1.5f, by + 2.0f);
	nvgStrokeColor(vg, nvgRGBA(style.light_r, style.light_g, style.light_b, 200));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	// 4. White lowercase typography with 8-way black outline
	const bool use_short = force_short_label || bsize < 28.0f;
	const char* label = (use_short && style.short_text && style.short_text[0] != '\0') ? style.short_text : style.text;
	if (label && label[0] != '\0') {
		const float font_size = std::clamp(bsize * (use_short ? 0.36f : 0.30f), 8.0f, 12.0f);
		nvgFontSize(vg, font_size);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		const float cx = bx + bsize * 0.5f;
		const float cy = by + bsize * 0.5f;

		// 8-way black outline
		nvgFillColor(vg, nvgRGBA(10, 10, 15, 255));
		constexpr float offsets[8][2] = {
			{ -1.0f, -1.0f }, {  0.0f, -1.0f }, {  1.0f, -1.0f },
			{ -1.0f,  0.0f },                   {  1.0f,  0.0f },
			{ -1.0f,  1.0f }, {  0.0f,  1.0f }, {  1.0f,  1.0f }
		};
		for (const auto& [ox, oy] : offsets) {
			nvgText(vg, cx + ox, cy + oy, label, nullptr);
		}

		// Centered pure white text
		nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
		nvgText(vg, cx, cy, label, nullptr);
	}
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_INDICATOR_DRAWING_UTILS_H_
