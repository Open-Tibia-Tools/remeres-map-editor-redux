#include "rendering/ui/lasso_gesture.h"

#include <catch2/catch_test_macros.hpp>

namespace {

	constexpr int kSpacing = 8;
	constexpr LassoModifiers kAlt { .alt = true };
	constexpr LassoModifiers kAltShift { .alt = true, .shift = true };
	constexpr LassoModifiers kAltCtrl { .alt = true, .ctrl = true };
	constexpr LassoModifiers kNone {};

	LassoEvent Press(LassoGesture& gesture, int x, int y, LassoModifiers mods = kAlt, bool can_start = true) {
		return gesture.OnButtonDown(LassoButton::Left, { x, y }, mods, can_start, kSpacing);
	}

	LassoEvent Release(LassoGesture& gesture, int x, int y, LassoModifiers mods = kAlt) {
		return gesture.OnButtonUp(LassoButton::Left, { x, y }, mods);
	}

	// Starts a lasso and drags a triangle-ish path with the left button held.
	void DragTriangle(LassoGesture& gesture) {
		REQUIRE(Press(gesture, 0, 0) == LassoEvent::Started);
		REQUIRE(gesture.OnMotion({ 40, 0 }, kAlt, true) == LassoEvent::Consumed);
		REQUIRE(gesture.OnMotion({ 40, 40 }, kAlt, true) == LassoEvent::Consumed);
	}

} // namespace

TEST_CASE("modifiers at Alt release pick the mode", "[lasso][gesture]") {
	REQUIRE(LassoModeFromModifiers(false, false) == LassoMode::Replace);
	REQUIRE(LassoModeFromModifiers(true, false) == LassoMode::Add);
	REQUIRE(LassoModeFromModifiers(false, true) == LassoMode::Subtract);
	REQUIRE(LassoModeFromModifiers(true, true) == LassoMode::Subtract);

	LassoGesture gesture;
	DragTriangle(gesture);
	REQUIRE(gesture.Mode() == LassoMode::Replace);
	REQUIRE(gesture.OnModifierKey(kAltShift) == LassoEvent::Consumed);
	REQUIRE(gesture.Mode() == LassoMode::Add);
	REQUIRE(gesture.OnAltReleased(LassoModifiers { .shift = false, .ctrl = true }) == LassoEvent::Committed);
	REQUIRE(gesture.Mode() == LassoMode::Subtract);
}

TEST_CASE("start requires Alt without Ctrl and the caller's permission", "[lasso][gesture]") {
	LassoGesture gesture;
	REQUIRE(Press(gesture, 0, 0, kNone) == LassoEvent::Ignored);
	REQUIRE(Press(gesture, 0, 0, kAltCtrl) == LassoEvent::Ignored); // smart pick chord
	REQUIRE(Press(gesture, 0, 0, kAlt, false) == LassoEvent::Ignored); // drawing mode, pasting...
	REQUIRE(gesture.OnButtonDown(LassoButton::Right, { 0, 0 }, kAlt, true, kSpacing) == LassoEvent::Ignored);
	REQUIRE_FALSE(gesture.IsActive());

	REQUIRE(Press(gesture, 0, 0, kAltShift) == LassoEvent::Started);
	REQUIRE(gesture.IsActive());
	REQUIRE(gesture.Points().size() == 1);
}

TEST_CASE("drag sampling respects the spacing threshold", "[lasso][gesture]") {
	LassoGesture gesture;
	REQUIRE(Press(gesture, 100, 100) == LassoEvent::Started);
	gesture.OnMotion({ 107, 100 }, kAlt, true);
	REQUIRE(gesture.Points().size() == 1);
	gesture.OnMotion({ 108, 100 }, kAlt, true);
	REQUIRE(gesture.Points().size() == 2);
	gesture.OnMotion({ 113, 105 }, kAlt, true); // distance sqrt(50) < 8 from (108, 100)
	REQUIRE(gesture.Points().size() == 2);
	gesture.OnMotion({ 114, 106 }, kAlt, true); // sqrt(72) >= 8
	REQUIRE(gesture.Points().size() == 3);
	REQUIRE(gesture.Cursor() == LassoScreenPoint { 114, 106 });

	// Moving without the button only moves the rubber band.
	REQUIRE(Release(gesture, 114, 106) == LassoEvent::Consumed);
	gesture.OnMotion({ 200, 200 }, kAlt, false);
	REQUIRE(gesture.Points().size() == 3);
	REQUIRE(gesture.Cursor() == LassoScreenPoint { 200, 200 });
}

