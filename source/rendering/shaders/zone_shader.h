#ifndef RME_RENDERING_SHADERS_ZONE_SHADER_H_
#define RME_RENDERING_SHADERS_ZONE_SHADER_H_

#include <string_view>

namespace rme::rendering::shaders {

/**
 * @brief GLSL module providing dedicated on-top zone overlays:
 *        Special Zones (PZ, No-PvP, No-Logout, PvP Zone),
 *        Spawn Radius (clean translucent magenta wash + individual spawn boundary borders),
 *        and Pathing / Blocking (translucent gray wash + bright cyan outer connected borders).
 */
inline constexpr std::string_view ZONE_SHADER_GLSL = R"(
uniform int uShowZoneBorders;
uniform vec4 uZoneBorderColor;
uniform vec4 uPzWash;
uniform vec4 uNpWash;
uniform vec4 uNlWash;
uniform vec4 uPvpWash;
uniform vec4 uBlockingWash;
uniform vec4 uSpawnWash;

void blendOverlayLayer(inout vec4 baseColor, inout bool hasOverlay, vec4 layerColor) {
    if (hasOverlay) {
        baseColor.rgb = mix(baseColor.rgb, layerColor.rgb, layerColor.a);
        baseColor.a = max(baseColor.a, layerColor.a);
    } else {
        baseColor = layerColor;
        hasOverlay = true;
    }
}

bool evaluateClusterBadge(vec2 quadCoord, vec2 quadSize, uint flags, out vec4 outColor) {
    int w = int(quadSize.x);
    int h = int(quadSize.y);
    int lx = clamp(int(floor(quadCoord.x * quadSize.x)), 0, w - 1);
    int ly = clamp(int(floor(quadCoord.y * quadSize.y)), 0, h - 1);

    // Rounded corners: discard the 4 outer corner pixels
    if ((lx == 0 && ly == 0) || (lx == w - 1 && ly == 0) ||
        (lx == 0 && ly == h - 1) || (lx == w - 1 && ly == h - 1)) {
        return false;
    }

    vec4 fg;
    vec4 border;
    if ((flags & 4u) != 0u) {
        // Protection Zone: #2F8BFF
        fg = vec4(0.18, 0.55, 1.00, 0.98);
        border = vec4(0.18, 0.55, 1.00, 0.98);
    } else if ((flags & 8u) != 0u) {
        // Non-PvP: #1FD97A
        fg = vec4(0.12, 0.85, 0.48, 0.98);
        border = vec4(0.12, 0.85, 0.48, 0.98);
    } else if ((flags & 16u) != 0u) {
        // No Logout: #FF850F
        fg = vec4(1.00, 0.52, 0.06, 0.98);
        border = vec4(1.00, 0.52, 0.06, 0.98);
    } else if ((flags & 32u) != 0u) {
        // PvP: #EB1F3D
        fg = vec4(0.92, 0.12, 0.24, 0.98);
        border = vec4(0.92, 0.12, 0.24, 0.98);
    } else {
        return false;
    }

    vec4 bg = vec4(0.06, 0.06, 0.09, 0.95);
    vec4 shadow = vec4(0.02, 0.02, 0.03, 0.80);

    int bThick = (h >= 36) ? 2 : 1;
    bool isEdge = (lx < bThick || lx >= w - bThick || ly < bThick || ly >= h - bThick);
    if (isEdge) {
        outColor = (ly >= h - bThick || lx >= w - bThick) ? shadow : border;
        return true;
    }

    int charH = 10;
    bool isText = false;

    if ((flags & 4u) != 0u) {
        // "Protection Zone" (99px text width)
        int totalW = 99;
        int tx0 = (w - totalW) / 2;
        int ty0 = (h - charH) / 2;
        int rx = lx - tx0;
        int ry = ly - ty0;
        if (ly >= ty0 && ly < ty0 + charH) {
            uint pMask[10] = uint[10](0x7Eu, 0x63u, 0x63u, 0x63u, 0x7Eu, 0x60u, 0x60u, 0x60u, 0x60u, 0x60u); // P (w=7)
            uint rMask[10] = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x1Eu, 0x1Bu, 0x18u, 0x18u, 0x18u, 0x18u); // r (w=5)
            uint oMask[10] = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x1Eu, 0x33u, 0x33u, 0x33u, 0x33u, 0x1Eu); // o (w=6)
            uint tMask[10] = uint[10](0x00u, 0x00u, 0x0Cu, 0x1Fu, 0x0Cu, 0x0Cu, 0x0Cu, 0x0Cu, 0x0Eu, 0x06u); // t (w=5)
            uint eMask[10] = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x1Eu, 0x33u, 0x3Fu, 0x30u, 0x33u, 0x1Eu); // e (w=6)
            uint cMask[10] = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x1Eu, 0x33u, 0x30u, 0x30u, 0x33u, 0x1Eu); // c (w=6)
            uint iMask[10] = uint[10](0x00u, 0x00u, 0x06u, 0x00u, 0x06u, 0x06u, 0x06u, 0x06u, 0x06u, 0x06u); // i (w=3)
            uint nMask[10] = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x36u, 0x3Bu, 0x33u, 0x33u, 0x33u, 0x33u); // n (w=6)
            uint zMask[10] = uint[10](0x7Fu, 0x07u, 0x0Eu, 0x1Cu, 0x38u, 0x38u, 0x70u, 0x70u, 0x78u, 0x7Fu); // Z (w=7)

            if (rx >= 0 && rx < 7)        isText = (((pMask[ry] >> (6 - rx)) & 1u) != 0u);
            else if (rx >= 8 && rx < 13)  isText = (((rMask[ry] >> (4 - (rx - 8))) & 1u) != 0u);
            else if (rx >= 14 && rx < 20) isText = (((oMask[ry] >> (5 - (rx - 14))) & 1u) != 0u);
            else if (rx >= 21 && rx < 26) isText = (((tMask[ry] >> (4 - (rx - 21))) & 1u) != 0u);
            else if (rx >= 27 && rx < 33) isText = (((eMask[ry] >> (5 - (rx - 27))) & 1u) != 0u);
            else if (rx >= 34 && rx < 40) isText = (((cMask[ry] >> (5 - (rx - 34))) & 1u) != 0u);
            else if (rx >= 41 && rx < 46) isText = (((tMask[ry] >> (4 - (rx - 41))) & 1u) != 0u);
            else if (rx >= 47 && rx < 50) isText = (((iMask[ry] >> (2 - (rx - 47))) & 1u) != 0u);
            else if (rx >= 51 && rx < 57) isText = (((oMask[ry] >> (5 - (rx - 51))) & 1u) != 0u);
            else if (rx >= 58 && rx < 64) isText = (((nMask[ry] >> (5 - (rx - 58))) & 1u) != 0u);
            else if (rx >= 70 && rx < 77) isText = (((zMask[ry] >> (6 - (rx - 70))) & 1u) != 0u);
            else if (rx >= 78 && rx < 84) isText = (((oMask[ry] >> (5 - (rx - 78))) & 1u) != 0u);
            else if (rx >= 85 && rx < 91) isText = (((nMask[ry] >> (5 - (rx - 85))) & 1u) != 0u);
            else if (rx >= 92 && rx < 98) isText = (((eMask[ry] >> (5 - (rx - 92))) & 1u) != 0u);
        }
    } else if ((flags & 8u) != 0u) {
        // "Non-PvP" (50px text width)
        int totalW = 50;
        int tx0 = (w - totalW) / 2;
        int ty0 = (h - charH) / 2;
        int rx = lx - tx0;
        int ry = ly - ty0;
        if (ly >= ty0 && ly < ty0 + charH) {
            uint nCapMask[10] = uint[10](0x63u, 0x73u, 0x73u, 0x5Bu, 0x4Fu, 0x4Fu, 0x67u, 0x63u, 0x63u, 0x63u); // N (w=7)
            uint oMask[10]    = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x1Eu, 0x33u, 0x33u, 0x33u, 0x33u, 0x1Eu); // o (w=6)
            uint nMask[10]    = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x36u, 0x3Bu, 0x33u, 0x33u, 0x33u, 0x33u); // n (w=6)
            uint dashMask[10] = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x1Fu, 0x1Fu, 0x00u, 0x00u, 0x00u); // - (w=5)
            uint pMask[10]    = uint[10](0x7Eu, 0x63u, 0x63u, 0x63u, 0x7Eu, 0x60u, 0x60u, 0x60u, 0x60u, 0x60u); // P (w=7)
            uint vMask[10]    = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x33u, 0x33u, 0x33u, 0x1Eu, 0x1Eu, 0x0Cu); // v (w=6)

            if (rx >= 0 && rx < 7)        isText = (((nCapMask[ry] >> (6 - rx)) & 1u) != 0u);
            else if (rx >= 8 && rx < 14)  isText = (((oMask[ry] >> (5 - (rx - 8))) & 1u) != 0u);
            else if (rx >= 15 && rx < 21) isText = (((nMask[ry] >> (5 - (rx - 15))) & 1u) != 0u);
            else if (rx >= 22 && rx < 27) isText = (((dashMask[ry] >> (4 - (rx - 22))) & 1u) != 0u);
            else if (rx >= 28 && rx < 35) isText = (((pMask[ry] >> (6 - (rx - 28))) & 1u) != 0u);
            else if (rx >= 36 && rx < 42) isText = (((vMask[ry] >> (5 - (rx - 36))) & 1u) != 0u);
            else if (rx >= 43 && rx < 50) isText = (((pMask[ry] >> (6 - (rx - 43))) & 1u) != 0u);
        }
    } else if ((flags & 16u) != 0u) {
        // "No Logout" (61px text width)
        int totalW = 61;
        int tx0 = (w - totalW) / 2;
        int ty0 = (h - charH) / 2;
        int rx = lx - tx0;
        int ry = ly - ty0;
        if (ly >= ty0 && ly < ty0 + charH) {
            uint nCapMask[10] = uint[10](0x63u, 0x73u, 0x73u, 0x5Bu, 0x4Fu, 0x4Fu, 0x67u, 0x63u, 0x63u, 0x63u); // N (w=7)
            uint oMask[10]    = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x1Eu, 0x33u, 0x33u, 0x33u, 0x33u, 0x1Eu); // o (w=6)
            uint lMask[10]    = uint[10](0x30u, 0x30u, 0x30u, 0x30u, 0x30u, 0x30u, 0x30u, 0x30u, 0x3Fu, 0x3Fu); // L (w=6)
            uint gMask[10]    = uint[10](0x00u, 0x00u, 0x00u, 0x1Eu, 0x33u, 0x33u, 0x1Fu, 0x03u, 0x33u, 0x1Eu); // g (w=6)
            uint uMask[10]    = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x33u, 0x33u, 0x33u, 0x33u, 0x37u, 0x1Eu); // u (w=6)
            uint tMask[10]    = uint[10](0x00u, 0x00u, 0x0Cu, 0x1Fu, 0x0Cu, 0x0Cu, 0x0Cu, 0x0Cu, 0x0Eu, 0x06u); // t (w=5)

            if (rx >= 0 && rx < 7)        isText = (((nCapMask[ry] >> (6 - rx)) & 1u) != 0u);
            else if (rx >= 8 && rx < 14)  isText = (((oMask[ry] >> (5 - (rx - 8))) & 1u) != 0u);
            else if (rx >= 20 && rx < 26) isText = (((lMask[ry] >> (5 - (rx - 20))) & 1u) != 0u);
            else if (rx >= 27 && rx < 33) isText = (((oMask[ry] >> (5 - (rx - 27))) & 1u) != 0u);
            else if (rx >= 34 && rx < 40) isText = (((gMask[ry] >> (5 - (rx - 34))) & 1u) != 0u);
            else if (rx >= 41 && rx < 47) isText = (((oMask[ry] >> (5 - (rx - 41))) & 1u) != 0u);
            else if (rx >= 48 && rx < 54) isText = (((uMask[ry] >> (5 - (rx - 48))) & 1u) != 0u);
            else if (rx >= 55 && rx < 60) isText = (((tMask[ry] >> (4 - (rx - 55))) & 1u) != 0u);
        }
    } else if ((flags & 32u) != 0u) {
        // "PvP" (22px text width)
        int totalW = 22;
        int tx0 = (w - totalW) / 2;
        int ty0 = (h - charH) / 2;
        int rx = lx - tx0;
        int ry = ly - ty0;
        if (ly >= ty0 && ly < ty0 + charH) {
            uint pMask[10] = uint[10](0x7Eu, 0x63u, 0x63u, 0x63u, 0x7Eu, 0x60u, 0x60u, 0x60u, 0x60u, 0x60u); // P (w=7)
            uint vMask[10] = uint[10](0x00u, 0x00u, 0x00u, 0x00u, 0x33u, 0x33u, 0x33u, 0x1Eu, 0x1Eu, 0x0Cu); // v (w=6)

            if (rx >= 0 && rx < 7)        isText = (((pMask[ry] >> (6 - rx)) & 1u) != 0u);
            else if (rx >= 8 && rx < 14)  isText = (((vMask[ry] >> (5 - (rx - 8))) & 1u) != 0u);
            else if (rx >= 15 && rx < 22) isText = (((pMask[ry] >> (6 - (rx - 15))) & 1u) != 0u);
        }
    }

    outColor = isText ? fg : bg;
    return true;
}

