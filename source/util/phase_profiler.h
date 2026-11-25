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

#ifndef RME_UTIL_PHASE_PROFILER_H_
#define RME_UTIL_PHASE_PROFILER_H_

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <numeric>
#include <string>
#include <string_view>
#include <spdlog/spdlog.h>

namespace rme::prof {

	// A tiny in-process phase profiler. Scopes accumulate milliseconds per named phase and a
	// ranked, per-frame breakdown is logged every kLogIntervalFrames frames.
	//
	// Off by default and off unless RME_PROFILE is set in the environment, so the shipped
	// build pays one relaxed atomic load plus a predictable branch per scope.
	enum class Phase : uint8_t {
		Frame,
		ChunkQuery,
		ChunkBake,
		TerrainDraw,
		ItemDraw,
		DynamicOverlay,
		ZoneOverlay,
		BlockingOverlay,
		HouseOverlay,
		HighlightOverlay,
		BadgeOverlay,
		MarkerLabels,
		NanoVg,
		AnimPatch,
		AtlasApply,
		Present,
		Count,
	};

	inline constexpr size_t kPhaseCount = static_cast<size_t>(Phase::Count);
	inline constexpr uint32_t kLogIntervalFrames = 120;

	inline constexpr std::string_view kPhaseNames[kPhaseCount] = {
		"frame",
		"chunk.query",
		"chunk.bake",
		"chunk.terrain",
		"chunk.items",
		"dynamic.overlays",
		"overlay.zones",
		"overlay.blocking",
		"overlay.houses",
		"overlay.highlight",
		"overlay.badges",
		"marker.labels",
		"nanovg",
		"anim.patch",
		"atlas.apply",
		"present",
	};

	inline constexpr size_t phaseIndex(Phase phase) noexcept {
		return static_cast<size_t>(phase);
	}

	struct State {
		std::atomic<bool> enabled { false };
		std::array<double, kPhaseCount> frame_ms {};
		std::array<uint64_t, kPhaseCount> frame_calls {};
		std::array<double, kPhaseCount> window_ms {};
		std::array<uint64_t, kPhaseCount> window_calls {};
		uint32_t frames = 0;
	};

	inline State state;

	inline void configureFromEnv() {
		const bool on = std::getenv("RME_PROFILE") != nullptr;
		state.enabled.store(on, std::memory_order_relaxed);
	}

	[[nodiscard]] inline bool enabled() noexcept {
		return state.enabled.load(std::memory_order_relaxed);
	}

	inline void add(Phase phase, double ms) {
		const size_t index = phaseIndex(phase);
		state.frame_ms[index] += ms;
		++state.frame_calls[index];
	}

	// RAII accumulator. Reads the enabled flag once so the destructor cost is predictable.
	class Scope {
	public:
		explicit Scope(Phase phase) noexcept :
			phase_(phase),
			on_(state.enabled.load(std::memory_order_relaxed)) {
			if (on_) {
				start_ = Clock::now();
			}
		}

		~Scope() {
			end();
		}

		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

		// Closes the scope early with the same recording the destructor performs, so its
		// sample lands before code that folds the profiler window (endFrame) instead of
		// leaking the leftover time into the next window.
		void end() noexcept {
			if (on_) {
				add(phase_, std::chrono::duration<double, std::milli>(Clock::now() - start_).count());
				on_ = false;
			}
		}

	private:
		using Clock = std::chrono::steady_clock;

		Clock::time_point start_ {};
		Phase phase_;
		bool on_;
	};

	// Closes the frame: folds the frame into the window and logs a ranked breakdown every
	// kLogIntervalFrames frames.
	inline void endFrame() {
		if (!enabled()) {
			return;
		}

		for (size_t i = 0; i < kPhaseCount; ++i) {
			state.window_ms[i] += state.frame_ms[i];
			state.window_calls[i] += state.frame_calls[i];
			state.frame_ms[i] = 0.0;
			state.frame_calls[i] = 0;
		}

		if (++state.frames < kLogIntervalFrames) {
			return;
		}

		std::array<size_t, kPhaseCount> order {};
		std::iota(order.begin(), order.end(), size_t { 0 });
		std::sort(order.begin(), order.end(), [](size_t a, size_t b) {
			return state.window_ms[a] > state.window_ms[b];
		});

		const double frames = static_cast<double>(state.frames);
		std::string line;
		line.reserve(512);
		for (const size_t i : order) {
			if (state.window_ms[i] <= 0.0) {
				continue;
			}
			const double per_frame = state.window_ms[i] / frames;
			const uint64_t calls_per_frame = state.window_calls[i] / state.frames;
			if (!line.empty()) {
				line += " | ";
			}
			line += std::format("{:.2f}ms {} ({}x)", per_frame, kPhaseNames[i], calls_per_frame);
		}
		spdlog::info("[PhaseProfile] {} frames | {}", state.frames, line);

		state.window_ms.fill(0.0);
		state.window_calls.fill(0);
		state.frames = 0;
	}

	// A phase scope that also reports where we are in the frame when it starts, so nested
	// phases can be attributed (used for the top-level render entry points).
	struct FrameMarker {
		explicit FrameMarker(Phase phase) noexcept : scope_(phase) {}

	private:
		Scope scope_;
	};
}

#endif
