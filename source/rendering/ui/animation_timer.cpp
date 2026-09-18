//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "rendering/ui/animation_timer.h"
#include "rendering/ui/map_display.h"

AnimationTimer::AnimationTimer(MapCanvas* canvas) :
	wxTimer(),
	map_canvas(canvas),
	started(false) {
		////
	};

AnimationTimer::~AnimationTimer() {
	////
};

void AnimationTimer::Notify() {
	map_canvas->RequestAnimationRepaint();
}

void AnimationTimer::Start() {
	if (!started) {
		started = true;
		constexpr int TARGET_FPS = 60;
		wxTimer::Start(1000 / TARGET_FPS);
	}
};

void AnimationTimer::Stop() {
	if (started) {
		started = false;
		wxTimer::Stop();
	}
};
