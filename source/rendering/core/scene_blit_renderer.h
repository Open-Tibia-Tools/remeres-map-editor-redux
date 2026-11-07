#ifndef RME_RENDERING_CORE_SCENE_BLIT_RENDERER_H_
#define RME_RENDERING_CORE_SCENE_BLIT_RENDERER_H_

#include "rendering/core/gl_resources.h"
#include <glad/glad.h>
#include <memory>

class ShaderProgram;

namespace rme::rendering {

/**
 * @brief Fullscreen quad/triangle blit renderer for displaying the SceneFramebuffer.
 *
 * Uses an empty VAO and gl_VertexID in the vertex shader to render a single full-screen
 * triangle without any client-side VBO memory allocations.
 */
class SceneBlitRenderer {
public:
	SceneBlitRenderer();
	~SceneBlitRenderer();

	SceneBlitRenderer(const SceneBlitRenderer&) = delete;
	SceneBlitRenderer& operator=(const SceneBlitRenderer&) = delete;

	bool Initialize();
	void Draw(GLuint texture_id, int viewport_w, int viewport_h);

private:
	std::unique_ptr<ShaderProgram> shader_;
	GLVertexArray empty_vao_;
	bool is_initialized_ = false;
};

} // namespace rme::rendering

#endif // RME_RENDERING_CORE_SCENE_BLIT_RENDERER_H_
