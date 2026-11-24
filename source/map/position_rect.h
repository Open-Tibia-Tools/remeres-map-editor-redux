//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MAP_POSITION_RECT_H_
#define RME_MAP_POSITION_RECT_H_

#include "map/position.h"

#include <algorithm>
#include <cstdint>

// Inclusive tile rectangle on one floor. The magic wand uses it to keep every search inside
// the visible part of the canvas (plus a small tolerance for edge tiles and border rings).
struct PositionRect {
	int min_x = 0;
	int min_y = 0;
	int max_x = -1;
	int max_y = -1;

	[[nodiscard]] bool empty() const noexcept {
		return min_x > max_x || min_y > max_y;
	}

	[[nodiscard]] bool contains(const Position& position) const noexcept {
		return position.x >= min_x && position.x <= max_x && position.y >= min_y && position.y <= max_y;
	}

	// Grows the rectangle by `tiles` on every side but never past the map limits.
	[[nodiscard]] PositionRect expanded(int tiles) const noexcept {
		if (empty() || tiles <= 0) {
			return *this;
		}
		PositionRect grown = *this;
		grown.min_x = std::max(0, min_x - tiles);
		grown.min_y = std::max(0, min_y - tiles);
		grown.max_x = std::min(static_cast<int>(MAP_MAX_WIDTH), max_x + tiles);
		grown.max_y = std::min(static_cast<int>(MAP_MAX_HEIGHT), max_y + tiles);
		return grown;
	}

	[[nodiscard]] PositionRect clampedToMap() const noexcept {
		if (empty()) {
			return *this;
		}
		PositionRect clamped = *this;
		clamped.min_x = std::max(0, min_x);
		clamped.min_y = std::max(0, min_y);
		clamped.max_x = std::min(static_cast<int>(MAP_MAX_WIDTH), max_x);
		clamped.max_y = std::min(static_cast<int>(MAP_MAX_HEIGHT), max_y);
		return clamped;
	}

	[[nodiscard]] uint64_t tileCount() const noexcept {
		if (empty()) {
			return 0;
		}
		return static_cast<uint64_t>(max_x - min_x + 1) * static_cast<uint64_t>(max_y - min_y + 1);
	}

	// Builds a rectangle from two opposite corners in any order. Negative coordinates stay
	// as they are; clamping is a separate step.
	[[nodiscard]] static PositionRect fromCorners(const Position& a, const Position& b) noexcept {
		PositionRect rect;
		rect.min_x = std::min(a.x, b.x);
		rect.min_y = std::min(a.y, b.y);
		rect.max_x = std::max(a.x, b.x);
		rect.max_y = std::max(a.y, b.y);
		return rect;
	}

	friend bool operator==(const PositionRect&, const PositionRect&) = default;
};

#endif
