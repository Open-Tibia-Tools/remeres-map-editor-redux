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

    ivec2 p = ivec2(floor(worldPos));
    bool isActive = (uHouseId == currentHouseId);

    int lx = p.x % 32; if (lx < 0) lx += 32;
    int ly = p.y % 32; if (ly < 0) ly += 32;

    // House Active: #B4EB1F (Lime green, same as ENTRY), House Inactive: #8F7FC4 (muted violet)
    vec4 zWash  = isActive ? vec4(0.71, 0.92, 0.12, 0.28) : vec4(0.56, 0.50, 0.77, 0.28);
    vec4 zLight = isActive ? vec4(0.89, 1.00, 0.56, 0.85) : vec4(0.82, 0.79, 0.94, 0.85);
    vec4 zDark  = isActive ? vec4(0.31, 0.42, 0.00, 0.95) : vec4(0.25, 0.20, 0.44, 0.95);

    if (houseId < 0.0) {
        // Base House Shader (ground): 3D kitchen tile lines + translucent wash (NO 'H' stamp, NO stripes)
        if (ly == 0 || lx == 0) {
            fragColor.rgb = mix(fragColor.rgb, zLight.rgb, zLight.a);
        } else if (ly == 31 || lx == 31) {
            fragColor.rgb = mix(fragColor.rgb, zDark.rgb, zDark.a);
        } else {
            fragColor.rgb = mix(fragColor.rgb, zWash.rgb, zWash.a);
        }
    } else {
        // Extended House Shader (items/walls): clean translucent wash (no striped lines)
        vec4 itemWash = vec4(zWash.rgb, zWash.a * 0.70);
        fragColor.rgb = mix(fragColor.rgb, itemWash.rgb, itemWash.a);
    }
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_HOUSE_SHADER_H_
