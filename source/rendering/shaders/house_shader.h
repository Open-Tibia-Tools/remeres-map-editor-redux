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

    // House Active: #B4EB1F (Lime green, same as ENTRY), House Inactive: #8F7FC4 (muted violet)
    vec4 zWash = isActive ? vec4(0.71, 0.92, 0.12, 0.30) : vec4(0.56, 0.50, 0.77, 0.28);

    if (houseId < 0.0) {
        // Base House Shader (ground): clean translucent wash (NO inside lines or bevels)
        fragColor.rgb = mix(fragColor.rgb, zWash.rgb, zWash.a);
    } else {
        // Extended House Shader (items/walls): clean translucent wash
        vec4 itemWash = vec4(zWash.rgb, zWash.a * 0.70);
        fragColor.rgb = mix(fragColor.rgb, itemWash.rgb, itemWash.a);
    }
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_HOUSE_SHADER_H_
