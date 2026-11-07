#ifndef RME_RENDERING_SHADERS_SCENE_BLIT_SHADER_H_
#define RME_RENDERING_SHADERS_SCENE_BLIT_SHADER_H_

#include <string_view>

namespace rme::rendering::shaders {

inline constexpr std::string_view SCENE_BLIT_VERTEX_SHADER = R"(#version 330 core

out vec2 v_texCoord;

void main() {
    // Full-screen triangle covering [-1, 1] NDC without a vertex buffer
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    v_texCoord = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
    gl_Position = vec4(x, y, 0.0, 1.0);
}
)";

inline constexpr std::string_view SCENE_BLIT_FRAGMENT_SHADER = R"(#version 330 core

in vec2 v_texCoord;
out vec4 FragColor;

uniform sampler2D u_sceneTexture;

void main() {
    FragColor = texture(u_sceneTexture, v_texCoord);
}
)";

} // namespace rme::rendering::shaders

#endif // RME_RENDERING_SHADERS_SCENE_BLIT_SHADER_H_
