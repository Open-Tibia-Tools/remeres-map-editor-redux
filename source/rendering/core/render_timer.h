//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_RENDER_TIMER_H_
#define RME_RENDERING_CORE_RENDER_TIMER_H_

#include <chrono>

class RenderTimer {
public:
	RenderTimer();
	~RenderTimer() = default;

	void Start();
	void Pause();
	void Resume();
	long getElapsedTime() const;

private:
	std::chrono::steady_clock::time_point start_time_{};
	std::chrono::steady_clock::duration accumulated_duration_{std::chrono::steady_clock::duration::zero()};
	bool is_paused = false;
};

#endif
