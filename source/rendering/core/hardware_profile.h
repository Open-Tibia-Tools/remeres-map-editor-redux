#ifndef RME_RENDERING_CORE_HARDWARE_PROFILE_H
#define RME_RENDERING_CORE_HARDWARE_PROFILE_H

#include "util/system_specs.h"
#include <cstdint>
#include <string_view>
#include <functional>
#include <vector>

enum class HardwareTier {
	LowEnd,           // <= 2 GB VRAM / <= 8 GB RAM / <= 4 cores
	Balanced,         // 3-6 GB VRAM / 12-16 GB RAM / 6-8 cores
	HighPerformance   // >= 8 GB VRAM / >= 24 GB RAM / >= 8 cores
};

enum class HardwareProfileMode {
	Auto = 0,
	LowEnd = 1,
	Balanced = 2,
	HighPerformance = 3
};

struct HardwareBudget {
	int initial_atlas_layers = 16;
	int atlas_expansion_step = 4;
	size_t max_cached_chunks = 65536;
	size_t target_cached_chunks = 49152;
	uint64_t far_floor_threshold_frames = 60;
	unsigned int worker_threads = 4;
};

class HardwareProfileManager {
public:
	static HardwareProfileManager& get();

	void initialize(const SystemSpecs& specs);
	void updateSpecs(const SystemSpecs& specs);

	[[nodiscard]] const SystemSpecs& getSpecs() const noexcept { return specs_; }
	[[nodiscard]] HardwareTier getDetectedTier() const noexcept { return detected_tier_; }
	[[nodiscard]] HardwareTier getActiveTier() const noexcept;
	[[nodiscard]] HardwareProfileMode getProfileMode() const noexcept { return mode_; }
	[[nodiscard]] const HardwareBudget& getActiveBudget() const noexcept { return active_budget_; }

	void setProfileMode(HardwareProfileMode mode);

	static HardwareBudget getBudgetForTier(HardwareTier tier, const SystemSpecs& specs);
	static std::string_view getTierName(HardwareTier tier) noexcept;
	static std::string_view getModeName(HardwareProfileMode mode) noexcept;

	// Observer pattern to notify listeners (e.g. ChunkCacheManager) when profile changes
	using ChangeCallback = std::function<void(const HardwareBudget&)>;
	void registerChangeCallback(ChangeCallback cb);

private:
	HardwareProfileManager() = default;
	void recomputeActiveBudget();

	SystemSpecs specs_;
	HardwareTier detected_tier_ = HardwareTier::Balanced;
	HardwareProfileMode mode_ = HardwareProfileMode::Auto;
	HardwareBudget active_budget_;
	std::vector<ChangeCallback> callbacks_;
};

#endif // RME_RENDERING_CORE_HARDWARE_PROFILE_H