TEST_CASE("clicks while Alt is held add vertices, Alt release with the button up commits", "[lasso][gesture]") {
	LassoGesture gesture;
	REQUIRE(Press(gesture, 0, 0) == LassoEvent::Started);
	REQUIRE(Release(gesture, 0, 0) == LassoEvent::Consumed);
	REQUIRE(Press(gesture, 2, 0) == LassoEvent::Consumed); // vertices ignore the drag spacing
	REQUIRE(Release(gesture, 2, 0) == LassoEvent::Consumed);
	REQUIRE(Press(gesture, 2, 2) == LassoEvent::Consumed);
	REQUIRE(Release(gesture, 2, 2) == LassoEvent::Consumed);
	REQUIRE(gesture.Points().size() == 3);
	REQUIRE_FALSE(gesture.IsLeftDown());

	REQUIRE(gesture.OnAltReleased(kNone) == LassoEvent::Committed);
	REQUIRE_FALSE(gesture.IsActive());
	REQUIRE(gesture.Points().size() == 3); // kept for the caller until the next start
	REQUIRE(Release(gesture, 2, 2, kNone) == LassoEvent::Ignored);
}

TEST_CASE("drag, click more vertices, release Alt", "[lasso][gesture]") {
	LassoGesture gesture;
	DragTriangle(gesture);
	REQUIRE(Release(gesture, 40, 40) == LassoEvent::Consumed);
	REQUIRE(Press(gesture, 0, 40) == LassoEvent::Consumed);
	REQUIRE(gesture.Points().size() == 4);
	REQUIRE(Release(gesture, 0, 40) == LassoEvent::Consumed);
	REQUIRE(gesture.OnAltReleased(kNone) == LassoEvent::Committed);
}

TEST_CASE("Alt release commits the rubber-band cursor", "[lasso][gesture]") {
	LassoGesture gesture;
	REQUIRE(Press(gesture, 0, 0) == LassoEvent::Started);
	REQUIRE(Release(gesture, 0, 0) == LassoEvent::Consumed);
	REQUIRE(Press(gesture, 40, 0) == LassoEvent::Consumed);
	REQUIRE(Release(gesture, 40, 0) == LassoEvent::Consumed);
	REQUIRE(Press(gesture, 40, 40) == LassoEvent::Consumed);
	REQUIRE(Release(gesture, 40, 40) == LassoEvent::Consumed);
	REQUIRE(gesture.OnMotion({ 0, 40 }, kAlt, false) == LassoEvent::Consumed);
	REQUIRE(gesture.OnAltReleased(kNone) == LassoEvent::Committed);
	REQUIRE(gesture.Points().size() == 4);
	REQUIRE(gesture.Points().back() == LassoScreenPoint { 0, 40 });

	// The cursor can also give a collinear sampled path a non-zero area; the drawn rubber band is
	// part of the applied polygon, so it commits instead of being discarded.
	LassoGesture collinear;
	REQUIRE(Press(collinear, 0, 0) == LassoEvent::Started);
	REQUIRE(collinear.OnMotion({ 20, 20 }, kAlt, true) == LassoEvent::Consumed);
	REQUIRE(collinear.OnMotion({ 40, 40 }, kAlt, true) == LassoEvent::Consumed);
	REQUIRE(collinear.OnMotion({ 0, 40 }, kAlt, false) == LassoEvent::Consumed);
	REQUIRE(collinear.OnAltReleased(kNone) == LassoEvent::Committed);
}

