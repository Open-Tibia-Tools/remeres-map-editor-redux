#ifndef RME_RENDERING_SHADERS_INDICATOR_SHADER_H_
#define RME_RENDERING_SHADERS_INDICATOR_SHADER_H_

#include <string_view>

namespace rme::rendering::shaders {

/**
 * @brief GLSL module providing full 32x32 square tile indicators with pixel typography.
 *
 * Supports:
 * - entry   (House entry point: Bright Pure Green)
 * - spawn   (Spawn center: Bright Pure Magenta)
 * - town    (Town temple: Bright Gold/Amber)
 * - waypt   (Waypoint: Vivid Bright Blue)
 * - stair   (Technical invisible stairs: Bright Pure Yellow)
 * - walk    (Technical invisible walkable: Bright Pure Cyan)
 * - block   (Technical invisible wall: Bright Pure Red)
 * - light   (Technical primal light source: Bright Sky Blue)
 * - invalid (Missing ground: Bright Pure Red)
 * - invalid (Missing top item: Bright Pure Orange)
 * - invalid (Invalid zone flags: Bright Pure Magenta)
 */
inline constexpr std::string_view INDICATOR_SHADER_GLSL = R"(
bool evaluateTileIndicator(vec2 quadCoord, float markerId, uint currentHouseId,
                           int showHouses, int showSpawns, int showTowns,
                           int showWaypoints, int showTechItems,
                           int showInvalidTiles, int showInvalidZones,
                           vec4 tint,
                           out vec4 outColor) {
    if (markerId < 1000000.0) {
        return false;
    }

    int lx = clamp(int(floor(quadCoord.x * 32.0)), 0, 31);
    int ly = clamp(int(floor(quadCoord.y * 32.0)), 0, 31);

    vec4 zBlack = vec4(0.05, 0.05, 0.07, 0.98);

    vec4 zWash;
    uint inMask[7];

    if (markerId < 2000000.0) {
        // House Entry ("entry") - Bright Pure Green #00FF00
        if (showHouses == 0) discard;
        zWash  = vec4(0.00, 1.00, 0.00, 0.50);
        inMask = uint[7](0x00008000u, 0x0001C000u, 0x014C8C80u, 0x01549540u, 0x014495C0u, 0x01849440u, 0x01051580u);
    } else if (markerId < 3000000.0) {
        // Spawn Center ("spawn") - Bright Pure Magenta #FF00FF
        if (showSpawns == 0) discard;
        zWash  = vec4(1.00, 0.00, 1.00, 0.50);
        inMask = uint[7](0x00000000u, 0x00000000u, 0x01A266C0u, 0x02A28A20u, 0x02AACA40u, 0x02AAA680u, 0x0294C260u);
    } else if (markerId < 4000000.0) {
        // Town Temple ("town") - Bright Gold/Amber #FFCC00
        if (showTowns == 0) discard;
        zWash  = vec4(1.00, 0.85, 0.00, 0.50);
        inMask = uint[7](0x00000100u, 0x00000380u, 0x00689100u, 0x00A8A900u, 0x00AAA900u, 0x00AAA900u, 0x00A51200u);
    } else if (markerId < 5000000.0) {
        // Waypoint ("waypt") - Bright Vivid Blue #00A6FF
        if (showWaypoints == 0) discard;
        zWash  = vec4(0.00, 0.65, 1.00, 0.50);
        inMask = uint[7](0x01000000u, 0x03800000u, 0x011A9A20u, 0x012AA220u, 0x012AB2A0u, 0x011B2AA0u, 0x020A3140u);
    } else if (markerId < 6000000.0) {
        // Invisible Stairs ("stair") - Bright Pure Yellow #FFFF00
        if (showTechItems == 0) discard;
        zWash  = vec4(1.00, 1.00, 0.00, 0.50);
        inMask = uint[7](0x00081000u, 0x00003800u, 0x00699300u, 0x00AA1080u, 0x002B1100u, 0x002A9200u, 0x002B2180u);
    } else if (markerId < 7000000.0) {
        // Invisible Walkable ("walk") - Bright Pure Cyan #00FFFF
        if (showTechItems == 0) discard;
        zWash  = vec4(0.00, 1.00, 1.00, 0.50);
        inMask = uint[7](0x00140000u, 0x00140000u, 0x0054D100u, 0x00351100u, 0x00359500u, 0x00555500u, 0x00558A00u);
    } else if (markerId < 8000000.0) {
        // Invisible Wall ("block") - Bright Pure Red #FF0000
        if (showTechItems == 0) discard;
        zWash  = vec4(1.00, 0.00, 0.00, 0.50);
        inMask = uint[7](0x00200880u, 0x00200880u, 0x00AC4980u, 0x0062AA80u, 0x0062AA80u, 0x00A2AA80u, 0x00AC4980u);
    } else if (markerId < 9000000.0) {
        // Primal Light ("light") - Bright Sky Blue #00D5FF
        if (showTechItems == 0) discard;
        zWash  = vec4(0.00, 0.85, 1.00, 0.50);
        inMask = uint[7](0x00210500u, 0x00710100u, 0x00236500u, 0x00255500u, 0x00255500u, 0x00256500u, 0x00454500u);
    } else if (markerId < 10000000.0) {
        // Missing Ground Tile ("invalid") - Bright Pure Red #FF0000
        if (showInvalidTiles == 0) discard;
        zWash  = vec4(1.00, 0.00, 0.00, 0.50);
        inMask = uint[7](0x02280020u, 0x02080000u, 0x0329A9A0u, 0x02AA2AA0u, 0x02AB2AA0u, 0x02AAAAA0u, 0x032B12A0u);
    } else if (markerId < 11000000.0) {
        // Missing Top Item ("invalid") - Bright Pure Orange #FF8000
        if (showInvalidTiles == 0) discard;
        zWash  = vec4(1.00, 0.50, 0.00, 0.50);
        inMask = uint[7](0x02280020u, 0x02080000u, 0x0329A9A0u, 0x02AA2AA0u, 0x02AB2AA0u, 0x02AAAAA0u, 0x032B12A0u);
    } else {
        // Invalid Zone Flags ("invalid") - Bright Pure Magenta #FF00FF
        if (showInvalidZones == 0) discard;
        zWash  = vec4(1.00, 0.00, 1.00, 0.50);
        inMask = uint[7](0x02280020u, 0x02080000u, 0x0329A9A0u, 0x02AA2AA0u, 0x02AB2AA0u, 0x02AAAAA0u, 0x032B12A0u);
    }

    // 1. Outer 1px black border
    if (lx == 0 || lx == 31 || ly == 0 || ly == 31) {
        outColor = zBlack;
        outColor.rgb *= tint.rgb;
        outColor.a *= tint.a;
        return true;
    }

    // 2. 1px light border (kitchen tile bevel)
    if (lx == 1 || lx == 30 || ly == 1 || ly == 30) {
        float highlightStrength = (ly == 1 || lx == 1) ? 0.65 : 0.30;
        float alpha = (ly == 1 || lx == 1) ? 0.85 : 0.75;
        outColor = vec4(mix(zWash.rgb, vec3(1.0), highlightStrength), alpha);
        outColor.rgb *= tint.rgb;
        outColor.a *= tint.a;
        return true;
    }

    // 3. Text (white font) with 1px black outline (ly in 11..19)
    if (ly >= 11 && ly <= 19 && lx >= 2 && lx <= 29) {
        int row = ly - 12;
        bool isText = false;
        if (row >= 0 && row < 7) {
            isText = ((inMask[row] >> lx) & 1u) != 0u;
        }

        if (isText) {
            outColor = vec4(1.0, 1.0, 1.0, 1.0);
            outColor.rgb *= tint.rgb;
            outColor.a *= tint.a;
            return true;
        }

        // 8-way dilation around white font for black outline
        bool isOutline = false;
        for (int dy = -1; dy <= 1; ++dy) {
            int r = row + dy;
            if (r >= 0 && r < 7) {
                uint m = inMask[r];
                if (((m >> (lx - 1)) & 7u) != 0u) {
                    isOutline = true;
                    break;
                }
            }
        }

        if (isOutline) {
            outColor = zBlack;
            outColor.rgb *= tint.rgb;
            outColor.a *= tint.a;
            return true;
        }
    }

    // 4. Interior 50% Transparent Background Wash
    outColor = zWash;
    outColor.rgb *= tint.rgb;
    outColor.a *= tint.a;
    return true;
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_INDICATOR_SHADER_H_
