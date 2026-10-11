//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_INDICATORS_ZONE_COLORS_H_
#define RME_RENDERING_INDICATORS_ZONE_COLORS_H_

#include <cmath>
#include <cstdint>
#include <glm/vec3.hpp>

namespace rme::rendering {

/**
 * @brief Computes a visually distinct, deterministic RGB color for any zone ID.
 *
 * Uses the golden-ratio hue spread across the HSV color wheel to keep neighboring
 * zone IDs visually contrasting without needing a fixed lookup table.
 */
inline glm::vec3 getZoneColor(uint16_t zone_id) noexcept {
	const float hue = std::fmod(static_cast<float>(zone_id) * 0.618034f, 1.0f) * 6.0f;
	const float x = 1.0f - std::fabs(std::fmod(hue, 2.0f) - 1.0f);
	switch (static_cast<int>(hue)) {
		case 0: return { 1.0f, x, 0.0f };
		case 1: return { x, 1.0f, 0.0f };
		case 2: return { 0.0f, 1.0f, x };
		case 3: return { 0.0f, x, 1.0f };
		case 4: return { x, 0.0f, 1.0f };
		default: return { 1.0f, 0.0f, x };
	}
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_ZONE_COLORS_H_
