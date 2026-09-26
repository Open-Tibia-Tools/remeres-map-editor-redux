#ifndef RME_RENDERING_SHADERS_SPRITE_BATCH_SHADER_H_
#define RME_RENDERING_SHADERS_SPRITE_BATCH_SHADER_H_

#include "rendering/shaders/indicator_shader.h"
#include "rendering/shaders/house_shader.h"
#include "rendering/shaders/zone_shader.h"
#include <string>

namespace rme::rendering::shaders {

/**
 * @brief Vertex shader for dynamic SpriteBatch quad rendering.
 */
inline constexpr const char* SPRITE_BATCH_VERT_SHADER = R"(#version 450 core
layout (location = 0) in vec2 aPos;       // 0,0 to 1,1
layout (location = 1) in vec2 aTexCoord;  // 0,0 to 1,1
layout (location = 2) in vec4 aRect;      // x, y, w, h
layout (location = 3) in vec4 aUV;        // u_min, v_min, u_max, v_max
layout (location = 4) in vec4 aTint;      // r, g, b, a
layout (location = 5) in float aLayer;    // texture array layer
layout (location = 6) in float aHouseId;
layout (location = 7) in float aZoneFlags;

out vec2 vWorldPos;
out vec2 vQuadCoord;
out vec2 vQuadSize;
out vec3 TexCoord;
out vec4 Tint;
flat out float vHouseId;
flat out float vZoneFlags;

uniform mat4 uMVP;

void main() {
    vec2 pos = aRect.xy + aPos * aRect.zw;
    gl_Position = uMVP * vec4(pos, 0.0, 1.0);
    vWorldPos = pos;
    vQuadCoord = aPos;
    vQuadSize = aRect.zw;
    TexCoord = vec3(mix(aUV.xy, aUV.zw, aTexCoord), aLayer);
    Tint = aTint;
    vHouseId = aHouseId;
    vZoneFlags = aZoneFlags;
}
)";

/**
 * @brief Composes and returns the complete fragment shader for dynamic SpriteBatch rendering,
 *        modularly injecting indicator, house, and zone shader procedures.
 */
inline std::string GetSpriteBatchFragShader() {
	return std::string(R"(#version 450 core
in vec2 vWorldPos;
in vec2 vQuadCoord;
in vec2 vQuadSize;
in vec3 TexCoord;
in vec4 Tint;
flat in float vHouseId;
flat in float vZoneFlags;
out vec4 FragColor;

uniform sampler2DArray uAtlas;
uniform vec4 uGlobalTint;
uniform uint uCurrentHouseId;
uniform int uShowHouses;
uniform int uShowSpawns;
uniform int uShowTowns;
uniform int uShowWaypoints;
uniform int uShowTechItems;
uniform int uShowBlocking;
uniform int uShowSpecialTiles;

)") + std::string(INDICATOR_SHADER_GLSL) + std::string(HOUSE_SHADER_GLSL) + std::string(ZONE_SHADER_GLSL) + R"(

void main() {
    if (evaluateTileIndicator(vQuadCoord, vHouseId, uCurrentHouseId,
                              uShowHouses, uShowSpawns, uShowTowns,
                              uShowWaypoints, uShowTechItems, FragColor)) {
        return;
    }

    if (vZoneFlags > 0.5) {
        if (!evaluateZoneOverlay(vWorldPos, vQuadCoord, vQuadSize, vZoneFlags,
                                 uShowBlocking, uShowSpawns, uShowSpecialTiles,
                                 FragColor)) {
            discard;
        }
        return;
    }

    vec4 texColor = texture(uAtlas, TexCoord);
    FragColor = texColor * Tint * uGlobalTint;
    if (FragColor.a < 0.01) {
        discard;
    }

    applyHouseOverlay(FragColor, vWorldPos, vHouseId, uCurrentHouseId, uShowHouses);
}
)";
}

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_SPRITE_BATCH_SHADER_H_
