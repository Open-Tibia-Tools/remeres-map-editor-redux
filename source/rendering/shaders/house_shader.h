#ifndef RME_RENDERING_SHADERS_HOUSE_SHADER_H_
#define RME_RENDERING_SHADERS_HOUSE_SHADER_H_

#include <string_view>

namespace rme::rendering::shaders {

/**
 * @brief GLSL module providing house zoning and overlay evaluation routines.
 *
 * Implements:
 * applyHouseOverlay: Evaluates house zoning atmosphere wash, centered 'H' ground emblem,
 * and 45-degree dark diagonal hatching for walls and extended items.
 * (Note: House Entry tile indicators are evaluated centrally via indicator_shader.h)
 */
inline constexpr std::string_view HOUSE_SHADER_GLSL = R"(
void applyHouseOverlay(inout vec4 fragColor, vec2 worldPos, float houseId, uint currentHouseId, int showHouses) {
    if (showHouses == 0 || houseId >= 1000000.0) {
        return;
    }

    uint uHouseId = uint(abs(houseId) + 0.5);
    if (uHouseId == 0u) {
        return;
    }

    bool isActive = (uHouseId == currentHouseId);

    // Multiplicative tints:
    // House Active: Lime green (matching ENTRY), House Inactive: Muted violet
    vec3 zTint = isActive ? vec3(0.85, 1.00, 0.45) : vec3(0.80, 0.74, 0.92);

    if (houseId < 0.0) {
        // Base House Shader (ground): seamless multiplicative tint (NO inside lines or bevels)
        fragColor.rgb *= zTint;
    } else {
        // Extended House Shader (items/walls): seamless multiplicative tint
        vec3 itemTint = mix(vec3(1.0), zTint, 0.80);
        fragColor.rgb *= itemTint;
    }
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_HOUSE_SHADER_H_
