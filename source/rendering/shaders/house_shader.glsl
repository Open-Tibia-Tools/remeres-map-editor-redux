// Remere's Map Editor Redux - Modular House Shading Library (GLSL)
// This file contains reusable GLSL procedures for house zoning and house entry rendering.

bool evaluateHouseEntry(vec2 quadCoord, float houseId, uint currentHouseId, int showHouses, out vec4 outColor) {
    if (houseId < 1000000.0) {
        return false;
    }
    if (showHouses == 0) {
        discard;
    }

    uint exitHouseId = uint(houseId - 1000000.0 + 0.5);
    bool isActive = (currentHouseId > 0u && exitHouseId == currentHouseId);

    int lx = clamp(int(floor(quadCoord.x * 32.0)), 0, 31);
    int ly = clamp(int(floor(quadCoord.y * 32.0)), 0, 31);

    int bThick = isActive ? 2 : 1;
    bool isSquareOutline = (lx < bThick || lx >= 32 - bThick || ly < bThick || ly >= 32 - bThick);
    vec4 outlineColor = isActive ? vec4(0.04, 0.15, 0.55, 0.95) : vec4(0.02, 0.10, 0.40, 0.95);
    vec4 bgColor = isActive ? vec4(0.12, 0.45, 0.95, 0.38) : vec4(0.08, 0.25, 0.70, 0.25);

    const uint insideMask[7] = uint[7](
        0x00000000u, 0x0519CA70u, 0x05288E10u, 0x02188A30u, 0x02288A10u, 0x02288A70u, 0x00000000u
    );
    const uint outlineMask[7] = uint[7](
        0x0FBFFFF8u, 0x0AE63588u, 0x0AD771E8u, 0x0DE55548u, 0x055555E8u, 0x05555588u, 0x077DDFF8u
    );

    int row = ly - 12;
    bool isTextInside = false;
    bool isTextOutline = false;
    if (row >= 0 && row < 7) {
        isTextInside = ((insideMask[row] >> lx) & 1u) != 0u;
        isTextOutline = ((outlineMask[row] >> lx) & 1u) != 0u;
    }

    if (isTextInside) {
        outColor = vec4(1.0, 1.0, 1.0, 0.98);
    } else if (isTextOutline) {
        outColor = vec4(0.02, 0.05, 0.15, 0.95);
    } else if (isSquareOutline) {
        outColor = outlineColor;
    } else {
        outColor = bgColor;
    }
    return true;
}

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

    // Unified House Zone Hue Wash (ground and walls share the same background atmosphere)
    vec4 zoneColor = isActive
        ? vec4(0.20, 0.95, 0.20, 0.28)   // Active house: Vibrant Emerald Green
        : vec4(1.00, 0.60, 0.00, 0.20);  // Inactive house: Warm Amber Orange

    fragColor.rgb = mix(fragColor.rgb, zoneColor.rgb, zoneColor.a);

    if (houseId < 0.0) {
        // Base House Shader (ground): centered 'H' emblem (green for active, amber for inactive)
        int lx = p.x % 32; if (lx < 0) lx += 32;
        int ly = p.y % 32; if (ly < 0) ly += 32;
        int dx = abs(lx - 16);
        int dy = abs(ly - 16);

        bool isInside = (dx >= 2 && dx <= 3 && dy <= 5) || (dx < 2 && dy <= 1);
        bool isOutline = (dy == 6 && dx >= 1 && dx <= 4) ||
                         (dx == 4 && dy <= 5) ||
                         (dx == 1 && dy >= 2 && dy <= 5) ||
                         (dx == 0 && dy == 2);

        if (isInside) {
            vec4 insideColor = isActive
                ? vec4(0.55, 1.00, 0.55, 0.95)   // Light neon green
                : vec4(1.00, 0.80, 0.40, 0.95);  // Light amber / gold
            fragColor.rgb = mix(fragColor.rgb, insideColor.rgb, insideColor.a);
        } else if (isOutline) {
            vec4 outlineColor = isActive
                ? vec4(0.04, 0.32, 0.08, 0.95)   // Dark green outline
                : vec4(0.40, 0.20, 0.00, 0.95);  // Dark amber outline
            fragColor.rgb = mix(fragColor.rgb, outlineColor.rgb, outlineColor.a);
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
