//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/render_timer.h"

RenderTimer::RenderTimer() {
	Start();
}

void RenderTimer::Start() {
	start_time_ = std::chrono::steady_clock::now();
	accumulated_duration_ = std::chrono::steady_clock::duration::zero();
	is_paused = false;
}

void RenderTimer::Pause() {
	if (!is_paused) {
		accumulated_duration_ += (std::chrono::steady_clock::now() - start_time_);
		is_paused = true;
	}
}

void RenderTimer::Resume() {
	if (is_paused) {
		start_time_ = std::chrono::steady_clock::now();
		is_paused = false;
	}
}

long RenderTimer::getElapsedTime() const {
	if (is_paused) {
		return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(accumulated_duration_).count());
	}
	const auto elapsed = accumulated_duration_ + (std::chrono::steady_clock::now() - start_time_);
	return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}
