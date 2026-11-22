#include "app/definitions.h"

#include "brushes/brush_footprint.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
	constexpr double EPSILON = 0.005;

	[[nodiscard]] int computeSpan(int value, bool exact) {
		return exact ? std::max(1, value) : std::max(0, value) * 2 + 1;
	}
}

int NormalizeBrushAxisValue(int value, bool exact) {
	const int min_value = exact ? 1 : 0;
	return std::clamp(value, min_value, 15);
}

BrushFootprint MakeBrushFootprint(const BrushSizeState& state) {
	BrushFootprint footprint;
	footprint.shape = state.shape;
	footprint.exact = state.exact;
	footprint.size_x = NormalizeBrushAxisValue(state.size_x, state.exact);
	footprint.size_y = NormalizeBrushAxisValue(state.size_y, state.exact);
	footprint.span_x = computeSpan(footprint.size_x, footprint.exact);
	footprint.span_y = computeSpan(footprint.size_y, footprint.exact);

	if (!footprint.exact) {
		footprint.min_offset_x = -footprint.size_x;
		footprint.max_offset_x = footprint.size_x;
		footprint.min_offset_y = -footprint.size_y;
		footprint.max_offset_y = footprint.size_y;
		return footprint;
	}

	const auto assign_axis = [](int span, int& min_offset, int& max_offset) {
		if (span % 2 == 1) {
			min_offset = -(span / 2);
			max_offset = span / 2;
			return;
		}

		min_offset = -(span - 1);
		max_offset = 0;
	};

	assign_axis(footprint.span_x, footprint.min_offset_x, footprint.max_offset_x);
	assign_axis(footprint.span_y, footprint.min_offset_y, footprint.max_offset_y);
	return footprint;
}

bool BrushFootprint::containsOffset(int dx, int dy) const {
	if (dx < min_offset_x || dx > max_offset_x || dy < min_offset_y || dy > max_offset_y) {
		return false;
	}

	if (shape == BRUSHSHAPE_SQUARE) {
		return true;
	}

	const auto safe_normalize = [](double delta, double radius) {
		if (radius <= 0.0) {
			return delta == 0.0 ? 0.0 : std::numeric_limits<double>::infinity();
		}
		return delta / radius;
	};

	if (!exact) {
		const double nx = safe_normalize(static_cast<double>(dx), static_cast<double>(size_x));
		const double ny = safe_normalize(static_cast<double>(dy), static_cast<double>(size_y));
		return nx * nx + ny * ny < 1.0 + EPSILON;
	}

	const double center_x = (static_cast<double>(min_offset_x) + static_cast<double>(max_offset_x)) / 2.0;
	const double center_y = (static_cast<double>(min_offset_y) + static_cast<double>(max_offset_y)) / 2.0;
	const double radius_x = static_cast<double>(span_x) / 2.0;
	const double radius_y = static_cast<double>(span_y) / 2.0;
	const double nx = safe_normalize(static_cast<double>(dx) - center_x, radius_x);
	const double ny = safe_normalize(static_cast<double>(dy) - center_y, radius_y);
	return nx * nx + ny * ny <= 1.0 + EPSILON;
}

int BrushFootprint::maxReachX() const {
	return std::max(std::abs(min_offset_x), std::abs(max_offset_x));
}

int BrushFootprint::maxReachY() const {
	return std::max(std::abs(min_offset_y), std::abs(max_offset_y));
}

int BrushFootprint::legacySize() const {
	return std::max(size_x, size_y);
}
