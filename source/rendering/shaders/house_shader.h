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

    // Option 2 (Deep Shadow):
    // House Active: Deep Emerald Lime #59BF0D, House Inactive: Midnight Amethyst #5C38A6
    vec4 zWash = isActive ? vec4(0.35, 0.75, 0.05, 0.52) : vec4(0.36, 0.22, 0.65, 0.52);

    if (houseId < 0.0) {
        // Base House Shader (ground): deep clean wash (52% opacity)
        fragColor.rgb = mix(fragColor.rgb, zWash.rgb, zWash.a);
    } else {
        // Extended House Shader (items/walls): deep shadow tint (45% shadow) + higher opacity (~72%)
        vec3 darkRgb = zWash.rgb * 0.55;
        float wallAlpha = min(0.95, zWash.a * 1.40);
        fragColor.rgb = mix(fragColor.rgb, darkRgb, wallAlpha);
    }
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_HOUSE_SHADER_H_
