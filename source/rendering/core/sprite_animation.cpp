//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/sprite_animation.h"
#include <algorithm>
#include <cstdlib>

namespace rme::rendering {

namespace {

[[nodiscard]] constexpr int euclideanModulo(int64_t val, int mod) noexcept {
	if (mod <= 0) {
		return 0;
	}
	const int64_t rem = val % mod;
	return static_cast<int>(rem >= 0 ? rem : rem + mod);
}

} // namespace

int SpriteAnimation::getFrame(
	const SpriteAnimationData& data,
	int64_t elapsed_ms,
	int tile_x,
	int tile_y,
	int tile_z
) noexcept {
	const int frames = std::max<int>(1, static_cast<int>(data.frame_count));
	if (frames <= 1) {
		return 0;
	}

	if (elapsed_ms < 0) {
		elapsed_ms = 0;
	}

	const int tile_offset = data.is_async ? std::abs(tile_x * 17 + tile_y * 31 + tile_z * 7) : 0;

	// Explicit start phase (0..frames-1).
	// When start_frame is the random sentinel (255 or -1), async spatial phase is applied via
	// timeline offset (tile_offset), so start_phase stays 0 to prevent applying the spatial offset twice.
	const bool is_random_sentinel = (data.start_frame == -1 || data.start_frame == 255);
	const int start_phase = (!is_random_sentinel && data.start_frame > 0) ? (static_cast<int>(data.start_frame) % frames) : 0;

	// Ping-pong mode (loop_count == -1)
	if (data.loop_count == -1) {
		if (data.durations.empty()) {
			const int64_t tick = elapsed_ms / 500;
			const int64_t phase_shift = data.is_async ? tile_offset : start_phase;
			const int cycle = 2 * frames - 2;
			const int tick_in_cycle = cycle > 0 ? euclideanModulo(tick + phase_shift, cycle) : 0;
			return getPingPongPhase(tick_in_cycle, frames);
		}

		const int num_phases = static_cast<int>(data.durations.size());
		if (num_phases <= 1) {
			return 0;
		}

		// Calculate total cycle duration: forward sequence (0..num_phases-1) + backward (num_phases-2..1)
		// data.total_duration_ms is the sum of all num_phases forward.
		int64_t cycle_length = data.total_duration_ms;
		for (int i = num_phases - 2; i >= 1; --i) {
			cycle_length += data.durations[i].getDuration();
		}

		if (cycle_length <= 0) {
			return 0;
		}

		const int64_t time_offset = data.is_async ? static_cast<int64_t>(tile_offset) * 500 : 0;
		int elapsed = static_cast<int>((elapsed_ms + time_offset) % cycle_length);

		// Forward phase: 0 -> num_phases - 1
		for (int i = 0; i < num_phases; ++i) {
			const int dur = static_cast<int>(data.durations[i].getDuration());
			if (elapsed < dur) {
				return i % frames;
			}
			elapsed -= dur;
		}

		// Backward phase: num_phases - 2 down to 1
		for (int i = num_phases - 2; i >= 1; --i) {
			const int dur = static_cast<int>(data.durations[i].getDuration());
			if (elapsed < dur) {
				return i % frames;
			}
			elapsed -= dur;
		}

		return 0;
	}

	// Forward loop
	if (data.durations.empty()) {
		const int64_t tick = elapsed_ms / 500;
		if (data.loop_count > 0 && tick >= static_cast<int64_t>(data.loop_count) * frames) {
			return frames - 1;
		}
		const int64_t phase_shift = data.is_async ? tile_offset : start_phase;
		return euclideanModulo(tick + phase_shift, frames);
	}

	const int total = static_cast<int>(data.total_duration_ms);
	if (total <= 0) {
		return start_phase;
	}

	// Finite loop termination: hold terminal frame once all cycles complete
	if (data.loop_count > 0 && elapsed_ms >= static_cast<int64_t>(data.loop_count) * total) {
		return frames - 1;
	}

	const int64_t time_offset = data.is_async ? static_cast<int64_t>(tile_offset) * 500 : 0;
	int elapsed = static_cast<int>((elapsed_ms + time_offset) % total);

	const int num_phases = static_cast<int>(data.durations.size());
	for (int i = 0; i < num_phases; ++i) {
		const int phase = (start_phase + i) % num_phases;
		const int dur = static_cast<int>(data.durations[phase].getDuration());
		if (elapsed < dur) {
			return phase % frames;
		}
		elapsed -= dur;
	}

	return start_phase;
}

} // namespace rme::rendering
