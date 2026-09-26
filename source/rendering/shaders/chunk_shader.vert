#version 430 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aRect;
layout (location = 3) in float aSpriteId;
layout (location = 4) in float aFlags;
layout (location = 5) in vec4 aTint;
layout (location = 6) in float aHouseId;
layout (location = 7) in float aZoneFlags;

out vec2 vWorldPos;
out vec2 vQuadCoord;
out vec3 vTexCoord;
out vec4 vColor;
flat out float vFlags;
flat out float vHouseId;
flat out float vZoneFlags;

uniform mat4 uMVP;
uniform vec4 uGlobalTint;
uniform samplerBuffer uAtlasLUT;

void main() {
	vec2 worldPos = aRect.xy + aPos * aRect.zw;
	gl_Position = uMVP * vec4(worldPos, 0.0, 1.0);

	vFlags = aFlags;
	vHouseId = aHouseId;
	vZoneFlags = aZoneFlags;
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
