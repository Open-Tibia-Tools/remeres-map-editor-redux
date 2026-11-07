//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_INDICATORS_ZONE_COLORS_H_
#define RME_RENDERING_INDICATORS_ZONE_COLORS_H_

#include <array>
#include <cmath>
#include <cstdint>
#include <glm/vec3.hpp>

namespace rme::rendering {

namespace detail {

inline glm::vec3 computeZoneColor(uint16_t zone_id) noexcept {
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

inline const std::array<glm::vec3, 1024>& getZoneColorTable() noexcept {
	static const auto s_table = [] {
		std::array<glm::vec3, 1024> table;
		for (size_t i = 0; i < table.size(); ++i) {
			table[i] = computeZoneColor(static_cast<uint16_t>(i));
		}
		return table;
	}();
	return s_table;
}

} // namespace detail

/**
 * @brief Computes a visually distinct, deterministic RGB color for any zone ID.
 *
 * Uses the golden-ratio hue spread across the HSV color wheel to keep neighboring
 * zone IDs visually contrasting. Frequently accessed IDs (< 1024) are served
 * from a precomputed lookup table to eliminate float modulo/fabs instructions in the render loop.
 */
inline glm::vec3 getZoneColor(uint16_t zone_id) noexcept {
	if (zone_id < 1024) {
		return detail::getZoneColorTable()[zone_id];
	}
	return detail::computeZoneColor(zone_id);
}

} // namespace rme::rendering

#endif // RME_RENDERING_INDICATORS_ZONE_COLORS_H_
