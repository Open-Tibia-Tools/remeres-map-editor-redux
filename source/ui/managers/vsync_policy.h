#ifndef RME_VSYNC_POLICY_H_
#define RME_VSYNC_POLICY_H_

#include "app/definitions.h"

enum class SwapIntervalApplicationResult {
	Set,
	NonAdaptive,
	NotSet,
};

enum class VSyncApplicationOutcome {
	Applied,
	AdaptiveFallback,
	Failed,
};

[[nodiscard]] VSyncMode sanitizeVSyncMode(int raw_value) noexcept;
[[nodiscard]] int getSwapIntervalForMode(VSyncMode mode) noexcept;
[[nodiscard]] VSyncApplicationOutcome classifySwapIntervalResult(VSyncMode requested_mode, SwapIntervalApplicationResult result) noexcept;

#endif
