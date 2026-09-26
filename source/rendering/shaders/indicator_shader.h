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
 */
inline constexpr std::string_view INDICATOR_SHADER_GLSL = R"(
bool evaluateTileIndicator(vec2 quadCoord, float markerId, uint currentHouseId,
                           int showHouses, int showSpawns, int showTowns,
                           int showWaypoints, int showTechItems,
                           out vec4 outColor) {
    if (markerId < 1000000.0) {
        return false;
    }

    int lx = clamp(int(floor(quadCoord.x * 32.0)), 0, 31);
    int ly = clamp(int(floor(quadCoord.y * 32.0)), 0, 31);

    vec4 outlineColor;
    vec4 bgColor;
    vec4 textOutlineColor = vec4(0.02, 0.05, 0.15, 0.95);
    int bThick = 1;
    uint inMask[7];
    uint outMask[7];

    if (markerId < 2000000.0) {
        // House Entry ("ENTRY")
        if (showHouses == 0) discard;
        uint exitHouseId = uint(markerId - 1000000.0 + 0.5);
        bool isActive = (currentHouseId > 0u && exitHouseId == currentHouseId);
        outlineColor = isActive ? vec4(0.25, 0.65, 1.00, 1.0) : vec4(0.15, 0.50, 1.00, 0.95);
        bgColor = isActive ? vec4(0.12, 0.45, 0.95, 0.38) : vec4(0.08, 0.30, 0.80, 0.28);
        textOutlineColor = vec4(0.02, 0.10, 0.30, 0.95);
        inMask = uint[7](0x00000000u, 0x0519CA70u, 0x05288E10u, 0x02188A30u, 0x02288A10u, 0x02288A70u, 0x00000000u);
        outMask = uint[7](0x0FBFFFF8u, 0x0AE63588u, 0x0AD771E8u, 0x0DE55548u, 0x055555E8u, 0x05555588u, 0x077DDFF8u);
    } else if (markerId < 3000000.0) {
        // Spawn Center ("SPAWN")
        if (showSpawns == 0) discard;
        outlineColor = vec4(1.00, 0.20, 1.00, 1.0);
        bgColor = vec4(0.85, 0.15, 0.85, 0.35);
        textOutlineColor = vec4(0.25, 0.02, 0.25, 0.95);
        inMask = uint[7](0x0944C770u, 0x0B452908u, 0x0D452908u, 0x0955E730u, 0x09552140u, 0x096D2140u, 0x09452138u);
        outMask = uint[7](0x16AB388Cu, 0x14AAD6F4u, 0x12BAD6F4u, 0x16AA18CCu, 0x16AADEB8u, 0x1692D2BCu, 0x16BAD2C4u);
    } else if (markerId < 4000000.0) {
        // Town Temple ("TOWN")
        if (showTowns == 0) discard;
        outlineColor = vec4(1.00, 0.85, 0.00, 1.0);
        bgColor = vec4(1.00, 0.75, 0.10, 0.35);
        textOutlineColor = vec4(0.30, 0.15, 0.00, 0.95);
        inMask = uint[7](0x025133E0u, 0x02D14880u, 0x03514880u, 0x02554880u, 0x02554880u, 0x025B4880u, 0x02513080u);
        outMask = uint[7](0x05AACC10u, 0x052AB770u, 0x04AEB540u, 0x05AAB540u, 0x05AAB540u, 0x05A4B540u, 0x05AECD40u);
    } else if (markerId < 5000000.0) {
        // Waypoint ("WAYPT")
        if (showWaypoints == 0) discard;
        outlineColor = vec4(0.00, 1.00, 1.00, 1.0);
        bgColor = vec4(0.05, 0.80, 0.85, 0.35);
        textOutlineColor = vec4(0.00, 0.20, 0.25, 0.95);
        inMask = uint[7](0x1F3A4C88u, 0x044A5288u, 0x044A5288u, 0x04399EA8u, 0x040912A8u, 0x040912D8u, 0x04091288u);
        outMask = uint[7](0x20C5B354u, 0x3BB5AD54u, 0x0AB5AD74u, 0x0AC66154u, 0x0A76ED54u, 0x0A16AD24u, 0x0A16AD74u);
    } else if (markerId < 6000000.0) {
        // Invisible Stairs ("STAIR")
        if (showTechItems == 0) discard;
        outlineColor = vec4(1.00, 0.95, 0.10, 1.0);
        bgColor = vec4(1.00, 0.90, 0.15, 0.35);
        textOutlineColor = vec4(0.30, 0.25, 0.00, 0.95);
        inMask = uint[7](0x07733EE0u, 0x09248810u, 0x09248810u, 0x07278860u, 0x05248880u, 0x09248880u, 0x09748870u);
        outMask = uint[7](0x188CC118u, 0x16DB77E8u, 0x16DB54E8u, 0x18D85598u, 0x1ADB5570u, 0x16DB5578u, 0x168B5588u);
    } else if (markerId < 7000000.0) {
        // Invisible Walkable ("WALK") - Cyan
        if (showTechItems == 0) discard;
        outlineColor = vec4(0.00, 0.95, 0.95, 1.0);
        bgColor = vec4(0.05, 0.75, 0.85, 0.35);
        textOutlineColor = vec4(0.00, 0.20, 0.25, 0.95);
        inMask = uint[7](0x02426440u, 0x01429440u, 0x00C29440u, 0x00C2F540u, 0x01429540u, 0x024296C0u, 0x025E9440u);
        outMask = uint[7](0x05A59AA0u, 0x06A56AA0u, 0x03256BA0u, 0x03250AA0u, 0x06A56AA0u, 0x05BD6920u, 0x05A16BA0u);
    } else if (markerId < 8000000.0) {
        // Invisible Wall ("BLOCK") - Red
        if (showTechItems == 0) discard;
        outlineColor = vec4(1.00, 0.15, 0.15, 1.0);
        bgColor = vec4(0.95, 0.20, 0.20, 0.35);
        textOutlineColor = vec4(0.30, 0.02, 0.02, 0.95);
        inMask = uint[7](0x09718270u, 0x050A4290u, 0x030A4290u, 0x030A4270u, 0x050A4290u, 0x090A4290u, 0x09719E70u);
        outMask = uint[7](0x168E6588u, 0x1AF5A568u, 0x0C95A568u, 0x0C95A588u, 0x1A95A568u, 0x16F5BD68u, 0x168E6188u);
    } else {
        // Primal Light ("LIGHT")
        if (showTechItems == 0) discard;
        outlineColor = vec4(0.35, 0.85, 1.00, 1.0);
        bgColor = vec4(0.30, 0.75, 1.00, 0.35);
        textOutlineColor = vec4(0.05, 0.15, 0.35, 0.95);
        inMask = uint[7](0x0FA5CE10u, 0x02242410u, 0x02242410u, 0x023DA410u, 0x02252410u, 0x02252410u, 0x0225CEF0u);
        outMask = uint[7](0x105A3128u, 0x1DDBDB28u, 0x055BDA28u, 0x05425A28u, 0x055ADA28u, 0x055ADBE8u, 0x055A3108u);
    }

    bool isSquareOutline = (lx < bThick || lx >= 32 - bThick || ly < bThick || ly >= 32 - bThick);

    int row = ly - 12;
    bool isTextInside = false;
    bool isTextOutline = false;
    if (row >= 0 && row < 7) {
        isTextInside = ((inMask[row] >> lx) & 1u) != 0u;
        isTextOutline = ((outMask[row] >> lx) & 1u) != 0u;
    }

    if (isTextInside) {
        outColor = vec4(1.0, 1.0, 1.0, 0.98);
    } else if (isTextOutline) {
        outColor = textOutlineColor;
    } else if (isSquareOutline) {
        outColor = outlineColor;
    } else {
        outColor = bgColor;
    }
    return true;
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_INDICATOR_SHADER_H_
