#ifndef RME_UTIL_SYSTEM_SPECS_H
#define RME_UTIL_SYSTEM_SPECS_H

#include <cstdint>
#include <string>

/**
 * SystemSpecs captures hardware capacities of the host system.
 */
struct SystemSpecs {
	uint32_t cpu_cores = 4;
	uint64_t total_ram_mb = 8192;
	uint64_t avail_ram_mb = 4096;
	uint64_t dedicated_vram_mb = 2048;
	uint64_t shared_vram_mb = 4096;
	std::string gpu_vendor = "Unknown";
	std::string gpu_renderer = "Unknown";
	std::string gl_version = "Unknown";
};

/**
 * SystemSpecsDetector discovers CPU, RAM, and GPU/VRAM parameters.
 * Adheres to SRP by handling hardware querying only without graphics or game logic.
 */
class SystemSpecsDetector {
public:
	static SystemSpecs detect(bool gl_context_active = false);
};

#endif // RME_UTIL_SYSTEM_SPECS_H
