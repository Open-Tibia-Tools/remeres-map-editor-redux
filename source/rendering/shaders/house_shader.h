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

    // Zero-Fill House Overlays: No background wash, floor sprites remain 100% natural and visible
    if (houseId < 0.0) {
        // Base House Shader (ground): corner brackets + centered 'H' emblem (zero background fill)
        int lx = p.x % 32; if (lx < 0) lx += 32;
        int ly = p.y % 32; if (ly < 0) ly += 32;

        bool isCore, isShadow;
        evaluateTileBracket(lx, ly, isCore, isShadow);

        int dx = abs(lx - 16);
        int dy = abs(ly - 16);

        bool isInside = (dx >= 2 && dx <= 3 && dy <= 5) || (dx < 2 && dy <= 1);
        bool isOutline = (dy == 6 && dx >= 1 && dx <= 4) ||
                         (dx == 4 && dy <= 5) ||
                         (dx == 1 && dy >= 2 && dy <= 5) ||
                         (dx == 0 && dy == 2);

        if (isInside) {
            vec4 insideColor = isActive
                ? vec4(0.55, 1.00, 0.55, 0.98)   // Light neon green
                : vec4(1.00, 0.80, 0.40, 0.98);  // Light amber / gold
            fragColor.rgb = mix(fragColor.rgb, insideColor.rgb, insideColor.a);
        } else if (isOutline) {
            vec4 outlineColor = isActive
                ? vec4(0.04, 0.32, 0.08, 0.95)   // Dark green outline
                : vec4(0.40, 0.20, 0.00, 0.95);  // Dark amber outline
            fragColor.rgb = mix(fragColor.rgb, outlineColor.rgb, outlineColor.a);
        } else if (isCore) {
            vec4 bracketColor = isActive
                ? vec4(0.20, 0.95, 0.20, 0.95)   // Green bracket
                : vec4(1.00, 0.60, 0.00, 0.90);  // Amber bracket
            fragColor.rgb = mix(fragColor.rgb, bracketColor.rgb, bracketColor.a);
        } else if (isShadow) {
            vec4 shadowColor = vec4(0.04, 0.04, 0.06, 0.95);
            fragColor.rgb = mix(fragColor.rgb, shadowColor.rgb, shadowColor.a);
        }
    } else {
        // Extended House Shader (items/walls): extremely dark diagonal lines from right to left
        int d = (p.x + p.y) % 4;
        if (d < 0) d += 4;
        if (d == 0) {
            vec4 hatchColor = isActive
                ? vec4(0.01, 0.05, 0.02, 0.92)   // Active house walls: extremely dark deep forest ink line
                : vec4(0.08, 0.04, 0.01, 0.90);  // Inactive house walls: extremely dark espresso amber ink line

            fragColor.rgb = mix(fragColor.rgb, hatchColor.rgb, hatchColor.a);
        }
    }
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_HOUSE_SHADER_H_