bool evaluateSpecialZones(uint flags, bool bNorthOuter, bool bSouthOuter, bool bWestOuter, bool bEastOuter, int lx, int ly, out vec4 outLayer) {
    bool hasZone = ((flags & 60u) != 0u);
    if (!hasZone) {
        return false;
    }

    int tile_lx = lx % 32;
    int tile_ly = ly % 32;

    // 1. GLOBAL OUTER OUTLINE
    // Only applied where neighbor is NOT a special zone and zone borders are enabled!
    if (uShowZoneBorders != 0) {
        if ((bNorthOuter && (tile_ly == 0 || tile_ly == 1)) ||
            (bSouthOuter && (tile_ly == 31 || tile_ly == 30)) ||
            (bWestOuter && (tile_lx == 0 || tile_lx == 1)) ||
            (bEastOuter && (tile_lx == 31 || tile_lx == 30))) {
            outLayer = uZoneBorderColor;
            return true;
        }
    }

    // 2. INTERIOR CLEAN TRANSLUCENT WASH + DIAGONAL TRIANGLE PARTITIONING
    bool hasPz  = ((flags & 4u) != 0u);
    bool hasNp  = ((flags & 8u) != 0u);
    bool hasNl  = ((flags & 16u) != 0u);
    bool hasPvp = ((flags & 32u) != 0u);

    vec4 activeWashes[4];
    int count = 0;
    if (hasPz)  activeWashes[count++] = uPzWash;
    if (hasNp)  activeWashes[count++] = uNpWash;
    if (hasNl)  activeWashes[count++] = uNlWash;
    if (hasPvp) activeWashes[count++] = uPvpWash;

    if (count == 1) {
        outLayer = activeWashes[0];
    } else if (count == 2) {
        // Consistent diagonal halves (triangles):
        // Top-Left triangle vs Bottom-Right triangle
        outLayer = (tile_lx + tile_ly < 31) ? activeWashes[0] : activeWashes[1];
    } else if (count == 3) {
        // 3 triangles: Top-Left half is Zone 0; Bottom-Right half split diagonally into Zone 1 & 2
        if (tile_lx + tile_ly < 31) {
            outLayer = activeWashes[0];
        } else if (tile_lx >= tile_ly) {
            outLayer = activeWashes[1];
        } else {
            outLayer = activeWashes[2];
        }
    } else {
        // 4 zones: symmetrical 4-triangle X meeting at center
        bool diag1 = (tile_ly <= tile_lx);
        bool diag2 = (tile_lx + tile_ly <= 31);
        if (diag1 && diag2) {
            outLayer = activeWashes[0]; // North triangle
        } else if (diag1 && !diag2) {
            outLayer = activeWashes[1]; // East triangle
        } else if (!diag1 && !diag2) {
            outLayer = activeWashes[2]; // South triangle
        } else {
            outLayer = activeWashes[3]; // West triangle
        }
    }

    return true;
}

