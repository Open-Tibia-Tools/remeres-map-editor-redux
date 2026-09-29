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

	const bool is_invalid = (type == TileIndicatorType::InvalidGround ||
	                         type == TileIndicatorType::InvalidItem ||
	                         type == TileIndicatorType::InvalidZone);

	if (is_invalid) {
		// Clean flat semi-transparent fills for invalid map content
		nvgBeginPath(vg);
		nvgRoundedRect(vg, bx, by, bsize, bsize, 2.0f);
		nvgFillColor(vg, nvgRGBA(style.bg_r, style.bg_g, style.bg_b, style.bg_a));
		nvgFill(vg);
		return;
	}

	// Zero-Fill CipSoft-style corner brackets with dark drop-shadow
	const float arm = std::max(3.0f, bsize * 0.25f);
	const float x0 = bx + 0.5f;
	const float y0 = by + 0.5f;
	const float x1 = bx + bsize - 0.5f;
	const float y1 = by + bsize - 0.5f;

	auto drawBracketSegments = [&](float strokeWidth, NVGcolor strokeColor) {
		nvgBeginPath(vg);
		// Top-Left corner
		nvgMoveTo(vg, x0, y0 + arm);
		nvgLineTo(vg, x0, y0);
		nvgLineTo(vg, x0 + arm, y0);

		// Top-Right corner
		nvgMoveTo(vg, x1 - arm, y0);
		nvgLineTo(vg, x1, y0);
		nvgLineTo(vg, x1, y0 + arm);

		// Bottom-Left corner
		nvgMoveTo(vg, x0, y1 - arm);
		nvgLineTo(vg, x0, y1);
		nvgLineTo(vg, x0 + arm, y1);

		// Bottom-Right corner
		nvgMoveTo(vg, x1 - arm, y1);
		nvgLineTo(vg, x1, y1);
		nvgLineTo(vg, x1, y1 - arm);

		// Midpoint ticks if badge is large enough
		if (bsize >= 20.0f) {
			const float mid = bsize * 0.5f;
			const float half_tick = std::max(2.0f, bsize * 0.08f);
			// Top & Bottom
			nvgMoveTo(vg, bx + mid - half_tick, y0);
			nvgLineTo(vg, bx + mid + half_tick, y0);
			nvgMoveTo(vg, bx + mid - half_tick, y1);
			nvgLineTo(vg, bx + mid + half_tick, y1);
			// Left & Right
			nvgMoveTo(vg, x0, by + mid - half_tick);
			nvgLineTo(vg, x0, by + mid + half_tick);
			nvgMoveTo(vg, x1, by + mid - half_tick);
			nvgLineTo(vg, x1, by + mid + half_tick);
		}

		nvgStrokeColor(vg, strokeColor);
		nvgStrokeWidth(vg, strokeWidth);
		nvgStroke(vg);
	};

	// 1. Dark outer drop-shadow
	drawBracketSegments(2.5f, nvgRGBA(10, 10, 15, 230));

	// 2. Vibrant core bracket stroke
	drawBracketSegments(1.2f, nvgRGBA(style.border_r, style.border_g, style.border_b, 255));

	// 3. Crisp centered typography with text shadow
	const bool use_short = force_short_label || bsize < 24.0f;
	const char* label = (use_short && style.short_text && style.short_text[0] != '\0') ? style.short_text : style.text;
	if (label && label[0] != '\0') {
		const float fontSize = use_short ? std::clamp(bsize * 0.55f, 7.0f, 11.0f) : std::clamp(bsize * 0.28f, 9.0f, 18.0f);
		nvgSave(vg);
		nvgScissor(vg, bx, by, bsize, bsize);
		nvgFontSize(vg, fontSize);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		const float cx = bx + bsize * 0.5f;
		const float cy = by + bsize * 0.5f;

		// Dark contrast outline
		nvgFillColor(vg, nvgRGBA(style.outline_r, style.outline_g, style.outline_b, 240));
		nvgText(vg, cx + 1.0f, cy, label, nullptr);
		nvgText(vg, cx - 1.0f, cy, label, nullptr);
		nvgText(vg, cx, cy + 1.0f, label, nullptr);
		nvgText(vg, cx, cy - 1.0f, label, nullptr);

		// White foreground text
		nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
		nvgText(vg, cx, cy, label, nullptr);
		nvgRestore(vg);
	}
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_INDICATOR_DRAWING_UTILS_H_
