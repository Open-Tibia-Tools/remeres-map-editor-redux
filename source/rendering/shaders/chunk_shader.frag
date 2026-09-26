#version 430 core

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

// Include or inject house_shader.glsl here
// (evaluateHouseEntry, applyHouseOverlay)

void main() {
	if (evaluateHouseEntry(vQuadCoord, vHouseId, uCurrentHouseId, uShowHouses, FragColor)) {
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
