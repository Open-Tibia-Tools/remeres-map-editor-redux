//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_SPRITE_ANIMATION_H_
#define RME_RENDERING_CORE_SPRITE_ANIMATION_H_

#include <algorithm>
#include <cstdint>
#include <vector>

namespace rme::rendering {

enum ItemAnimationDuration : uint16_t {
	ITEM_FRAME_DURATION = 500
};

struct AnimationDuration {
	uint32_t min_ms = 500;
	uint32_t max_ms = 500;

	[[nodiscard]] constexpr uint32_t getDuration() const noexcept {
		return (min_ms + max_ms) / 2;
	}
};

struct SpriteAnimationData {
	uint16_t frame_count = 0;
	int16_t start_frame = 0;         // 0..frames-1, or 255 (-1) for async random
	int8_t loop_count = 0;           // 0 = infinite, -1 = ping-pong, >0 = count
	bool is_async = false;
	uint32_t shortest_step_ms = 500; // Precalculated at load time (O(1) lookup)
	uint32_t total_duration_ms = 0;  // Sum of average durations
	std::vector<AnimationDuration> durations; // Empty if uniform 500ms
};

class SpriteAnimation {
public:
	// Pure, stateless mathematical frame resolver.
	// Thread-safe, lock-free, zero heap allocation.
	[[nodiscard]] static int getFrame(
		const SpriteAnimationData& data,
		int64_t elapsed_ms,
		int tile_x = 0,
		int tile_y = 0,
		int tile_z = 0
	) noexcept;

	// Fast ping-pong index reflection: 0, 1, ..., N-1, N-2, ..., 1
	[[nodiscard]] static constexpr int getPingPongPhase(int raw_phase, int frames) noexcept {
		if (frames <= 1) {
			return 0;
		}
		const int cycle = frames * 2 - 2;
		const int mod = (raw_phase >= 0) ? (raw_phase % cycle) : ((raw_phase % cycle + cycle) % cycle);
		return (mod >= frames) ? (cycle - mod) : mod;
	}
};

} // namespace rme::rendering

#endif // RME_RENDERING_CORE_SPRITE_ANIMATION_H_
