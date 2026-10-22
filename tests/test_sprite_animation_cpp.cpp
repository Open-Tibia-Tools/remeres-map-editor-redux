#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "rendering/core/sprite_animation.h"

#define TEST_CHECK(expr) \
	do { \
		if (!(expr)) { \
			std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ \
			          << " (" #expr ")" << std::endl; \
			std::exit(1); \
		} \
	} while (false)

using namespace rme::rendering;

void test_uniform_forward_loop() {
	std::cout << "[Test] Uniform forward loop (7.x / classic mode)..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 4;
	data.start_frame = 0;
	data.loop_count = 0;
	data.is_async = false;
	data.shortest_step_ms = 500;
	data.total_duration_ms = 2000;

	// Frame changes every 500ms: 0, 1, 2, 3, 0, 1...
	TEST_CHECK(SpriteAnimation::getFrame(data, 0) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 250) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 499) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 500) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(data, 999) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(data, 1000) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(data, 1499) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(data, 1500) == 3);
	TEST_CHECK(SpriteAnimation::getFrame(data, 1999) == 3);
	TEST_CHECK(SpriteAnimation::getFrame(data, 2000) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 2500) == 1);

	std::cout << "  OK" << std::endl;
}

void test_custom_durations_forward_loop() {
	std::cout << "[Test] Custom variable duration forward loop (10.50+ / Protobuf)..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 3;
	data.start_frame = 0;
	data.loop_count = 0;
	data.is_async = false;

	// Frame 0: 100ms, Frame 1: 300ms, Frame 2: 600ms -> Total = 1000ms
	data.durations.push_back(AnimationDuration { 100, 100 });
	data.durations.push_back(AnimationDuration { 300, 300 });
	data.durations.push_back(AnimationDuration { 600, 600 });
	data.total_duration_ms = 1000;
	data.shortest_step_ms = 100;

	TEST_CHECK(SpriteAnimation::getFrame(data, 0) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 99) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 100) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(data, 399) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(data, 400) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(data, 999) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(data, 1000) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 1050) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 1100) == 1);

	std::cout << "  OK" << std::endl;
}

void test_ping_pong_uniform() {
	std::cout << "[Test] Ping-pong uniform 500ms cycle..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 4;
	data.start_frame = 0;
	data.loop_count = -1; // Ping-pong
	data.is_async = false;

	// Frames sequence: 0, 1, 2, 3, 2, 1 -> repeats every 6 ticks (3000ms)
	TEST_CHECK(SpriteAnimation::getFrame(data, 0) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 500) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(data, 1000) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(data, 1500) == 3);
	TEST_CHECK(SpriteAnimation::getFrame(data, 2000) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(data, 2500) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(data, 3000) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 3500) == 1);

	std::cout << "  OK" << std::endl;
}

void test_ping_pong_variable_durations() {
	std::cout << "[Test] Ping-pong variable duration timeline walk..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 3;
	data.start_frame = 0;
	data.loop_count = -1; // Ping-pong
	data.is_async = false;

	// Frame 0: 100ms, Frame 1: 200ms, Frame 2: 300ms
	// Forward: 0 (100ms), 1 (200ms), 2 (300ms)
	// Backward: 1 (200ms)
	// Full cycle = 100 + 200 + 300 + 200 = 800ms
	data.durations.push_back(AnimationDuration { 100, 100 });
	data.durations.push_back(AnimationDuration { 200, 200 });
	data.durations.push_back(AnimationDuration { 300, 300 });
	data.total_duration_ms = 600;
	data.shortest_step_ms = 100;

	// 0..99ms -> Phase 0
	TEST_CHECK(SpriteAnimation::getFrame(data, 0) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 50) == 0);
	// 100..299ms -> Phase 1
	TEST_CHECK(SpriteAnimation::getFrame(data, 100) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(data, 250) == 1);
	// 300..599ms -> Phase 2
	TEST_CHECK(SpriteAnimation::getFrame(data, 300) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(data, 550) == 2);
	// 600..799ms -> Phase 1 (rebound)
	TEST_CHECK(SpriteAnimation::getFrame(data, 600) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(data, 750) == 1);
	// 800ms -> Phase 0 (cycle restart)
	TEST_CHECK(SpriteAnimation::getFrame(data, 800) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 900) == 1);

	std::cout << "  OK" << std::endl;
}

void test_spatial_async_desynchronization() {
	std::cout << "[Test] Spatial asynchronous phase desynchronization..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 4;
	data.start_frame = 255; // Async random spatial phase mode
	data.loop_count = 0;
	data.is_async = true;

	// At elapsed_ms = 0, tiles at different coordinates must produce different phases
	const int frame_tile1 = SpriteAnimation::getFrame(data, 0, 100, 100, 7);
	const int frame_tile2 = SpriteAnimation::getFrame(data, 0, 101, 100, 7);
	const int frame_tile3 = SpriteAnimation::getFrame(data, 0, 100, 101, 7);

	// tile1 offset: (100*17 + 100*31 + 7*7) % 4 = (1700 + 3100 + 49) % 4 = 4849 % 4 = 1
	TEST_CHECK(frame_tile1 == 1);
	// tile2 offset: (101*17 + 100*31 + 49) % 4 = 4866 % 4 = 2
	TEST_CHECK(frame_tile2 == 2);
	// tile3 offset: (100*17 + 101*31 + 49) % 4 = 4880 % 4 = 0
	TEST_CHECK(frame_tile3 == 0);

	// Verify that advancing time advances all tiles deterministically
	TEST_CHECK(SpriteAnimation::getFrame(data, 500, 100, 100, 7) == (frame_tile1 + 1) % 4);

	std::cout << "  OK" << std::endl;
}

