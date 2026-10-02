#ifndef RME_RENDERING_SHADERS_INDICATOR_SHADER_H_
#define RME_RENDERING_SHADERS_INDICATOR_SHADER_H_

#include <string_view>

namespace rme::rendering::shaders {

/**
 * @brief GLSL module providing full 32x32 square tile indicators with pixel typography.
 *
 * Supports:
 * - ENTRY  (House entry point: Blue)
 * - SPAWN  (Spawn center: Purple/Magenta)
 * - TOWN   (Town temple: Gold/Amber)
 * - WAYPT  (Waypoint: Cyan)
 * - STAIR  (Technical invisible stairs: Yellow)
 * - WALK   (Technical invisible walkable: Cyan)
 * - BLOCK  (Technical invisible wall: Red)
 * - LIGHT  (Technical primal light source: Sky Blue)
 * - INVALID (Missing ground: Red)
 * - INVALID (Missing top item: Orange/Yellow)
 * - INVALID (Invalid zone flags: Magenta)
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
    vec4 zFg;
    uint inMask[7];

    if (markerId < 2000000.0) {
        // House Entry ("ENTRY") - Lime Green #B4EB1F (complement of violet)
        if (showHouses == 0) discard;
        zWash  = vec4(0.71, 0.92, 0.12, 0.28);
        zFg    = vec4(0.71, 0.92, 0.12, 0.98);
        inMask = uint[7](0x00000000u, 0x0519CA70u, 0x05288E10u, 0x02188A30u, 0x02288A10u, 0x02288A70u, 0x00000000u);
    } else if (markerId < 3000000.0) {
        // Spawn Center ("SPAWN") - Magenta #F226F2
        if (showSpawns == 0) discard;
        zWash  = vec4(0.95, 0.15, 0.95, 0.28);
        zFg    = vec4(0.95, 0.15, 0.95, 0.98);
        inMask = uint[7](0x0944C770u, 0x0B452908u, 0x0D452908u, 0x0955E730u, 0x09552140u, 0x096D2140u, 0x09452138u);
    } else if (markerId < 4000000.0) {
        // Town Temple ("TOWN") - Gold #FFD900
        if (showTowns == 0) discard;
        zWash  = vec4(1.00, 0.85, 0.00, 0.28);
        zFg    = vec4(1.00, 0.85, 0.00, 0.98);
        inMask = uint[7](0x025133E0u, 0x02D14880u, 0x03514880u, 0x02554880u, 0x02554880u, 0x025B4880u, 0x02513080u);
    } else if (markerId < 5000000.0) {
        // Waypoint ("WAYPT") - Vivid Cyan #00E5FF
        if (showWaypoints == 0) discard;
        zWash  = vec4(0.00, 0.90, 1.00, 0.28);
        zFg    = vec4(0.00, 0.90, 1.00, 0.98);
        inMask = uint[7](0x1F3A4C88u, 0x044A5288u, 0x044A5288u, 0x04399EA8u, 0x040912A8u, 0x040912D8u, 0x04091288u);
    } else if (markerId < 6000000.0) {
        // Invisible Stairs ("STAIR") - Muted Yellow #E8D96B
        if (showTechItems == 0) discard;
        zWash  = vec4(0.91, 0.85, 0.42, 0.28);
        zFg    = vec4(0.91, 0.85, 0.42, 0.98);
        inMask = uint[7](0x07733EE0u, 0x09248810u, 0x09248810u, 0x07278860u, 0x05248880u, 0x09248880u, 0x09748870u);
    } else if (markerId < 7000000.0) {
        // Invisible Walkable ("WALK") - Muted Cyan-Teal #5CB8C4
        if (showTechItems == 0) discard;
        zWash  = vec4(0.36, 0.72, 0.77, 0.28);
        zFg    = vec4(0.36, 0.72, 0.77, 0.98);
        inMask = uint[7](0x02426440u, 0x01429440u, 0x00C29440u, 0x00C2F540u, 0x01429540u, 0x024296C0u, 0x025E9440u);
    } else if (markerId < 8000000.0) {
        // Invisible Wall ("BLOCK") - Muted Red #C0504D
        if (showTechItems == 0) discard;
        zWash  = vec4(0.75, 0.31, 0.30, 0.28);
        zFg    = vec4(0.75, 0.31, 0.30, 0.98);
        inMask = uint[7](0x09718270u, 0x050A4290u, 0x030A4290u, 0x030A4270u, 0x050A4290u, 0x090A4290u, 0x09719E70u);
    } else if (markerId < 9000000.0) {
        // Primal Light ("LIGHT") - Pale Ice Azure #BFE9FF
        if (showTechItems == 0) discard;
        zWash  = vec4(0.75, 0.91, 1.00, 0.28);
        zFg    = vec4(0.75, 0.91, 1.00, 0.98);
        inMask = uint[7](0x0FA5CE10u, 0x02242410u, 0x02242410u, 0x023DA410u, 0x02252410u, 0x02252410u, 0x0225CEF0u);
    } else if (markerId < 10000000.0) {
        // Missing Ground Tile ("INVALID") - Warm Red #EB1F3D
        if (showInvalidTiles == 0) discard;
        zWash  = vec4(0.92, 0.12, 0.24, 0.28);
        zFg    = vec4(0.92, 0.12, 0.24, 0.98);
        inMask = uint[7](0x00000000u, 0x0345D550u, 0x054555D0u, 0x0545D550u, 0x05455550u, 0x035D4950u, 0x00000000u);
    } else if (markerId < 11000000.0) {
        // Missing Top Item ("INVALID") - Warm Orange #FF850F
        if (showInvalidTiles == 0) discard;
        zWash  = vec4(1.00, 0.52, 0.06, 0.28);
        zFg    = vec4(1.00, 0.52, 0.06, 0.98);
        inMask = uint[7](0x00000000u, 0x0345D550u, 0x054555D0u, 0x0545D550u, 0x05455550u, 0x035D4950u, 0x00000000u);
    } else {
        // Invalid Zone Flags ("INVALID") - Magenta #F226F2
        if (showInvalidZones == 0) discard;
        zWash  = vec4(0.95, 0.15, 0.95, 0.28);
        zFg    = vec4(0.95, 0.15, 0.95, 0.98);
        inMask = uint[7](0x00000000u, 0x0345D550u, 0x054555D0u, 0x0545D550u, 0x05455550u, 0x035D4950u, 0x00000000u);
    }

    vec4 zBg = vec4(0.06, 0.06, 0.09, 0.95);

    // 1. 2px solid black outer outline
    if (lx <= 1 || lx >= 30 || ly <= 1 || ly >= 30) {
        outColor = zBlack;
        outColor.rgb *= tint.rgb;
        outColor.a *= tint.a;
        return true;
    }

    // 2. Centered Bold Badge (ly in 10..20, lx in 2..29)
    if (lx >= 2 && lx <= 29 && ly >= 10 && ly <= 20) {
        bool isBadgeBorder = (lx == 2 || lx == 29 || ly == 10 || ly == 20);
        if (isBadgeBorder) {
            // Discard/wash outer 4 corner pixels of badge for rounded pill effect
            if ((lx == 2 && ly == 10) || (lx == 29 && ly == 10) ||
                (lx == 2 && ly == 20) || (lx == 29 && ly == 20)) {
                outColor = zWash;
            } else {
                outColor = zFg;
            }
            outColor.rgb *= tint.rgb;
            outColor.a *= tint.a;
            return true;
        }

        int row = ly - 12;
        bool isText = false;
        if (row >= 0 && row < 7) {
            isText = ((inMask[row] >> lx) & 1u) != 0u;
        }
        outColor = isText ? zFg : zBg;
        outColor.rgb *= tint.rgb;
        outColor.a *= tint.a;
        return true;
    }

    // 3. Interior Wash
    outColor = zWash;
    outColor.rgb *= tint.rgb;
    outColor.a *= tint.a;
    return true;
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_INDICATOR_SHADER_H_
