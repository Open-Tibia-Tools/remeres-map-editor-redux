#ifndef RME_RENDERING_SHADERS_CHUNK_SHADER_H_
#define RME_RENDERING_SHADERS_CHUNK_SHADER_H_

#include "rendering/core/shader_program.h"
#include "rendering/core/drawing_options.h"
#include "rendering/shaders/indicator_shader.h"
#include "rendering/shaders/house_shader.h"
#include "rendering/shaders/pathing_shader.h"
#include <string>

namespace rme::rendering::shaders {

/**
 * @brief Vertex shader for MDI-driven terrain chunk rendering.
 */
inline constexpr const char* CHUNK_VERT_SHADER = R"(#version 430 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aRect;
layout (location = 3) in float aSpriteId;
layout (location = 4) in float aFlags;
layout (location = 5) in vec4 aTint;
layout (location = 6) in float aHouseId;
layout (location = 7) in float aDepth;

out vec2 vWorldPos;
out vec2 vQuadCoord;
out vec3 vTexCoord;
out vec4 vColor;
flat out float vFlags;
flat out float vHouseId;

uniform mat4 uMVP;
uniform vec4 uGlobalTint;
uniform samplerBuffer uAtlasLUT;

void main() {
	vec2 worldPos = aRect.xy + aPos * aRect.zw;
	gl_Position = uMVP * vec4(worldPos, 0.0, 1.0);
	gl_Position.z = aDepth * 2.0 - 1.0;

	vFlags = aFlags;
	vHouseId = aHouseId;
	vWorldPos = worldPos;
	vQuadCoord = aPos;
	vColor = aTint * uGlobalTint;

	int flagBits = int(aFlags + 0.5);
	if ((flagBits & 1) != 0) {
		vTexCoord = vec3(0.0);
	} else {
		int baseTexel = int(aSpriteId + 0.5) * 2;
		vec4 uvRect = texelFetch(uAtlasLUT, baseTexel);
		vec4 meta = texelFetch(uAtlasLUT, baseTexel + 1);
		vec2 uv = mix(uvRect.xy, uvRect.zw, aTexCoord);
		vTexCoord = vec3(uv, meta.x);
	}
}
)";

/**
 * @brief Composes and returns the complete fragment shader for terrain chunk rendering,
 *        modularly injecting indicator, house, and zone shader procedures.
 */
inline std::string GetChunkFragShader() {
	return std::string(R"(#version 430 core
flat in float vFlags;
flat in float vHouseId;
in vec2 vWorldPos;
in vec2 vQuadCoord;
in vec3 vTexCoord;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2DArray uAtlas;
uniform uint uCurrentHouseId;
uniform int uShowHouses;
uniform int uShowTowns;
uniform int uShowWaypoints;
uniform int uShowTechItems;
uniform int uShowInvalidTiles;
uniform int uShowInvalidZones;
uniform int uShowBlocking;
uniform int uExtendedPathingShader;
uniform vec4 uBlockingWash;
uniform int uBlockingBlendMode;

)") + std::string(INDICATOR_SHADER_GLSL) + std::string(HOUSE_SHADER_GLSL) + std::string(PATHING_SHADER_GLSL) + R"(

void main() {
	if (evaluateTileIndicator(vQuadCoord, vHouseId,
	                          uShowHouses, 0, uShowTowns,
	                          uShowWaypoints, uShowTechItems,
	                          uShowInvalidTiles, uShowInvalidZones,
	                          vColor, FragColor)) {
		return;
	}

	int flagBits = int(vFlags + 0.5);
	if ((flagBits & 1) != 0) {
		FragColor = vColor;
		if (FragColor.a < 0.01) {
			discard;
		}
		return;
	}

	vec4 texColor = texture(uAtlas, vTexCoord);
	FragColor = texColor * vColor;
	if (FragColor.a < 0.01) {
		discard;
	}

	applyHouseOverlay(FragColor, vWorldPos, vHouseId, uCurrentHouseId, uShowHouses);
	bool isBlocking = ((flagBits & 2) != 0);
	applyPathingOverlay(FragColor, isBlocking, uShowBlocking, uExtendedPathingShader, uBlockingWash, uBlockingBlendMode);
}
)";
}

/**
 * @brief Sets domain-specific uniform values for the Chunk cache shader.
 */
inline void SetChunkShaderUniforms(
	ShaderProgram& shader,
	uint32_t current_house_id,
	const DrawingOptions& options
) {
	shader.Use();
	shader.SetUint("uCurrentHouseId", current_house_id);
	shader.SetInt("uShowHouses", options.show_houses ? 1 : 0);
	shader.SetVec4("uHouseActiveWash", options.house_active_color);
	shader.SetVec4("uHouseInactiveWash", options.house_inactive_color);
	shader.SetInt("uHouseActiveBlendMode", options.house_active_blend_mode);
	shader.SetInt("uHouseInactiveBlendMode", options.house_inactive_blend_mode);
	shader.SetInt("uShowTowns", options.show_towns ? 1 : 0);
	shader.SetInt("uShowWaypoints", (options.show_waypoints && !options.ingame) ? 1 : 0);
	shader.SetInt("uShowTechItems", (options.show_tech_items && !options.ingame) ? 1 : 0);
	shader.SetInt("uShowInvalidTiles", (options.show_invalid_tiles && !options.ingame) ? 1 : 0);
	shader.SetInt("uShowInvalidZones", (options.show_invalid_zones && !options.ingame) ? 1 : 0);
	shader.SetInt("uShowBlocking", options.show_blocking ? 1 : 0);
	shader.SetInt("uExtendedPathingShader", options.extended_pathing_shader ? 1 : 0);
	shader.SetVec4("uBlockingWash", options.zone_blocking_color);
	shader.SetInt("uBlockingBlendMode", options.zone_blocking_blend_mode);
}

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_CHUNK_SHADER_H_
