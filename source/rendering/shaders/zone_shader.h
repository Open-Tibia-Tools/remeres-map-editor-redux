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
void blendOverlayLayer(inout vec4 baseColor, inout bool hasOverlay, vec4 layerColor) {
    if (hasOverlay) {
        baseColor.rgb = mix(baseColor.rgb, layerColor.rgb, layerColor.a);
        baseColor.a = max(baseColor.a, layerColor.a);
    } else {
        baseColor = layerColor;
        hasOverlay = true;
    }
}

bool evaluateSpecialZones(uint flags, bool bNorthOuter, bool bSouthOuter, bool bWestOuter, bool bEastOuter, int lx, int ly, out vec4 outLayer) {
    bool hasZone = ((flags & 60u) != 0u);
    if (!hasZone) {
        return false;
    }

    int tile_lx = lx % 32;
    int tile_ly = ly % 32;

    // Define colors for each zone:
    // zWash:  Base translucent wash (Vanilla RME style, ~28% alpha)
    // zDark:  3D shadow bevel color (darker shade of the zone color)
    // zLight: 3D highlight bevel color (lighter tint of the zone color)
    vec4 zWash  = vec4(0.0);
    vec4 zDark  = vec4(0.0);
    vec4 zLight = vec4(0.0);

    if ((flags & 4u) != 0u) {
        // Protection Zone: Golden Yellow
        zWash  = vec4(1.00, 0.88, 0.12, 0.28);
        zDark  = vec4(0.55, 0.40, 0.00, 0.95);
        zLight = vec4(1.00, 0.98, 0.70, 0.85);
    } else if ((flags & 8u) != 0u) {
        // No-PvP: Emerald Green
        zWash  = vec4(0.12, 0.85, 0.24, 0.26);
        zDark  = vec4(0.00, 0.38, 0.08, 0.95);
        zLight = vec4(0.72, 1.00, 0.78, 0.85);
    } else if ((flags & 16u) != 0u) {
        // No-Logout: Warm Orange
        zWash  = vec4(1.00, 0.52, 0.06, 0.28);
        zDark  = vec4(0.52, 0.18, 0.00, 0.95);
        zLight = vec4(1.00, 0.78, 0.50, 0.85);
    } else if ((flags & 32u) != 0u) {
        // PvP Zone: Crimson Red
        zWash  = vec4(0.92, 0.12, 0.24, 0.28);
        zDark  = vec4(0.44, 0.02, 0.08, 0.95);
        zLight = vec4(1.00, 0.60, 0.68, 0.85);
    }

    vec4 zBlack = vec4(0.05, 0.05, 0.07, 0.98); // 2px solid black global outer outline

    // 1. GLOBAL OUTER OUTLINE (2px solid black)
    // Only applied where neighbor is NOT a special zone!
    if (bNorthOuter && (tile_ly == 0 || tile_ly == 1)) {
        outLayer = zBlack;
        return true;
    }
    if (bSouthOuter && (tile_ly == 31 || tile_ly == 30)) {
        outLayer = zBlack;
        return true;
    }
    if (bWestOuter && (tile_lx == 0 || tile_lx == 1)) {
        outLayer = zBlack;
        return true;
    }
    if (bEastOuter && (tile_lx == 31 || tile_lx == 30)) {
        outLayer = zBlack;
        return true;
    }

    // 2. GLOBAL OUTER 3D BEVEL (Inside the 2px black border)
    // North & West inner edge: 1px light highlight
    if (bNorthOuter && tile_ly == 2) {
        outLayer = zLight;
        return true;
    }
    if (bWestOuter && tile_lx == 2) {
        outLayer = zLight;
        return true;
    }
    // South & East inner edge: 2px darker shade shadow
    if (bSouthOuter && (tile_ly == 29 || tile_ly == 28)) {
        outLayer = zDark;
        return true;
    }
    if (bEastOuter && (tile_lx == 29 || tile_lx == 28)) {
        outLayer = zDark;
        return true;
    }

    // 3. INTERNAL INTER-ZONE 3D BEVEL (NO BLACK!)
    // Where different special zones meet:
    bool bNorthInner = (flags & 262144u) != 0u;
    bool bSouthInner = (flags & 524288u) != 0u;
    bool bWestInner  = (flags & 1048576u) != 0u;
    bool bEastInner  = (flags & 2097152u) != 0u;

    if (bNorthInner && tile_ly == 0) {
        outLayer = zLight;
        return true;
    }
    if (bWestInner && tile_lx == 0) {
        outLayer = zLight;
        return true;
    }
    if (bSouthInner && tile_ly == 31) {
        outLayer = zDark;
        return true;
    }
    if (bEastInner && tile_lx == 31) {
        outLayer = zDark;
        return true;
    }

    // 4. INTERIOR: Clean translucent color wash (Vanilla RME style)
    outLayer = zWash;
    return true;
}

