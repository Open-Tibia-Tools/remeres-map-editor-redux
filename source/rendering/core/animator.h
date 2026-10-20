//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#ifndef RME_ANIMATOR_H_
#define RME_ANIMATOR_H_

#include <vector>
#include <algorithm>
#include "util/common.h"

enum AnimationDirection {
	ANIMATION_FORWARD = 0,
	ANIMATION_BACKWARD = 1
};

enum ItemAnimationDuration {
	ITEM_FRAME_DURATION = 500
};

struct FrameDuration {
	int min;
	int max;

	FrameDuration(int min, int max) :
		min(min), max(max) {
		ASSERT(min <= max);
	}

	int getDuration() const {
		if (min == max) {
			return min;
		}
		return uniform_random(min, max);
	};

	void setValues(int min, int max) {
		ASSERT(min <= max);
		this->min = min;
		this->max = max;
	}
};

class Animator {
public:
	Animator(int frames, int start_frame, int loop_count, bool async);
	~Animator();

	int getStartFrame() const;

	FrameDuration* getFrameDuration(int frame);

	int getFrame(long elapsed_time = -1);
	void setFrame(int frame);

	// Shortest time any frame of this animation can be on screen, i.e. the repaint cadence at
	// which no animation frame can be skipped. Zero when the animation has no usable duration.
	[[nodiscard]] int getFrameStepMs() const {
		if (frame_count <= 0) {
			return 0;
		}
		// A frame is shown for uniform_random(min, max) ms, so pacing at the mean duration can
		// land every repaint outside a shorter frame's window and skip it forever (phase offset
		// between the wall clock and the animation cycle). Pacing at the shortest minimum shows
		// every frame; the 60 Hz floor keeps a pathological tiny duration from spinning the
		// repaint loop far above its useful rate.
		int shortest = 0;
		for (const auto& entry : durations) {
			// A zero minimum means the frame can roll a near-instant display, so it still
			// needs fast repaints; its maximum is the best available estimate for it.
			const int candidate = entry.min > 0 ? entry.min : entry.max;
			if (candidate > 0 && (shortest == 0 || candidate < shortest)) {
				shortest = candidate;
			}
		}
		if (shortest == 0) {
			// No positive minima available: fall back to the running mean.
			int total = total_duration;
			if (total <= 0) {
				for (const auto& entry : durations) {
					total += entry.max;
				}
			}
			return total > 0 ? std::max(1, total / frame_count) : 0;
		}
		return std::max(16, shortest);
	}

	void reset();

private:
	int getDuration(int frame) const;
	int getPingPongFrame();
	int getLoopFrame();
	void calculateSynchronous(long current_time = -1);

	int frame_count;
	int start_frame;
	int loop_count;
	bool async;
	std::vector<FrameDuration> durations;
	int current_frame;
	int current_loop;
	int current_duration;
	int total_duration;
	AnimationDirection direction;
	long last_time;
	bool is_complete;
};

#endif
