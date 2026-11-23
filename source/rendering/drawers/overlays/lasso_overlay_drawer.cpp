//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/drawers/overlays/lasso_overlay_drawer.h"

#include <nanovg.h>

#include "ui/theme.h"

namespace {

	constexpr unsigned char kRubberBandAlpha = 160;

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

void LassoOverlayDrawer::draw(NVGcontext* vg, std::span<const LassoScreenPoint> points, std::span<const std::uint8_t> handle_vertices, LassoScreenPoint cursor, const LassoOverlayStyle& style) const {
	if (!vg || points.empty()) {
		return;
	}

	const NVGcolor outline = ThemeColor(Theme::Role::Accent, 255);

	nvgSave(vg);
	nvgLineJoin(vg, NVG_ROUND);
	nvgLineCap(vg, NVG_ROUND);

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

	bool any_handle = false;
	nvgBeginPath(vg);
	for (std::size_t i = 0; i < points.size() && i < handle_vertices.size(); ++i) {
		if (handle_vertices[i] != 0) {
			AddHandle(vg, points[i], style.handle_radius);
			any_handle = true;
		}
	}
	if (any_handle) {
		nvgFillColor(vg, outline);
		nvgFill(vg);
		nvgStrokeWidth(vg, style.line_width * 0.5f);
		nvgStrokeColor(vg, ThemeColor(Theme::Role::TextOnAccent, 255));
		nvgStroke(vg);
	}

	nvgRestore(vg);
}
