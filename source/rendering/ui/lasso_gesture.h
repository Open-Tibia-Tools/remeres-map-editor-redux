//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_UI_LASSO_GESTURE_H_
#define RME_RENDERING_UI_LASSO_GESTURE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <vector>

// Alt-held lasso gesture state machine (no wxWidgets, no editor access).
//
// Start: left button down while Alt is held and Ctrl is not (Ctrl+Alt+Left stays smart pick).
// While active: left-drag appends a point whenever the cursor is at least the sample spacing away
// from the last point; each further left click adds a vertex.
// Finalize: the Alt key-up. The mode comes from the modifiers held at that moment:
// none = Replace, Shift = Add, Ctrl or Ctrl+Shift = Subtract.
// Cancel: Escape, right click, focus or capture loss, tab or map switch, and any input event that
// reports Alt as no longer held (its key-up was lost, so the gesture must not finalize).
// Fewer than 3 points or a zero-area polygon is discarded without touching the selection.
// Button releases whose press the lasso consumed are swallowed, even after the lasso ended.

enum class LassoMode {
	Replace,
	Add,
	Subtract,
};

[[nodiscard]] constexpr LassoMode LassoModeFromModifiers(bool shift_down, bool ctrl_down) noexcept {
	if (ctrl_down) {
		return LassoMode::Subtract;
	}
	if (shift_down) {
		return LassoMode::Add;
	}
	return LassoMode::Replace;
}

[[nodiscard]] constexpr const char* LassoModeLabel(LassoMode mode) noexcept {
	switch (mode) {
		case LassoMode::Add:
			return "Add";
		case LassoMode::Subtract:
			return "Sub";
		case LassoMode::Replace:
		default:
			return "Replace";
	}
}

// Canvas client coordinates, the same space as mouse events and the NanoVG overlay.
struct LassoScreenPoint {
	int x = 0;
	int y = 0;

	friend constexpr bool operator==(const LassoScreenPoint&, const LassoScreenPoint&) = default;
};

struct LassoModifiers {
	bool alt = false;
	bool shift = false;
	bool ctrl = false;
};

enum class LassoButton {
	Left,
	Right,
	Middle,
};

enum class LassoEvent {
	Ignored, // Not lasso input; normal handling continues.
	Consumed, // The lasso swallowed the event.
	Started,
	Committed, // Points() and Mode() hold the polygon until the next start.
	Discarded, // Ended without a valid polygon; the selection stays unchanged.
	Cancelled,
};

[[nodiscard]] constexpr bool IsLassoEventConsumed(LassoEvent event) noexcept {
	return event != LassoEvent::Ignored;
}

// True when the polygon encloses a non-zero area: the fan triangles from the first point are not
// all degenerate (exact in 64-bit integers).
[[nodiscard]] constexpr bool HasLassoArea(std::span<const LassoScreenPoint> points) noexcept {
	if (points.size() < 3) {
		return false;
	}
	const LassoScreenPoint& origin = points.front();
	for (std::size_t i = 1; i + 1 < points.size(); ++i) {
		const std::int64_t ax = points[i].x - origin.x;
		const std::int64_t ay = points[i].y - origin.y;
		const std::int64_t bx = points[i + 1].x - origin.x;
		const std::int64_t by = points[i + 1].y - origin.y;
		if (ax * by - ay * bx != 0) {
			return true;
		}
	}
	return false;
}

// Same as HasLassoArea for the polygon closed through the pending rubber-band cursor. `append`
// must match whether the caller will add the cursor as a final vertex (false at the point cap or
// when the cursor duplicates the last sampled point).
[[nodiscard]] constexpr bool HasLassoAreaWithCursor(std::span<const LassoScreenPoint> points, LassoScreenPoint cursor, bool append) noexcept {
	if (!append) {
		return HasLassoArea(points);
	}
	if (points.empty()) {
		return false;
	}
	if (HasLassoArea(points)) {
		return true;
	}
	if (points.size() < 2) {
		return false;
	}
	const LassoScreenPoint& origin = points.front();
	const LassoScreenPoint& last = points.back();
	const std::int64_t ax = last.x - origin.x;
	const std::int64_t ay = last.y - origin.y;
	const std::int64_t bx = cursor.x - origin.x;
	const std::int64_t by = cursor.y - origin.y;
	return ax * by - ay * bx != 0;
}

[[nodiscard]] inline std::string FormatLassoStatus(LassoMode mode, std::size_t point_count, bool at_point_limit) {
	// U+2014 EM DASH, spelled as UTF-8 bytes so the literal does not depend on the source charset.
	return std::format("[{}] {} {}{} \xE2\x80\x94 Release Alt to apply", LassoModeLabel(mode), point_count, point_count == 1 ? "point" : "points", at_point_limit ? " (limit reached)" : "");
}

class LassoGesture {
public:
	static constexpr std::size_t kMaxPoints = 4096;
	// Freehand sampling distance in device-independent pixels.
	static constexpr int kSampleSpacingDip = 8;

	LassoGesture() {
		points.reserve(kMaxPoints);
		click_vertices.reserve(kMaxPoints);
	}

	[[nodiscard]] bool IsActive() const noexcept {
		return active;
	}
	[[nodiscard]] bool IsLeftDown() const noexcept {
		return active && left_down;
	}
	[[nodiscard]] std::span<const LassoScreenPoint> Points() const noexcept {
		return points;
	}
	// 1 for vertices placed by a click, 0 for freehand drag samples. Empty for a freehand lasso,
	// so the overlay draws no handles while dragging; click-vertex lassos keep their handles.
	[[nodiscard]] std::span<const std::uint8_t> HandleVertices() const noexcept {
		return freehand ? std::span<const std::uint8_t> {} : std::span<const std::uint8_t>(click_vertices);
	}
	[[nodiscard]] LassoScreenPoint Cursor() const noexcept {
		return cursor;
	}
	[[nodiscard]] LassoMode Mode() const noexcept {
		return mode;
	}
	[[nodiscard]] bool IsAtPointLimit() const noexcept {
		return at_point_limit;
	}

