#ifndef RME_RENDERING_CORE_WALK_ANIMATION_H_
#define RME_RENDERING_CORE_WALK_ANIMATION_H_

namespace WalkAnimation {

	// Walk frames advance this many times per step (the classic two walk frames played twice).
	inline constexpr int kFrameChangesPerStep = 4;

	// walk_progress: steps completed since the creature started moving plus the progress through
	// the current step in [0, 1], so the walk cycle continues across consecutive steps.
	// Negative while standing.
	[[nodiscard]] constexpr bool IsWalking(float walk_progress) noexcept {
		return walk_progress >= 0.0f;
	}

	// Frame of a walking outfit sprite with `frames` frames.
	// walking_group: the sprite is a separate walking frame group (clients with frame groups),
	// so every frame is a walk frame. Otherwise frame 0 is the standing frame and the walk
	// starts at frame 1; a two-frame sprite alternates 1 and 0.
	[[nodiscard]] constexpr int Frame(int frames, float walk_progress, bool walking_group) noexcept {
		if (!IsWalking(walk_progress) || frames <= 1) {
			return 0;
		}
		const int tick = static_cast<int>(walk_progress * static_cast<float>(kFrameChangesPerStep));
		if (walking_group) {
			return tick % frames;
		}
		if (frames == 2) {
			return (1 + tick) % 2;
		}
		return 1 + tick % (frames - 1);
	}

} // namespace WalkAnimation

#endif