TEST_CASE("Alt release with the left button down commits and swallows the release", "[lasso][gesture]") {
	LassoGesture gesture;
	DragTriangle(gesture);
	REQUIRE(gesture.IsLeftDown());
	REQUIRE(gesture.OnAltReleased(kNone) == LassoEvent::Committed);
	REQUIRE_FALSE(gesture.IsLeftDown());
	// The pending release belongs to the lasso: it must not reach the normal click handling.
	REQUIRE(Release(gesture, 40, 40, kNone) == LassoEvent::Consumed);
	REQUIRE(Release(gesture, 40, 40, kNone) == LassoEvent::Ignored);
}

TEST_CASE("fewer than three points or zero area is discarded", "[lasso][gesture]") {
	LassoGesture gesture;
	REQUIRE(Press(gesture, 0, 0) == LassoEvent::Started);
	gesture.OnMotion({ 20, 0 }, kAlt, true);
	REQUIRE(gesture.OnAltReleased(kNone) == LassoEvent::Discarded);
	REQUIRE(gesture.Points().empty());

	REQUIRE(Press(gesture, 0, 0) == LassoEvent::Started);
	gesture.OnMotion({ 20, 20 }, kAlt, true);
	gesture.OnMotion({ 40, 40 }, kAlt, true);
	REQUIRE(gesture.Points().size() == 3);
	REQUIRE(gesture.OnAltReleased(kNone) == LassoEvent::Discarded); // collinear

	REQUIRE(Press(gesture, 0, 0) == LassoEvent::Started);
	REQUIRE(gesture.OnAltReleased(kNone) == LassoEvent::Discarded); // released before any polygon

	const LassoScreenPoint retraced[] = { { 0, 0 }, { 10, 0 }, { 0, 0 }, { 0, 10 } };
	REQUIRE_FALSE(HasLassoArea(retraced));
	const LassoScreenPoint triangle[] = { { 0, 0 }, { 10, 0 }, { 0, 10 } };
	REQUIRE(HasLassoArea(triangle));
}

TEST_CASE("Escape, right click and focus loss cancel", "[lasso][gesture]") {
	LassoGesture gesture;
	DragTriangle(gesture);
	REQUIRE(gesture.Cancel() == LassoEvent::Cancelled);
	REQUIRE_FALSE(gesture.IsActive());
	REQUIRE(gesture.Points().empty());
	REQUIRE(gesture.OnAltReleased(kNone) == LassoEvent::Ignored); // Alt release afterwards does nothing
	REQUIRE(Release(gesture, 40, 40, kNone) == LassoEvent::Consumed); // button was still down
	REQUIRE(gesture.Cancel() == LassoEvent::Ignored);

	DragTriangle(gesture);
	REQUIRE(gesture.OnButtonDown(LassoButton::Right, { 5, 5 }, kAlt, true, kSpacing) == LassoEvent::Cancelled);
	REQUIRE(gesture.OnButtonUp(LassoButton::Right, { 5, 5 }, kAlt) == LassoEvent::Consumed); // no popup menu
	REQUIRE(gesture.OnButtonUp(LassoButton::Right, { 5, 5 }, kAlt) == LassoEvent::Ignored);
}

TEST_CASE("lost Alt key-up cancels instead of committing", "[lasso][gesture]") {
	LassoGesture gesture;
	DragTriangle(gesture);
	REQUIRE(gesture.OnMotion({ 0, 40 }, kNone, true) == LassoEvent::Cancelled);
	REQUIRE_FALSE(gesture.IsActive());

	DragTriangle(gesture);
	REQUIRE(Release(gesture, 40, 40, kNone) == LassoEvent::Cancelled);

	DragTriangle(gesture);
	REQUIRE(Release(gesture, 40, 40) == LassoEvent::Consumed);
	REQUIRE(Press(gesture, 0, 40, kNone) == LassoEvent::Cancelled);
	REQUIRE(Release(gesture, 0, 40, kNone) == LassoEvent::Consumed);

	DragTriangle(gesture);
	REQUIRE(gesture.OnModifierKey(LassoModifiers { .shift = true }) == LassoEvent::Cancelled);
}