	// can_start is the caller's eligibility (selection mode, not pasting, not dragging).
	// sample_spacing_px is kSampleSpacingDip converted to canvas pixels; used when a lasso starts.
	LassoEvent OnButtonDown(LassoButton button, LassoScreenPoint p, LassoModifiers mods, bool can_start, int sample_spacing_px) {
		auto& swallow = SwallowFlag(button);
		if (!active) {
			swallow = false;
			if (button != LassoButton::Left || !can_start || !mods.alt || mods.ctrl) {
				return LassoEvent::Ignored;
			}
			Start(p, mods, sample_spacing_px);
			swallow = true;
			return LassoEvent::Started;
		}

		swallow = true;
		if (!mods.alt) {
			return End(LassoEvent::Cancelled);
		}
		switch (button) {
			case LassoButton::Right:
				return End(LassoEvent::Cancelled);
			case LassoButton::Middle:
				UpdateModifiers(mods);
				return LassoEvent::Consumed;
			case LassoButton::Left:
				break;
		}

		UpdateModifiers(mods);
		cursor = p;
		left_down = true;
		AppendPoint(p, true);
		return LassoEvent::Consumed;
	}

	LassoEvent OnButtonUp(LassoButton button, LassoScreenPoint p, LassoModifiers mods) {
		auto& swallow = SwallowFlag(button);
		const bool swallowed = swallow;
		swallow = false;
		if (!active) {
			return swallowed ? LassoEvent::Consumed : LassoEvent::Ignored;
		}

		if (button == LassoButton::Left) {
			left_down = false;
		}
		if (!mods.alt) {
			return End(LassoEvent::Cancelled);
		}
		UpdateModifiers(mods);
		cursor = p;
		return LassoEvent::Consumed;
	}

	LassoEvent OnMotion(LassoScreenPoint p, LassoModifiers mods, bool left_is_down) {
		if (!active) {
			if (!left_is_down) {
				// The release of a swallowed press happened outside the canvas.
				SwallowFlag(LassoButton::Left) = false;
			}
			return LassoEvent::Ignored;
		}
		if (!mods.alt) {
			return End(LassoEvent::Cancelled);
		}

		UpdateModifiers(mods);
		cursor = p;
		if (left_down && !left_is_down) {
			left_down = false;
		}
		if (left_down && !points.empty()) {
			const std::int64_t dx = p.x - points.back().x;
			const std::int64_t dy = p.y - points.back().y;
			if (dx * dx + dy * dy >= sample_spacing_sq) {
				freehand = true;
				AppendPoint(p, false);
			}
		}
		return LassoEvent::Consumed;
	}

	// Shift or Ctrl pressed or released while the lasso is active.
	LassoEvent OnModifierKey(LassoModifiers mods) {
		if (!active) {
			return LassoEvent::Ignored;
		}
		if (!mods.alt) {
			return End(LassoEvent::Cancelled);
		}
		UpdateModifiers(mods);
		return LassoEvent::Consumed;
	}

	// The Alt key-up reached the canvas: close the polygon and apply it.
	LassoEvent OnAltReleased(LassoModifiers mods) {
		if (!active) {
			return LassoEvent::Ignored;
		}
		UpdateModifiers(mods);
		// The rubber band is the last visible edge; commit the point it ends at, not the older
		// sampled point, so the applied polygon matches the drawn gesture.
		AppendPoint(cursor, false);
		if (points.size() < 3 || !HasLassoArea(points)) {
			return End(LassoEvent::Discarded);
		}
		active = false;
		left_down = false;
		return LassoEvent::Committed;
	}

	LassoEvent Cancel() {
		if (!active) {
			return LassoEvent::Ignored;
		}
		return End(LassoEvent::Cancelled);
	}

private:
	void Start(LassoScreenPoint p, LassoModifiers mods, int sample_spacing_px) {
		const std::int64_t spacing = sample_spacing_px > 1 ? sample_spacing_px : 1;
		sample_spacing_sq = spacing * spacing;
		points.clear();
		click_vertices.clear();
		points.push_back(p);
		click_vertices.push_back(1);
		freehand = false;
		at_point_limit = false;
		cursor = p;
		left_down = true;
		active = true;
		UpdateModifiers(mods);
	}

	LassoEvent End(LassoEvent result) {
		active = false;
		left_down = false;
		points.clear();
		click_vertices.clear();
		freehand = false;
		at_point_limit = false;
		return result;
	}

	void UpdateModifiers(LassoModifiers mods) {
		mode = LassoModeFromModifiers(mods.shift, mods.ctrl);
	}

	void AppendPoint(LassoScreenPoint p, bool from_click) {
		if (!points.empty() && points.back() == p) {
			return;
		}
		if (points.size() >= kMaxPoints) {
			at_point_limit = true;
			return;
		}
		points.push_back(p);
		click_vertices.push_back(from_click ? 1 : 0);
	}

	bool& SwallowFlag(LassoButton button) {
		return swallow_release[static_cast<std::size_t>(button)];
	}

	std::vector<LassoScreenPoint> points;
	std::vector<std::uint8_t> click_vertices;
	LassoScreenPoint cursor;
	LassoMode mode = LassoMode::Replace;
	std::int64_t sample_spacing_sq = 1;
	bool active = false;
	bool left_down = false;
	bool freehand = false;
	bool at_point_limit = false;
	std::array<bool, 3> swallow_release {};
};

#endif
