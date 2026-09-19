//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_LIGHT_PALETTE_H_
#define RME_RENDERING_CORE_LIGHT_PALETTE_H_

#include <cstdint>
#include <array>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include "rendering/core/light_types.h"

namespace rme::lighting {

struct PaletteRGB {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
};

inline constexpr auto generatePaletteTable() {
	std::array<PaletteRGB, 256> table {};
	for (int color = 1; color < 216; ++color) {
		table[color].r = static_cast<uint8_t>((color / 36) % 6 * 51);
		table[color].g = static_cast<uint8_t>((color / 6) % 6 * 51);
		table[color].b = static_cast<uint8_t>(color % 6 * 51);
	}
	return table;
}

inline constexpr auto s_palette_table = generatePaletteTable();

[[nodiscard]] inline glm::vec3 getAmbientRGB(int floor, const LightConfig& config) noexcept {
	const bool above_ground = floor <= 7;
	const uint8_t color_index = above_ground ? config.ambient_color : static_cast<uint8_t>(215);
	const float server_intensity = above_ground ? (static_cast<float>(config.ambient_intensity) / 255.0f) : 0.0f;
	const float final_intensity = std::max(config.minimum_ambient_light, server_intensity);
	const auto& rgb = s_palette_table[color_index];
	return glm::vec3(
		static_cast<float>(rgb.r) / 255.0f,
		static_cast<float>(rgb.g) / 255.0f,
		static_cast<float>(rgb.b) / 255.0f
	) * final_intensity;
}

[[nodiscard]] inline uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept {
	return static_cast<uint32_t>(r) |
	      (static_cast<uint32_t>(g) << 8) |
	      (static_cast<uint32_t>(b) << 16) |
	      (static_cast<uint32_t>(a) << 24);
}

} // namespace rme::lighting

#endif // RME_RENDERING_CORE_LIGHT_PALETTE_H_
