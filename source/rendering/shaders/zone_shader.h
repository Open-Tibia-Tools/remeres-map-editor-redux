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

bool evaluateSpecialZones(uint flags, bool bNorth, bool bSouth, bool bWest, bool bEast, out vec4 outLayer) {
    vec4 zWash = vec4(0.0);
    vec4 zBorder = vec4(0.0);
    bool hasZone = false;

    if ((flags & 4u) != 0u) {
        // PZ: Vibrant emerald green wash + border
        hasZone = true;
        zWash = vec4(0.15, 0.90, 0.20, 0.28);
        zBorder = vec4(0.20, 1.00, 0.30, 0.95);
    } else if ((flags & 8u) != 0u) {
        // No-PvP: Golden yellow wash + border
        hasZone = true;
        zWash = vec4(0.95, 0.85, 0.10, 0.28);
        zBorder = vec4(1.00, 0.90, 0.10, 0.95);
    } else if ((flags & 16u) != 0u) {
        // No-Logout: Warm orange wash + border
        hasZone = true;
        zWash = vec4(1.00, 0.50, 0.05, 0.28);
        zBorder = vec4(1.00, 0.55, 0.10, 0.95);
    } else if ((flags & 32u) != 0u) {
        // PvP Zone: Crimson red wash + border
        hasZone = true;
        zWash = vec4(0.85, 0.05, 0.25, 0.28);
        zBorder = vec4(1.00, 0.15, 0.30, 0.95);
    }

    if (!hasZone) {
        return false;
    }

    bool isBorder = (bNorth && (flags & 1024u) != 0u) ||
                    (bSouth && (flags & 2048u) != 0u) ||
                    (bWest  && (flags & 4096u) != 0u) ||
                    (bEast  && (flags & 8192u) != 0u);
    outLayer = isBorder ? zBorder : zWash;
    return true;
}

bool evaluateSpawnOverlay(uint flags, bool bNorth, bool bSouth, bool bWest, bool bEast, out vec4 outLayer) {
    if ((flags & 2u) == 0u) {
        return false;
    }
    vec4 spawnWash = vec4(0.85, 0.15, 0.85, 0.25);
    vec4 spawnBorder = vec4(1.00, 0.20, 1.00, 0.95);
    bool isBorder = (bNorth && (flags & 16384u) != 0u) ||
                    (bSouth && (flags & 32768u) != 0u) ||
                    (bWest  && (flags & 65536u) != 0u) ||
                    (bEast  && (flags & 131072u) != 0u);
    outLayer = isBorder ? spawnBorder : spawnWash;
    return true;
}

bool evaluateBlockingOverlay(uint flags, bool bNorth, bool bSouth, bool bWest, bool bEast, out vec4 outLayer) {
    if ((flags & 1u) == 0u) {
        return false;
    }
    bool isBorder = (bNorth && (flags & 64u) != 0u) ||
                    (bSouth && (flags & 128u) != 0u) ||
                    (bWest  && (flags & 256u) != 0u) ||
                    (bEast  && (flags & 512u) != 0u);
    outLayer = isBorder ? vec4(0.00, 0.95, 1.00, 0.95) : vec4(0.40, 0.40, 0.40, 0.35);
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

    bool bNorth = (ly == 0);
    bool bSouth = (ly == maxY);
    bool bWest  = (lx == 0);
    bool bEast  = (lx == maxX);

    vec4 color = vec4(0.0);
    bool hasOverlay = false;
    vec4 layer;

    // 1. Special Zones (showSpecialTiles)
    if (showSpecialTiles != 0 && evaluateSpecialZones(flags, bNorth, bSouth, bWest, bEast, layer)) {
        blendOverlayLayer(color, hasOverlay, layer);
    }

    // 2. Spawn Radius (showSpawns)
    if (showSpawns != 0 && evaluateSpawnOverlay(flags, bNorth, bSouth, bWest, bEast, layer)) {
        blendOverlayLayer(color, hasOverlay, layer);
    }

    // 3. Pathing / Blocking (showBlocking)
    if (showBlocking != 0 && evaluateBlockingOverlay(flags, bNorth, bSouth, bWest, bEast, layer)) {
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
