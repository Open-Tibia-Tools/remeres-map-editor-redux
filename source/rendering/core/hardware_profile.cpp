#include "rendering/core/hardware_profile.h"

#include <algorithm>
#include <spdlog/spdlog.h>

HardwareProfileManager& HardwareProfileManager::get() {
	static HardwareProfileManager instance;
	return instance;
}

void HardwareProfileManager::initialize(const SystemSpecs& specs) {
	specs_ = specs;

	// Classifier rules:
	// LowEnd: VRAM <= 2048 MB OR Total RAM <= 8192 MB OR CPU cores <= 4
	if (specs_.dedicated_vram_mb > 0 && specs_.dedicated_vram_mb <= 2048) {
		detected_tier_ = HardwareTier::LowEnd;
	} else if (specs_.total_ram_mb > 0 && specs_.total_ram_mb <= 8192) {
		detected_tier_ = HardwareTier::LowEnd;
	} else if (specs_.cpu_cores <= 4) {
		detected_tier_ = HardwareTier::LowEnd;
	} else if (specs_.dedicated_vram_mb >= 8192 && specs_.total_ram_mb >= 24576 && specs_.cpu_cores >= 8) {
		detected_tier_ = HardwareTier::HighPerformance;
	} else {
		detected_tier_ = HardwareTier::Balanced;
	}

	spdlog::info("[HardwareProfile] Classification complete: Detected Tier is {} (CPU: {} threads, RAM: {} MB, VRAM: {} MB)",
		getTierName(detected_tier_), specs_.cpu_cores, specs_.total_ram_mb, specs_.dedicated_vram_mb);

	recomputeActiveBudget();
}

void HardwareProfileManager::updateSpecs(const SystemSpecs& specs) {
	initialize(specs);
}

HardwareTier HardwareProfileManager::getActiveTier() const noexcept {
	switch (mode_) {
		case HardwareProfileMode::LowEnd:
			return HardwareTier::LowEnd;
		case HardwareProfileMode::Balanced:
			return HardwareTier::Balanced;
		case HardwareProfileMode::HighPerformance:
			return HardwareTier::HighPerformance;
		case HardwareProfileMode::Auto:
		default:
			return detected_tier_;
	}
}

void HardwareProfileManager::setProfileMode(HardwareProfileMode mode) {
	if (mode_ == mode) {
		return;
	}
	mode_ = mode;
	recomputeActiveBudget();
}

HardwareBudget HardwareProfileManager::getBudgetForTier(HardwareTier tier, const SystemSpecs& specs) {
	HardwareBudget budget;
	switch (tier) {
		case HardwareTier::LowEnd:
			budget.initial_atlas_layers = 4;   // 268 MB VRAM
			budget.atlas_expansion_step = 2;   // 134 MB step
			budget.max_cached_chunks = 8192;   // ~180 MB VRAM ceiling
			budget.target_cached_chunks = 6144;
			budget.far_floor_threshold_frames = 30; // aggressive off-floor eviction
			budget.worker_threads = std::clamp(specs.cpu_cores, 1u, 2u);
			break;

		case HardwareTier::Balanced:
			budget.initial_atlas_layers = 8;   // 536 MB VRAM
			budget.atlas_expansion_step = 4;   // 268 MB step
			budget.max_cached_chunks = 24576;  // ~500 MB VRAM ceiling
			budget.target_cached_chunks = 18432;
			budget.far_floor_threshold_frames = 60;
			budget.worker_threads = std::clamp(specs.cpu_cores > 1 ? specs.cpu_cores - 1 : 1u, 2u, 4u);
			break;

		case HardwareTier::HighPerformance:
		default:
			budget.initial_atlas_layers = 16;  // 1073 MB VRAM
			budget.atlas_expansion_step = 4;   // 268 MB step
			budget.max_cached_chunks = 65536;  // ~1.2 GB VRAM ceiling
			budget.target_cached_chunks = 49152;
			budget.far_floor_threshold_frames = 120;
			budget.worker_threads = std::clamp(specs.cpu_cores > 1 ? specs.cpu_cores - 1 : 1u, 4u, 16u);
			break;
	}
	return budget;
}

std::string_view HardwareProfileManager::getTierName(HardwareTier tier) noexcept {
	switch (tier) {
		case HardwareTier::LowEnd:
			return "Low-End";
		case HardwareTier::Balanced:
			return "Balanced";
		case HardwareTier::HighPerformance:
			return "High-Performance";
		default:
			return "Unknown";
	}
}

std::string_view HardwareProfileManager::getModeName(HardwareProfileMode mode) noexcept {
	switch (mode) {
		case HardwareProfileMode::Auto:
			return "Auto";
		case HardwareProfileMode::LowEnd:
			return "Low-End";
		case HardwareProfileMode::Balanced:
			return "Balanced";
		case HardwareProfileMode::HighPerformance:
			return "High-Performance";
		default:
			return "Unknown";
	}
}

void HardwareProfileManager::registerChangeCallback(ChangeCallback cb) {
	if (cb) {
		callbacks_.push_back(std::move(cb));
	}
}

void HardwareProfileManager::recomputeActiveBudget() {
	const auto active_tier = getActiveTier();
	active_budget_ = getBudgetForTier(active_tier, specs_);

	spdlog::info("[HardwareProfile] Active Mode: {} | Active Tier: {} | Atlas Initial Layers: {} | Max Cached Chunks: {} | Target Chunks: {} | Workers: {}",
		getModeName(mode_),
		getTierName(active_tier),
		active_budget_.initial_atlas_layers,
		active_budget_.max_cached_chunks,
		active_budget_.target_cached_chunks,
		active_budget_.worker_threads
	);

	for (const auto& cb : callbacks_) {
		cb(active_budget_);
	}
}
