#ifndef RME_RENDERING_CORE_SCENE_FRAMEBUFFER_H_
#define RME_RENDERING_CORE_SCENE_FRAMEBUFFER_H_

#include "rendering/core/gl_resources.h"
#include <glad/glad.h>
#include <memory>
#include <cstdint>

namespace rme::rendering {

/**
 * @brief RAII container for the scene off-screen framebuffer.
 *
 * Encapsulates an FBO with a color texture (GL_TEXTURE_2D, RGBA8) and a depth-stencil
 * renderbuffer (GL_DEPTH24_STENCIL8). Manages allocation and resizing only when dimensions
 * change, satisfying the no-allocation-in-hot-paths invariant.
 */
class SceneFramebuffer {
public:
	SceneFramebuffer();
	~SceneFramebuffer();

	SceneFramebuffer(const SceneFramebuffer&) = delete;
	SceneFramebuffer& operator=(const SceneFramebuffer&) = delete;
	SceneFramebuffer(SceneFramebuffer&&) noexcept;
	SceneFramebuffer& operator=(SceneFramebuffer&&) noexcept;

	bool Resize(int width, int height);
	void Bind();
	void Unbind();
	void SetSmoothing(bool smooth);

	[[nodiscard]] GLuint GetColorTexture() const noexcept {
		return color_texture_ ? color_texture_->GetID() : 0;
	}
	[[nodiscard]] GLuint GetFramebufferID() const noexcept {
		return fbo_ ? fbo_->GetID() : 0;
	}
	[[nodiscard]] int GetWidth() const noexcept { return width_; }
	[[nodiscard]] int GetHeight() const noexcept { return height_; }
	[[nodiscard]] bool IsValid() const noexcept { return is_valid_; }

private:
	void ReleaseResources();

	std::unique_ptr<GLFramebuffer> fbo_;
	std::unique_ptr<GLTextureResource> color_texture_;
	std::unique_ptr<GLRenderbuffer> depth_stencil_rbo_;
	int width_ = 0;
	int height_ = 0;
	bool is_valid_ = false;
	bool is_smoothed_ = false;
};

} // namespace rme::rendering

#endif // RME_RENDERING_CORE_SCENE_FRAMEBUFFER_H_
