#include "rendering/drawers/overlays/lasso_overlay_drawer.h"

#include <nanovg.h>

#include "ui/theme.h"

namespace {

	constexpr unsigned char kFillAlpha = 48;
	constexpr unsigned char kRubberBandAlpha = 160;
	// Freehand drags sample a point every few pixels; past this count only the first and last
	// points get a handle.
	constexpr size_t kMaxHandles = 64;

	void AddHandle(NVGcontext* vg, LassoScreenPoint point, float radius) {
		nvgCircle(vg, static_cast<float>(point.x), static_cast<float>(point.y), radius);
	}

	NVGcolor ThemeColor(Theme::Role role, unsigned char alpha) {
		const wxColour colour = Theme::Get(role);
		return nvgRGBA(colour.Red(), colour.Green(), colour.Blue(), alpha);
	}

	void TracePolyline(NVGcontext* vg, std::span<const LassoScreenPoint> points) {
		nvgMoveTo(vg, static_cast<float>(points.front().x), static_cast<float>(points.front().y));
		for (const LassoScreenPoint& point : points.subspan(1)) {
			nvgLineTo(vg, static_cast<float>(point.x), static_cast<float>(point.y));
		}
	}

} // namespace

void LassoOverlayDrawer::draw(NVGcontext* vg, std::span<const LassoScreenPoint> points, LassoScreenPoint cursor, const LassoOverlayStyle& style) const {
	if (!vg || points.empty()) {
		return;
	}

	const NVGcolor outline = ThemeColor(Theme::Role::Accent, 255);

	nvgSave(vg);
	nvgLineJoin(vg, NVG_ROUND);
	nvgLineCap(vg, NVG_ROUND);

	if (points.size() >= 3) {
		// NanoVG fills with the nonzero rule while the selection uses even-odd, so a loop traced
		// twice in the same direction looks filled here but selects only the tiles along its edges.
		nvgBeginPath(vg);
		TracePolyline(vg, points);
		nvgClosePath(vg);
		nvgFillColor(vg, ThemeColor(Theme::Role::SelectionFill, kFillAlpha));
		nvgFill(vg);
	}

	if (points.size() >= 2) {
		nvgBeginPath(vg);
		TracePolyline(vg, points);
		nvgStrokeWidth(vg, style.line_width);
		nvgStrokeColor(vg, outline);
		nvgStroke(vg);
	}

	nvgBeginPath(vg);
	nvgMoveTo(vg, static_cast<float>(points.back().x), static_cast<float>(points.back().y));
	nvgLineTo(vg, static_cast<float>(cursor.x), static_cast<float>(cursor.y));
	nvgStrokeWidth(vg, style.line_width);
	nvgStrokeColor(vg, ThemeColor(Theme::Role::Accent, kRubberBandAlpha));
	nvgStroke(vg);

	nvgBeginPath(vg);
	if (points.size() <= kMaxHandles) {
		for (const LassoScreenPoint& point : points) {
			AddHandle(vg, point, style.handle_radius);
		}
	} else {
		AddHandle(vg, points.front(), style.handle_radius);
		AddHandle(vg, points.back(), style.handle_radius);
	}
	nvgFillColor(vg, outline);
	nvgFill(vg);
	nvgStrokeWidth(vg, style.line_width * 0.5f);
	nvgStrokeColor(vg, ThemeColor(Theme::Role::TextOnAccent, 255));
	nvgStroke(vg);

	nvgRestore(vg);
}
