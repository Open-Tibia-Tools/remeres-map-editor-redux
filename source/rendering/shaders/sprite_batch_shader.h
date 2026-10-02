#ifndef RME_RENDERING_SHADERS_SPRITE_BATCH_SHADER_H_
#define RME_RENDERING_SHADERS_SPRITE_BATCH_SHADER_H_

#include "rendering/core/shader_program.h"
#include "rendering/shaders/indicator_shader.h"
#include "rendering/shaders/house_shader.h"
#include "rendering/shaders/zone_shader.h"
#include <glm/vec4.hpp>
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
uniform int uShowInvalidTiles;
uniform int uShowInvalidZones;

)") + std::string(INDICATOR_SHADER_GLSL) + std::string(HOUSE_SHADER_GLSL) + std::string(ZONE_SHADER_GLSL) + R"(

void main() {
    if (evaluateTileIndicator(vQuadCoord, vHouseId, uCurrentHouseId,
                              uShowHouses, uShowSpawns, uShowTowns,
                              uShowWaypoints, uShowTechItems,
                              uShowInvalidTiles, uShowInvalidZones,
                              Tint, FragColor)) {
        return;
    }

    if (vZoneFlags > 0.5) {
        if (!evaluateZoneOverlay(vWorldPos, vQuadCoord, vQuadSize, vZoneFlags,
                                 uShowBlocking, uShowSpawns, uShowSpecialTiles,
                                 FragColor)) {
            discard;
        }
        FragColor.a *= Tint.a * uGlobalTint.a;
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

/**
 * @brief Sets domain-specific uniform values for the SpriteBatch shader.
 *
 * Keeps SpriteBatch decoupled from OTBM domain concepts (houses, spawns, zones).
 */
inline void SetSpriteBatchOverlayUniforms(
	ShaderProgram& shader,
	uint32_t current_house_id,
	bool show_houses,
	bool show_spawns,
	bool show_towns,
	bool show_waypoints,
	bool show_tech_items,
	bool show_blocking,
	bool show_special_tiles,
	bool show_invalid_tiles,
	bool show_invalid_zones,
	bool show_zone_borders,
	const glm::vec4& zone_border_color,
	const glm::vec4& zone_pz_color,
	const glm::vec4& zone_nopvp_color,
	const glm::vec4& zone_nologout_color,
	const glm::vec4& zone_pvp_color,
	const glm::vec4& zone_blocking_color,
	const glm::vec4& zone_spawn_color,
	const glm::vec4& house_active_color,
	const glm::vec4& house_inactive_color)
{
	shader.Use();
	shader.SetUint("uCurrentHouseId", current_house_id);
	shader.SetInt("uShowHouses", show_houses ? 1 : 0);
	shader.SetInt("uShowSpawns", show_spawns ? 1 : 0);
	shader.SetInt("uShowTowns", show_towns ? 1 : 0);
	shader.SetInt("uShowWaypoints", show_waypoints ? 1 : 0);
	shader.SetInt("uShowTechItems", show_tech_items ? 1 : 0);
	shader.SetInt("uShowBlocking", show_blocking ? 1 : 0);
	shader.SetInt("uShowSpecialTiles", show_special_tiles ? 1 : 0);
	shader.SetInt("uShowInvalidTiles", show_invalid_tiles ? 1 : 0);
	shader.SetInt("uShowInvalidZones", show_invalid_zones ? 1 : 0);

	shader.SetInt("uShowZoneBorders", show_zone_borders ? 1 : 0);
	shader.SetVec4("uZoneBorderColor", zone_border_color);
	shader.SetVec4("uPzWash", zone_pz_color);
	shader.SetVec4("uNpWash", zone_nopvp_color);
	shader.SetVec4("uNlWash", zone_nologout_color);
	shader.SetVec4("uPvpWash", zone_pvp_color);
	shader.SetVec4("uBlockingWash", zone_blocking_color);
	shader.SetVec4("uSpawnWash", zone_spawn_color);
	shader.SetVec4("uHouseActiveWash", house_active_color);
	shader.SetVec4("uHouseInactiveWash", house_inactive_color);
}

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_SPRITE_BATCH_SHADER_H_
