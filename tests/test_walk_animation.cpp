#include "rendering/core/walk_animation.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <vector>

using WalkAnimation::Frame;
using WalkAnimation::IsWalking;

namespace {
	// Frame shown at each quarter step over `steps` consecutive steps.
	std::vector<int> FramesPerQuarterStep(int frames, bool walking_group, int steps) {
		std::vector<int> shown;
		for (int quarter = 0; quarter < steps * 4; ++quarter) {
			shown.push_back(Frame(frames, (static_cast<float>(quarter) + 0.5f) / 4.0f, walking_group));
		}
		return shown;
	}

	std::set<int> FramesOverSteps(int frames, bool walking_group, int steps) {
		std::set<int> shown;
		for (int ms = 0; ms < steps * 1000; ++ms) {
			shown.insert(Frame(frames, static_cast<float>(ms) / 1000.0f, walking_group));
		}
		return shown;
	}
} // namespace

TEST_CASE("standing creatures draw frame 0", "[walk]") {
	REQUIRE_FALSE(IsWalking(-1.0f));
	REQUIRE(IsWalking(0.0f));
	REQUIRE(Frame(3, -1.0f, false) == 0);
	REQUIRE(Frame(8, -1.0f, true) == 0);
}

TEST_CASE("classic outfits keep the previous preview frames", "[walk]") {
	// Previous preview: phase = 1 + int(progress * 4) % 2 within a step, drawn as phase % frames.
	for (int ms = 0; ms < 3000; ++ms) {
		const float walk_progress = static_cast<float>(ms) / 1000.0f;
		const float step_progress = static_cast<float>(ms % 1000) / 1000.0f;
		const int phase = 1 + static_cast<int>(step_progress * 4.0f) % 2;
		REQUIRE(Frame(3, walk_progress, false) == phase % 3);
		REQUIRE(Frame(2, walk_progress, false) == phase % 2);
	}
	REQUIRE(FramesOverSteps(3, false, 1) == std::set<int> { 1, 2 });
}

TEST_CASE("walking frame groups cycle through every frame at the classic pace", "[walk]") {
	REQUIRE(FramesPerQuarterStep(8, true, 2) == std::vector<int> { 0, 1, 2, 3, 4, 5, 6, 7 });
	REQUIRE(FramesPerQuarterStep(8, true, 3) == std::vector<int> { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3 });
	REQUIRE(FramesOverSteps(8, true, 2) == std::set<int> { 0, 1, 2, 3, 4, 5, 6, 7 });
}

TEST_CASE("long single-group walk cycles skip only the standing frame", "[walk]") {
	REQUIRE(FramesOverSteps(8, false, 2) == std::set<int> { 1, 2, 3, 4, 5, 6, 7 });
	REQUIRE(FramesPerQuarterStep(5, false, 1) == std::vector<int> { 1, 2, 3, 4 });
}

TEST_CASE("sprites without walk frames stay on frame 0", "[walk]") {
	REQUIRE(FramesOverSteps(1, false, 2) == std::set<int> { 0 });
	REQUIRE(FramesOverSteps(1, true, 2) == std::set<int> { 0 });
	REQUIRE(FramesOverSteps(0, true, 2) == std::set<int> { 0 });
}