bool evaluateSpawnOverlay(uint flags, bool bNorth, bool bSouth, bool bWest, bool bEast, int lx, int ly, int maxX, int maxY, out vec4 outLayer) {
    if ((flags & 2u) == 0u) {
        return false;
    }

    vec4 sBorder = vec4(1.00, 0.20, 1.00, 0.95);
    vec4 sShadow = vec4(0.04, 0.04, 0.06, 0.95);
    vec4 sSunlight = vec4(1.00, 0.20, 1.00, 0.09);

    float fMaxX = float(maxX);
    float fMaxY = float(maxY);

    // Rounded outer corners (radius = 5.0 px)
    if (bNorth && bWest && lx < 6 && ly < 6) {
        float d = length(vec2(5.0 - float(lx), 5.0 - float(ly)));
        if (d > 5.8) return false;
        if (d >= 4.5) { outLayer = sShadow; return true; }
        if (d >= 3.2) { outLayer = sBorder; return true; }
        outLayer = sSunlight; return true;
    }
    if (bNorth && bEast && lx >= maxX - 5 && ly < 6) {
        float d = length(vec2(float(lx) - (fMaxX - 5.0), 5.0 - float(ly)));
        if (d > 5.8) return false;
        if (d >= 4.5) { outLayer = sShadow; return true; }
        if (d >= 3.2) { outLayer = sBorder; return true; }
        outLayer = sSunlight; return true;
    }
    if (bSouth && bWest && lx < 6 && ly >= maxY - 5) {
        float d = length(vec2(5.0 - float(lx), float(ly) - (fMaxY - 5.0)));
        if (d > 5.8) return false;
        if (d >= 4.5) { outLayer = sShadow; return true; }
        if (d >= 3.2) { outLayer = sBorder; return true; }
        outLayer = sSunlight; return true;
    }
    if (bSouth && bEast && lx >= maxX - 5 && ly >= maxY - 5) {
        float d = length(vec2(float(lx) - (fMaxX - 5.0), float(ly) - (fMaxY - 5.0)));
        if (d > 5.8) return false;
        if (d >= 4.5) { outLayer = sShadow; return true; }
        if (d >= 3.2) { outLayer = sBorder; return true; }
        outLayer = sSunlight; return true;
    }

    // Straight perimeter edges
    if (bNorth && ly == 0) { outLayer = sShadow; return true; }
    if (bNorth && ly == 1) { outLayer = sBorder; return true; }
    if (bSouth && ly == maxY) { outLayer = sShadow; return true; }
    if (bSouth && ly == maxY - 1) { outLayer = sBorder; return true; }
    if (bWest  && lx == 0) { outLayer = sShadow; return true; }
    if (bWest  && lx == 1) { outLayer = sBorder; return true; }
    if (bEast  && lx == maxX) { outLayer = sShadow; return true; }
    if (bEast  && lx == maxX - 1) { outLayer = sBorder; return true; }

    outLayer = sSunlight;
    return true;
}

bool evaluateBlockingOverlay(uint flags, bool bNorth, bool bSouth, bool bWest, bool bEast, int lx, int ly, out vec4 outLayer) {
    if ((flags & 1u) == 0u) {
        return false;
    }

    int tile_lx = lx % 32;
    int tile_ly = ly % 32;

    vec4 bBorder = vec4(0.00, 0.95, 1.00, 0.95);
    vec4 bShadow = vec4(0.04, 0.04, 0.06, 0.95);
    vec4 bSunlight = vec4(0.00, 0.95, 1.00, 0.09);

    // Rounded outer corners (radius = 3.5 px)
    if (bNorth && bWest && tile_lx < 4 && tile_ly < 4) {
        float d = length(vec2(3.5 - float(tile_lx), 3.5 - float(tile_ly)));
        if (d > 4.3) return false;
        if (d >= 3.0) { outLayer = bShadow; return true; }
        if (d >= 1.9) { outLayer = bBorder; return true; }
        outLayer = bSunlight; return true;
    }
    if (bNorth && bEast && tile_lx >= 28 && tile_ly < 4) {
        float d = length(vec2(float(tile_lx) - 27.5, 3.5 - float(tile_ly)));
        if (d > 4.3) return false;
        if (d >= 3.0) { outLayer = bShadow; return true; }
        if (d >= 1.9) { outLayer = bBorder; return true; }
        outLayer = bSunlight; return true;
    }
    if (bSouth && bWest && tile_lx < 4 && tile_ly >= 28) {
        float d = length(vec2(3.5 - float(tile_lx), float(tile_ly) - 27.5));
        if (d > 4.3) return false;
        if (d >= 3.0) { outLayer = bShadow; return true; }
        if (d >= 1.9) { outLayer = bBorder; return true; }
        outLayer = bSunlight; return true;
    }
    if (bSouth && bEast && tile_lx >= 28 && tile_ly >= 28) {
        float d = length(vec2(float(tile_lx) - 27.5, float(tile_ly) - 27.5));
        if (d > 4.3) return false;
        if (d >= 3.0) { outLayer = bShadow; return true; }
        if (d >= 1.9) { outLayer = bBorder; return true; }
        outLayer = bSunlight; return true;
    }

    // Straight perimeter edges
    if (bNorth && tile_ly == 0) { outLayer = bShadow; return true; }
    if (bNorth && tile_ly == 1) { outLayer = bBorder; return true; }
    if (bSouth && tile_ly == 31) { outLayer = bShadow; return true; }
    if (bSouth && tile_ly == 30) { outLayer = bBorder; return true; }
    if (bWest  && tile_lx == 0) { outLayer = bShadow; return true; }
    if (bWest  && tile_lx == 1) { outLayer = bBorder; return true; }
    if (bEast  && tile_lx == 31) { outLayer = bShadow; return true; }
    if (bEast  && tile_lx == 30) { outLayer = bBorder; return true; }

    outLayer = bSunlight;
    return true;
}

bool evaluateZoneOverlay(vec2 worldPos, vec2 quadCoord, vec2 quadSize, float zoneFlags,
                         int showBlocking, int showSpawns, int showSpecialTiles,
                         out vec4 outColor) {
    if (zoneFlags < 0.5) {
        return false;
    }

    uint flags = uint(zoneFlags + 0.5);
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