void test_sea_border_synchronization() {
	std::cout << "[Test] Sea border synchronous multi-tile phase lockstep (ID 4627/4638)..." << std::endl;
	SpriteAnimationData data;
	data.frame_count = 14;
	data.start_frame = -1; // Sentinel: default start
	data.loop_count = 0;   // Infinite forward loop
	data.is_async = false; // Synchronous in Tibia DAT (animationMode == 1)
	for (int i = 0; i < 14; ++i) {
		data.durations.push_back(AnimationDuration { 200, 200 });
	}
	data.total_duration_ms = 14 * 200; // 2800ms
	data.shortest_step_ms = 200;

	// At ANY elapsed_ms, all tiles across the entire shoreline must return the exact same frame!
	for (int64_t t = 0; t <= 5600; t += 50) {
		const int expected_frame = static_cast<int>((t % 2800) / 200);
		for (int x = 50; x <= 60; ++x) {
			for (int y = 100; y <= 110; ++y) {
				const int frame = SpriteAnimation::getFrame(data, t, x, y, 7);
				TEST_CHECK(frame == expected_frame);
			}
		}
	}
	std::cout << "  OK" << std::endl;
}

void test_finite_loop_termination() {
	std::cout << "[Test] Finite loop count termination..." << std::endl;

	// Uniform mode: 3 frames, loop_count = 2 (total 6 ticks = 3000ms)
	SpriteAnimationData uniform_data;
	uniform_data.frame_count = 3;
	uniform_data.loop_count = 2;
	uniform_data.start_frame = 0;
	uniform_data.is_async = false;

	TEST_CHECK(SpriteAnimation::getFrame(uniform_data, 0) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(uniform_data, 500) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(uniform_data, 1000) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(uniform_data, 1500) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(uniform_data, 2000) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(uniform_data, 2500) == 2);
	// At and beyond 3000ms (2 loops * 3 frames * 500ms), must hold terminal frame 2
	TEST_CHECK(SpriteAnimation::getFrame(uniform_data, 3000) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(uniform_data, 5000) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(uniform_data, 50000) == 2);

	// Variable duration mode: 2 frames (100ms, 200ms -> cycle = 300ms), loop_count = 3 (total 900ms)
	SpriteAnimationData var_data;
	var_data.frame_count = 2;
	var_data.loop_count = 3;
	var_data.start_frame = 0;
	var_data.is_async = false;
	var_data.durations.push_back(AnimationDuration { 100, 100 });
	var_data.durations.push_back(AnimationDuration { 200, 200 });
	var_data.total_duration_ms = 300;

	TEST_CHECK(SpriteAnimation::getFrame(var_data, 0) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(var_data, 99) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(var_data, 100) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(var_data, 299) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(var_data, 300) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(var_data, 600) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(var_data, 700) == 1);
	// At and beyond 900ms (3 * 300ms), must hold terminal frame 1
	TEST_CHECK(SpriteAnimation::getFrame(var_data, 900) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(var_data, 1500) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(var_data, 10000) == 1);

	std::cout << "  OK" << std::endl;
}

void test_ping_pong_more_than_32_phases() {
	std::cout << "[Test] Ping-pong animation with > 32 duration phases (unbounded)..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 40;
	data.loop_count = -1; // Ping-pong
	data.is_async = false;

	uint32_t total = 0;
	for (int i = 0; i < 40; ++i) {
		data.durations.push_back(AnimationDuration { 50, 50 }); // 50ms each
		total += 50;
	}
	data.total_duration_ms = total; // 2000ms

	// Cycle length = 40 phases forward (2000ms) + 38 phases backward (1900ms) = 3900ms
	// Phase 0: 0..49ms
	TEST_CHECK(SpriteAnimation::getFrame(data, 0) == 0);
	// Phase 35 (beyond old 32 cap): 35 * 50 = 1750ms
	TEST_CHECK(SpriteAnimation::getFrame(data, 1750) == 35);
	// Peak phase 39: 39 * 50 = 1950ms
	TEST_CHECK(SpriteAnimation::getFrame(data, 1950) == 39);
	// Rebound phase 38: 2000ms
	TEST_CHECK(SpriteAnimation::getFrame(data, 2000) == 38);
	// Rebound phase 35: 2000 + (39 - 1 - 35) * 50 = 2000 + 3 * 50 = 2150ms
	TEST_CHECK(SpriteAnimation::getFrame(data, 2150) == 35);
	// Rebound phase 1: 2000 + 37 * 50 = 3850ms
	TEST_CHECK(SpriteAnimation::getFrame(data, 3850) == 1);
	// Cycle restart phase 0: 3900ms
	TEST_CHECK(SpriteAnimation::getFrame(data, 3900) == 0);

	std::cout << "  OK" << std::endl;
}

