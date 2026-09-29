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

bool evaluateSpecialZones(uint flags, int lx, int ly, out vec4 outLayer) {
    bool hasZone = ((flags & 60u) != 0u);
    if (!hasZone) {
        return false;
    }

    int tile_lx = lx % 32;
    int tile_ly = ly % 32;

    // 1. Dedicated 4-corner micro-badges (High-contrast 2-tone with dark shadow)
    // Top-Left: [PZ] (Yellow)
    if ((flags & 4u) != 0u && tile_lx >= 1 && tile_lx <= 9 && tile_ly >= 1 && tile_ly <= 7) {
        int r = tile_ly - 2;
        int c = tile_lx - 2;
        uint pzMask[5] = uint[5](0x77u, 0x51u, 0x72u, 0x44u, 0x47u);
        bool isText = (r >= 0 && r < 5 && c >= 0 && c < 7) && (((pzMask[r] >> (6 - c)) & 1u) != 0u);
        outLayer = isText ? vec4(1.00, 0.90, 0.10, 0.98) : vec4(0.06, 0.06, 0.08, 0.92);
        return true;
    }

    // Top-Right: [NP] (Emerald Green)
    if ((flags & 8u) != 0u && tile_lx >= 22 && tile_lx <= 30 && tile_ly >= 1 && tile_ly <= 7) {
        int r = tile_ly - 2;
        int c = tile_lx - 23;
        uint npMask[5] = uint[5](0x57u, 0x75u, 0x77u, 0x54u, 0x54u);
        bool isText = (r >= 0 && r < 5 && c >= 0 && c < 7) && (((npMask[r] >> (6 - c)) & 1u) != 0u);
        outLayer = isText ? vec4(0.20, 1.00, 0.30, 0.98) : vec4(0.06, 0.06, 0.08, 0.92);
        return true;
    }

    // Bottom-Left: [NL] (Warm Orange)
    if ((flags & 16u) != 0u && tile_lx >= 1 && tile_lx <= 9 && tile_ly >= 24 && tile_ly <= 30) {
        int r = tile_ly - 25;
        int c = tile_lx - 2;
        uint nlMask[5] = uint[5](0x54u, 0x74u, 0x74u, 0x54u, 0x57u);
        bool isText = (r >= 0 && r < 5 && c >= 0 && c < 7) && (((nlMask[r] >> (6 - c)) & 1u) != 0u);
        outLayer = isText ? vec4(1.00, 0.55, 0.10, 0.98) : vec4(0.06, 0.06, 0.08, 0.92);
        return true;
    }

    // Bottom-Right: [PvP] (Crimson Red)
    if ((flags & 32u) != 0u && tile_lx >= 18 && tile_lx <= 30 && tile_ly >= 24 && tile_ly <= 30) {
        int r = tile_ly - 25;
        int c = tile_lx - 19;
        uint pvpMask[5] = uint[5](0x707u, 0x505u, 0x757u, 0x454u, 0x424u);
        bool isText = (r >= 0 && r < 5 && c >= 0 && c < 11) && (((pvpMask[r] >> (10 - c)) & 1u) != 0u);
        outLayer = isText ? vec4(1.00, 0.15, 0.30, 0.98) : vec4(0.06, 0.06, 0.08, 0.92);
        return true;
    }

    // 2. Zero-Fill Brackets on every zone tile with dark shadow
    bool isCore, isShadow;
    evaluateTileBracket(tile_lx, tile_ly, isCore, isShadow);

    if (isCore) {
        vec4 zBorder = vec4(1.00, 0.90, 0.10, 0.95);
        if ((flags & 4u) != 0u) {
            zBorder = vec4(1.00, 0.90, 0.10, 0.95);
        } else if ((flags & 8u) != 0u) {
            zBorder = vec4(0.20, 1.00, 0.30, 0.95);
        } else if ((flags & 16u) != 0u) {
            zBorder = vec4(1.00, 0.55, 0.10, 0.95);
        } else if ((flags & 32u) != 0u) {
            zBorder = vec4(1.00, 0.15, 0.30, 0.95);
        }
        outLayer = zBorder;
        return true;
    } else if (isShadow) {
        outLayer = vec4(0.04, 0.04, 0.06, 0.95);
        return true;
    }

    return false;
}

bool evaluateSpawnOverlay(uint flags, int lx, int ly, out vec4 outLayer) {
    if ((flags & 2u) == 0u) {
        return false;
    }
    int tile_lx = lx % 32;
    int tile_ly = ly % 32;

    bool isCore, isShadow;
    evaluateTileBracket(tile_lx, tile_ly, isCore, isShadow);

    if (isCore) {
        outLayer = vec4(1.00, 0.20, 1.00, 0.95);
        return true;
    } else if (isShadow) {
        outLayer = vec4(0.04, 0.04, 0.06, 0.95);
        return true;
    }
    return false;
}

bool evaluateBlockingOverlay(uint flags, int lx, int ly, out vec4 outLayer) {
    if ((flags & 1u) == 0u) {
        return false;
    }
    int tile_lx = lx % 32;
    int tile_ly = ly % 32;

    bool isCore, isShadow;
    evaluateTileBracket(tile_lx, tile_ly, isCore, isShadow);

    if (isCore) {
        outLayer = vec4(0.00, 0.95, 1.00, 0.95);
        return true;
    } else if (isShadow) {
        outLayer = vec4(0.04, 0.04, 0.06, 0.95);
        return true;
    }
    return false;
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

    // 1. Special Zones (showSpecialTiles) with dedicated 4-corner micro-badges & zero-fill brackets
    if (showSpecialTiles != 0 && evaluateSpecialZones(flags, lx, ly, layer)) {
        blendOverlayLayer(color, hasOverlay, layer);
    }

    // 2. Spawn Radius (showSpawns) with magenta zero-fill brackets
    if (showSpawns != 0 && evaluateSpawnOverlay(flags, lx, ly, layer)) {
        blendOverlayLayer(color, hasOverlay, layer);
    }

    // 3. Pathing / Blocking (showBlocking) with cyan zero-fill brackets
    if (showBlocking != 0 && evaluateBlockingOverlay(flags, lx, ly, layer)) {
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
