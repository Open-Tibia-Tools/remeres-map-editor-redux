#ifndef RME_RENDERING_SHADERS_PATHING_SHADER_H_
#define RME_RENDERING_SHADERS_PATHING_SHADER_H_

#include <string_view>

namespace rme::rendering::shaders {

/**
 * @brief GLSL module providing Extended Pathing Shader evaluation routines.
 *
 * Implements:
 * applyPathingOverlay: Evaluates blocking obstacle shading directly on non-walkable
 * walls, doodads, and items when "Show pathing" and "Extended pathing shader" are active.
 */
inline constexpr std::string_view PATHING_SHADER_GLSL = R"(
uniform int uShowBlocking;
uniform int uExtendedPathingShader;
uniform vec4 uBlockingWash;
uniform int uBlockingBlendMode;

void applyPathingOverlay(inout vec4 fragColor, bool isBlocking) {
    if (uShowBlocking == 0 || uExtendedPathingShader == 0 || !isBlocking) {
        return;
    }

    vec3 darkRgb = uBlockingWash.rgb * 0.55;
    float wallAlpha = min(0.95, uBlockingWash.a * 1.40);

    if (uBlockingBlendMode == 1) {
        // Multiplicative tint
        fragColor.rgb *= mix(vec3(1.0), darkRgb, wallAlpha);
    } else {
        // Alpha blend wash
        fragColor.rgb = mix(fragColor.rgb, darkRgb, wallAlpha);
    }
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_PATHING_SHADER_H_
