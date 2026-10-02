#ifndef RME_RENDERING_SHADERS_HOUSE_SHADER_H_
#define RME_RENDERING_SHADERS_HOUSE_SHADER_H_

#include <string_view>

namespace rme::rendering::shaders {

/**
 * @brief GLSL module providing house zoning and overlay evaluation routines.
 *
 * Implements:
 * applyHouseOverlay: Evaluates house zoning atmosphere wash (active vs. inactive house tinting)
 * for ground tiles and deep shadow tinting for walls and extended items.
 * Connected borders are handled via ZoneOverlayDrawer border passes.
 * (Note: House Entry tile indicators are evaluated centrally via indicator_shader.h)
 */
inline constexpr std::string_view HOUSE_SHADER_GLSL = R"(
uniform vec4 uHouseActiveWash;
uniform vec4 uHouseInactiveWash;
uniform int uHouseActiveBlendMode;
uniform int uHouseInactiveBlendMode;

void applyHouseOverlay(inout vec4 fragColor, vec2 worldPos, float houseId, uint currentHouseId, int showHouses) {
    if (showHouses == 0 || houseId >= 1000000.0) {
        return;
    }

    uint uHouseId = uint(abs(houseId) + 0.5);
    if (uHouseId == 0u) {
        return;
    }

    bool isActive = (uHouseId == currentHouseId);

    // Dynamic house active / inactive washes:
    vec4 zWash = isActive ? uHouseActiveWash : uHouseInactiveWash;
    int blendMode = isActive ? uHouseActiveBlendMode : uHouseInactiveBlendMode;

    if (blendMode == 1) {
        if (houseId < 0.0) {
            // Base House Shader (ground): multiplicative tint
            fragColor.rgb *= mix(vec3(1.0), zWash.rgb, zWash.a);
        } else {
            // Extended House Shader (items/walls): deep shadow multiplicative tint
            vec3 darkRgb = zWash.rgb * 0.55;
            float wallAlpha = min(0.95, zWash.a * 1.40);
            fragColor.rgb *= mix(vec3(1.0), darkRgb, wallAlpha);
        }
    } else {
        if (houseId < 0.0) {
            // Base House Shader (ground): clean wash
            fragColor.rgb = mix(fragColor.rgb, zWash.rgb, zWash.a);
        } else {
            // Extended House Shader (items/walls): deep shadow tint (45% shadow) + higher opacity
            vec3 darkRgb = zWash.rgb * 0.55;
            float wallAlpha = min(0.95, zWash.a * 1.40);
            fragColor.rgb = mix(fragColor.rgb, darkRgb, wallAlpha);
        }
    }
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_HOUSE_SHADER_H_
