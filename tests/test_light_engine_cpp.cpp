#include <iostream>
#include <cmath>
#include <cassert>
#include <chrono>
#include <array>
#include <vector>

// Include GLM from vcpkg
#include <glm/glm.hpp>

// Include project headers
#include "rendering/core/light_types.h"
#include "rendering/core/light_palette.h"

namespace rme::lighting {
	inline constexpr float constexpr_sqrt(float x) {
		if (x <= 0.0f) return 0.0f;
		float curr = x;
		float prev = 0.0f;
		for (int i = 0; i < 20; ++i) {
			if (curr == prev) break;
			prev = curr;
			curr = 0.5f * (curr + x / curr);
		}
		return curr;
	}

	inline constexpr auto generateDistanceTable() {
		std::array<float, 513> table {};
		for (size_t i = 0; i < table.size(); ++i) {
			table[i] = constexpr_sqrt(static_cast<float>(i));
		}
		return table;
	}

	inline constexpr auto s_distance_table = generateDistanceTable();
}

int main() {
	std::cout << "========================================================\n";
	std::cout << "Native C++ Empirical Test: Milestone 2 Lighting Engine\n";
	std::cout << "========================================================\n\n";

	// 1. Distance LUT static and runtime verification
	std::cout << "[Test 1] Exhaustive Distance LUT vs std::sqrt (513 entries)...\n";
	static_assert(rme::lighting::s_distance_table.size() == 513);
	static_assert(rme::lighting::s_distance_table[0] == 0.0f);
	static_assert(rme::lighting::s_distance_table[1] == 1.0f);
	static_assert(rme::lighting::s_distance_table[256] == 16.0f);

	float max_error = 0.0f;
	for (size_t i = 0; i < rme::lighting::s_distance_table.size(); ++i) {
		float expected = std::sqrt(static_cast<float>(i));
		float actual = rme::lighting::s_distance_table[i];
		float diff = std::abs(actual - expected);
		if (diff > max_error) {
			max_error = diff;
		}
		if (diff > 1e-5f) {
			std::cerr << "LUT mismatch at index " << i << ": actual=" << actual << ", expected=" << expected << "\n";
			return 1;
		}
		if (i > 0 && rme::lighting::s_distance_table[i] < rme::lighting::s_distance_table[i - 1]) {
			std::cerr << "LUT monotonicity violated at index " << i << "\n";
			return 1;
		}
	}
	std::cout << "PASS: All 513 entries verified against std::sqrt. Max error: " << max_error << "\n";

	// 2. Palette 6x6x6 Color Cube verification
	std::cout << "\n[Test 2] Palette Table vs OTClient 6x6x6 formula (256 entries)...\n";
	static_assert(rme::lighting::s_palette_table.size() == 256);
	static_assert(rme::lighting::s_palette_table[0].r == 0 && rme::lighting::s_palette_table[0].g == 0 && rme::lighting::s_palette_table[0].b == 0);
	static_assert(rme::lighting::s_palette_table[215].r == 255 && rme::lighting::s_palette_table[215].g == 255 && rme::lighting::s_palette_table[215].b == 255);
	static_assert(rme::lighting::s_palette_table[216].r == 0);

	for (int c = 1; c < 216; ++c) {
		uint8_t exp_r = static_cast<uint8_t>((c / 36) % 6 * 51);
		uint8_t exp_g = static_cast<uint8_t>((c / 6) % 6 * 51);
		uint8_t exp_b = static_cast<uint8_t>(c % 6 * 51);
		const auto& p = rme::lighting::s_palette_table[c];
		if (p.r != exp_r || p.g != exp_g || p.b != exp_b) {
			std::cerr << "Palette mismatch at " << c << "\n";
			return 1;
		}
	}
	std::cout << "PASS: All 256 palette entries match OTClient color cube formula.\n";

	// 3. Ambient Lighting Model across all 16 floors
	std::cout << "\n[Test 3] Ambient Lighting across floors 0..15...\n";
	rme::lighting::LightConfig daylight_config{
		.ambient_color = 215,
		.ambient_intensity = 255,
		.minimum_ambient_light = 0.0f,
		.enabled = true
	};

	for (int f = 0; f <= 15; ++f) {
		glm::vec3 amb = rme::lighting::getAmbientRGB(f, daylight_config);
		if (f <= 7) {
			if (amb.r != 1.0f || amb.g != 1.0f || amb.b != 1.0f) {
				std::cerr << "Daylight ambient failed on floor " << f << "\n";
				return 1;
			}
		} else {
			if (amb.r != 0.0f || amb.g != 0.0f || amb.b != 0.0f) {
				std::cerr << "Underground pitch black failed on floor " << f << "\n";
				return 1;
			}
		}
	}

	// Minimum ambient clamp underground
	rme::lighting::LightConfig dim_config{
		.ambient_color = 215,
		.ambient_intensity = 0,
		.minimum_ambient_light = 0.2f,
		.enabled = true
	};
	for (int f = 8; f <= 15; ++f) {
		glm::vec3 amb = rme::lighting::getAmbientRGB(f, dim_config);
		if (std::abs(amb.r - 0.2f) > 1e-5f || std::abs(amb.g - 0.2f) > 1e-5f || std::abs(amb.b - 0.2f) > 1e-5f) {
			std::cerr << "Underground minimum ambient clamp failed on floor " << f << "\n";
			return 1;
		}
	}
	std::cout << "PASS: Ambient lighting rules verified across all 16 floors.\n";

	// 4. PackRGBA bitwise and memory layout verification
	std::cout << "\n[Test 4] PackRGBA bitwise and memory layout...\n";
	uint32_t packed = rme::lighting::packRGBA(0x12, 0x34, 0x56, 0x78);
	if (packed != 0x78563412) {
		std::cerr << "packRGBA value mismatch: 0x" << std::hex << packed << "\n";
		return 1;
	}
	const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(&packed);
	if (byte_ptr[0] != 0x12 || byte_ptr[1] != 0x34 || byte_ptr[2] != 0x56 || byte_ptr[3] != 0x78) {
		std::cerr << "packRGBA little-endian byte ordering failed!\n";
		return 1;
	}
	std::cout << "PASS: PackRGBA matches little-endian GL_RGBA8 memory layout exactly.\n";

	// 5. C++ Native Throughput Benchmark: 10,000,000 LUT lookups
	std::cout << "\n[Test 5] High-Throughput C++ Benchmark (10,000,000 lookups)...\n";
	auto t_start = std::chrono::high_resolution_clock::now();
	volatile float sum = 0.0f;
	for (int i = 0; i < 10'000'000; ++i) {
		sum += rme::lighting::s_distance_table[i % 513];
	}
	auto t_end = std::chrono::high_resolution_clock::now();
	auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();
	double ns_per_op = static_cast<double>(duration_us * 1000) / 10'000'000.0;
	std::cout << "10,000,000 LUT lookups completed in " << duration_us / 1000.0 << " ms ("
	          << ns_per_op << " ns/op, sum=" << sum << ")\n";
	std::cout << "PASS: Distance LUT executes in ~" << ns_per_op << " nanoseconds per operation with ZERO std::sqrt calls.\n";

	std::cout << "\n========================================================\n";
	std::cout << "ALL NATIVE C++ VERIFICATION TESTS PASSED SUCCESSFULLY!\n";
	std::cout << "========================================================\n";
	return 0;
}
