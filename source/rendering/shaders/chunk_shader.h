#ifndef RME_RENDERING_SHADERS_CHUNK_SHADER_H_
#define RME_RENDERING_SHADERS_CHUNK_SHADER_H_

#include "rendering/shaders/indicator_shader.h"
#include "rendering/shaders/house_shader.h"
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

	vFlags = aFlags;
	vHouseId = aHouseId;
	vWorldPos = worldPos;
	vQuadCoord = aPos;
	vColor = aTint * uGlobalTint;

	if (aFlags > 0.5) {
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

)") + std::string(INDICATOR_SHADER_GLSL) + std::string(HOUSE_SHADER_GLSL) + R"(

void main() {
	if (evaluateTileIndicator(vQuadCoord, vHouseId, uCurrentHouseId,
	                          uShowHouses, 0, uShowTowns,
	                          uShowWaypoints, uShowTechItems, FragColor)) {
		return;
	}

	if (vFlags > 0.5) {
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
}
)";
}

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_CHUNK_SHADER_H_