bool evaluateSpawnOverlay(uint flags, bool bNorth, bool bSouth, bool bWest, bool bEast, int lx, int ly, int maxX, int maxY, out vec4 outLayer) {
    if ((flags & 2u) == 0u) {
        return false;
    }

    // 1. GLOBAL OUTER OUTLINE on perimeter (when zone borders are enabled)
    if (uShowZoneBorders != 0) {
        if ((bNorth && (ly == 0 || ly == 1)) ||
            (bSouth && (ly == maxY || ly == maxY - 1)) ||
            (bWest  && (lx == 0 || lx == 1)) ||
            (bEast  && (lx == maxX || lx == maxX - 1))) {
            outLayer = uZoneBorderColor;
            return true;
        }
    }

    // 2. Interior Clean Translucent Wash
    outLayer = uSpawnWash;
    return true;
}

bool evaluateBlockingOverlay(uint flags, bool bNorth, bool bSouth, bool bWest, bool bEast, int lx, int ly, out vec4 outLayer) {
    if ((flags & 1u) == 0u) {
        return false;
    }

    int tile_lx = lx % 32;
    int tile_ly = ly % 32;

    // 1. GLOBAL OUTER OUTLINE
    // Only applied where neighbor is NOT blocking and zone borders are enabled!
    if (uShowZoneBorders != 0) {
        if ((bNorth && (tile_ly == 0 || tile_ly == 1)) ||
            (bSouth && (tile_ly == 31 || tile_ly == 30)) ||
            (bWest && (tile_lx == 0 || tile_lx == 1)) ||
            (bEast && (tile_lx == 31 || tile_lx == 30))) {
            outLayer = uZoneBorderColor;
            return true;
        }
    }

    // 2. Interior Clean Translucent Wash
    outLayer = uBlockingWash;
    return true;
}

