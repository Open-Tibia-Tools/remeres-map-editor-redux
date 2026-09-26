#version 450 core

in vec2 vWorldPos;
in vec2 vQuadCoord;
in vec3 TexCoord;
in vec4 Tint;
flat in float vHouseId;
out vec4 FragColor;

uniform sampler2DArray uAtlas;
uniform vec4 uGlobalTint;
uniform uint uCurrentHouseId;
uniform int uShowHouses;

// Include or inject house_shader.glsl here
// (evaluateHouseEntry, applyHouseOverlay)

void main() {
    if (evaluateHouseEntry(vQuadCoord, vHouseId, uCurrentHouseId, uShowHouses, FragColor)) {
        return;
    }

    vec4 texColor = texture(uAtlas, TexCoord);
    FragColor = texColor * Tint * uGlobalTint;
    if (FragColor.a < 0.01) {
        discard;
    }

    applyHouseOverlay(FragColor, vWorldPos, vHouseId, uCurrentHouseId, uShowHouses);
}