TEST_CASE("middle button is swallowed while active", "[lasso][gesture]") {
	LassoGesture gesture;
	DragTriangle(gesture);
	REQUIRE(gesture.OnButtonDown(LassoButton::Middle, { 1, 1 }, kAlt, true, kSpacing) == LassoEvent::Consumed);
	REQUIRE(gesture.IsActive());
	REQUIRE(gesture.OnAltReleased(kNone) == LassoEvent::Committed);
	REQUIRE(gesture.OnButtonUp(LassoButton::Middle, { 1, 1 }, kNone) == LassoEvent::Consumed);
}

TEST_CASE("point count is capped", "[lasso][gesture]") {
	LassoGesture gesture;
	REQUIRE(Press(gesture, 0, 0) == LassoEvent::Started);
	for (int i = 1; i < static_cast<int>(LassoGesture::kMaxPoints) + 50; ++i) {
		gesture.OnMotion({ i * kSpacing, (i % 2) * kSpacing }, kAlt, true);
	}
	REQUIRE(gesture.Points().size() == LassoGesture::kMaxPoints);
	REQUIRE(gesture.IsAtPointLimit());
	REQUIRE(gesture.OnAltReleased(kNone) == LassoEvent::Committed);
}

TEST_CASE("pending cursor can give the lasso its area", "[lasso][gesture]") {
	REQUIRE_FALSE(HasLassoAreaWithCursor(std::span<const LassoScreenPoint> {}, { 10, 10 }, true));

	const LassoScreenPoint one[] = { { 0, 0 } };
	REQUIRE_FALSE(HasLassoAreaWithCursor(one, { 10, 10 }, true));

	const LassoScreenPoint two[] = { { 0, 0 }, { 10, 0 } };
	REQUIRE(HasLassoAreaWithCursor(two, { 0, 10 }, true));
	REQUIRE_FALSE(HasLassoAreaWithCursor(two, { 20, 0 }, true)); // still collinear
	REQUIRE_FALSE(HasLassoAreaWithCursor(two, { 0, 10 }, false)); // cursor not appended

	const LassoScreenPoint collinear[] = { { 0, 0 }, { 10, 10 }, { 20, 20 } };
	REQUIRE(HasLassoAreaWithCursor(collinear, { 0, 20 }, true));
	REQUIRE_FALSE(HasLassoArea(collinear));
	REQUIRE_FALSE(HasLassoAreaWithCursor(collinear, { 0, 20 }, false));
}

TEST_CASE("handles exist only for click-vertex lassos", "[lasso][gesture]") {
	LassoGesture clicks;
	REQUIRE(Press(clicks, 0, 0) == LassoEvent::Started);
	REQUIRE(clicks.HandleVertices().size() == 1);
	REQUIRE(clicks.HandleVertices()[0] != 0);
	REQUIRE(Release(clicks, 0, 0) == LassoEvent::Consumed);
	REQUIRE(Press(clicks, 20, 0) == LassoEvent::Consumed);
	REQUIRE(clicks.HandleVertices().size() == 2);
	REQUIRE(clicks.HandleVertices()[1] != 0);

	LassoGesture drag;
	REQUIRE(Press(drag, 0, 0) == LassoEvent::Started);
	REQUIRE(drag.OnMotion({ 40, 0 }, kAlt, true) == LassoEvent::Consumed);
	REQUIRE(drag.Points().size() == 2);
	REQUIRE(drag.HandleVertices().empty());
}

TEST_CASE("status text names mode, point count and the Alt hint", "[lasso][gesture]") {
	REQUIRE(FormatLassoStatus(LassoMode::Replace, 5, false) == "[Replace] 5 points \xE2\x80\x94 Release Alt to apply");
	REQUIRE(FormatLassoStatus(LassoMode::Add, 1, false) == "[Add] 1 point \xE2\x80\x94 Release Alt to apply");
	REQUIRE(FormatLassoStatus(LassoMode::Subtract, 4096, true) == "[Sub] 4096 points (limit reached) \xE2\x80\x94 Release Alt to apply");
}