bool evaluateZoneOverlay(vec2 worldPos, vec2 quadCoord, vec2 quadSize, float zoneFlags,
                         int showBlocking, int showSpawns, int showSpecialTiles,
                         out vec4 outColor) {
    if (zoneFlags < 0.5) {
        return false;
    }

    uint flags = uint(zoneFlags + 0.5);

    // Dedicated cluster badge indicator quad (Bit 22 = 4194304u)
    if ((flags & 4194304u) != 0u) {
        if (showSpecialTiles == 0) {
            return false;
        }
        return evaluateClusterBadge(quadCoord, quadSize, flags, outColor);
    }

    int maxX = int(max(quadSize.x - 1.0, 0.0));
    int maxY = int(max(quadSize.y - 1.0, 0.0));
    int lx = clamp(int(floor(quadCoord.x * quadSize.x)), 0, maxX);
    int ly = clamp(int(floor(quadCoord.y * quadSize.y)), 0, maxY);

    vec4 color = vec4(0.0);
    bool hasOverlay = false;
    vec4 layer;

    // 1. Special Zones (showSpecialTiles) with continuous SDF rounded silhouette & sunlight color grading
    bool bNorthZone = (flags & 1024u) != 0u;
    bool bSouthZone = (flags & 2048u) != 0u;
    bool bWestZone  = (flags & 4096u) != 0u;
    bool bEastZone  = (flags & 8192u) != 0u;

    if (showSpecialTiles != 0 && evaluateSpecialZones(flags, bNorthZone, bSouthZone, bWestZone, bEastZone, lx, ly, layer)) {
        blendOverlayLayer(color, hasOverlay, layer);
    }

    // 2. Spawn Radius (showSpawns) with continuous SDF rounded silhouette & magenta sunlight lift
    bool bNorthSpawn = (ly <= 1) && (flags & 16384u) != 0u;
    bool bSouthSpawn = (ly >= maxY - 1) && (flags & 32768u) != 0u;
    bool bWestSpawn  = (lx <= 1) && (flags & 65536u) != 0u;
    bool bEastSpawn  = (lx >= maxX - 1) && (flags & 131072u) != 0u;

    if (showSpawns != 0 && evaluateSpawnOverlay(flags, bNorthSpawn, bSouthSpawn, bWestSpawn, bEastSpawn, lx, ly, maxX, maxY, layer)) {
        blendOverlayLayer(color, hasOverlay, layer);
    }

    // 3. Pathing / Blocking (showBlocking) with continuous SDF rounded silhouette & cyan lift
    bool bNorthBlock = (flags & 64u) != 0u;
    bool bSouthBlock = (flags & 128u) != 0u;
    bool bWestBlock  = (flags & 256u) != 0u;
    bool bEastBlock  = (flags & 512u) != 0u;

    if (showBlocking != 0 && evaluateBlockingOverlay(flags, bNorthBlock, bSouthBlock, bWestBlock, bEastBlock, lx, ly, layer)) {
        blendOverlayLayer(color, hasOverlay, layer);
    }

    if (!hasOverlay) {
        return false;
    }

    outColor = color;
    return true;
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_ZONE_SHADER_H_