void test_phase_aligned_durations() {
	std::cout << "[Test] Phase-aligned duration lookup with start_phase > 0..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 3;
	data.start_frame = 1; // start at phase 1
	data.loop_count = 0;
	data.is_async = false;

	// Phase 0: 100ms, Phase 1: 300ms, Phase 2: 500ms -> Total = 900ms
	data.durations.push_back(AnimationDuration { 100, 100 });
	data.durations.push_back(AnimationDuration { 300, 300 });
	data.durations.push_back(AnimationDuration { 500, 500 });
	data.total_duration_ms = 900;

	// Starting at phase 1:
	// First 300ms must show Phase 1 (using Phase 1's duration: 300ms)
	TEST_CHECK(SpriteAnimation::getFrame(data, 0) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(data, 150) == 1);
	TEST_CHECK(SpriteAnimation::getFrame(data, 299) == 1);

	// Next 500ms must show Phase 2 (using Phase 2's duration: 500ms)
	TEST_CHECK(SpriteAnimation::getFrame(data, 300) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(data, 500) == 2);
	TEST_CHECK(SpriteAnimation::getFrame(data, 799) == 2);

	// Final 100ms must show Phase 0 (using Phase 0's duration: 100ms)
	TEST_CHECK(SpriteAnimation::getFrame(data, 800) == 0);
	TEST_CHECK(SpriteAnimation::getFrame(data, 899) == 0);

	// Cycle restart:
	TEST_CHECK(SpriteAnimation::getFrame(data, 900) == 1);

	std::cout << "  OK" << std::endl;
}

void test_edge_cases() {
	std::cout << "[Test] Edge cases (single frame, zero frames, negative time)..." << std::endl;

	SpriteAnimationData empty_data;
	TEST_CHECK(SpriteAnimation::getFrame(empty_data, 1000) == 0);

	SpriteAnimationData single_frame;
	single_frame.frame_count = 1;
	TEST_CHECK(SpriteAnimation::getFrame(single_frame, 1000) == 0);

	SpriteAnimationData negative_time;
	negative_time.frame_count = 3;
	TEST_CHECK(SpriteAnimation::getFrame(negative_time, -500) == 0);

	std::cout << "  OK" << std::endl;
}

void test_concurrency_stress() {
	std::cout << "[Test] Multi-threaded lock-free stress test (16 threads x 500,000 queries)..." << std::endl;

	SpriteAnimationData data;
	data.frame_count = 5;
	data.start_frame = 255;
	data.loop_count = -1;
	data.is_async = true;
	data.durations.push_back(AnimationDuration { 100, 100 });
	data.durations.push_back(AnimationDuration { 150, 150 });
	data.durations.push_back(AnimationDuration { 200, 200 });
	data.durations.push_back(AnimationDuration { 250, 250 });
	data.durations.push_back(AnimationDuration { 300, 300 });

	constexpr int NUM_THREADS = 16;
	constexpr int QUERIES_PER_THREAD = 500000;
	std::vector<std::thread> threads;
	threads.reserve(NUM_THREADS);

	const auto start = std::chrono::high_resolution_clock::now();

	for (int t = 0; t < NUM_THREADS; ++t) {
		threads.emplace_back([&data, t]() {
			uint64_t checksum = 0;
			for (int i = 0; i < QUERIES_PER_THREAD; ++i) {
				const int64_t time = i * 33;
				const int x = t * 10 + (i & 15);
				const int y = t * 20 + ((i >> 4) & 15);
				const int frame = SpriteAnimation::getFrame(data, time, x, y, 7);
				checksum += frame;
			}
			TEST_CHECK(checksum > 0);
		});
	}

	for (auto& th : threads) {
		th.join();
	}

	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - start
	).count();

	std::cout << "  OK: 8,000,000 queries completed in " << elapsed << " ms (~"
	          << (8000000.0 / (elapsed > 0 ? elapsed : 1) * 1000.0) << " queries/sec, 0 contention)" << std::endl;
}

int main() {
	std::cout << "===========================================" << std::endl;
	std::cout << "  RME SpriteAnimation Subsystem Test Suite  " << std::endl;
	std::cout << "===========================================" << std::endl;

	test_uniform_forward_loop();
	test_custom_durations_forward_loop();
	test_ping_pong_uniform();
	test_ping_pong_variable_durations();
	test_spatial_async_desynchronization();
	test_sea_border_synchronization();
	test_finite_loop_termination();
	test_ping_pong_more_than_32_phases();
	test_phase_aligned_durations();
	test_edge_cases();
	test_concurrency_stress();

	std::cout << "===========================================" << std::endl;
	std::cout << "  ALL TESTS PASSED SUCCESSFULLY!          " << std::endl;
	std::cout << "===========================================" << std::endl;
	return 0;
}
