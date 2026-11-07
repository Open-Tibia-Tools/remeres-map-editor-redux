#include "rendering/core/scene_framebuffer.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace rme::rendering {

SceneFramebuffer::SceneFramebuffer() = default;

SceneFramebuffer::~SceneFramebuffer() {
	ReleaseResources();
}

SceneFramebuffer::SceneFramebuffer(SceneFramebuffer&& other) noexcept = default;
SceneFramebuffer& SceneFramebuffer::operator=(SceneFramebuffer&& other) noexcept = default;

void SceneFramebuffer::ReleaseResources() {
	depth_stencil_rbo_.reset();
	color_texture_.reset();
	fbo_.reset();
	width_ = 0;
	height_ = 0;
	is_valid_ = false;
}

bool SceneFramebuffer::Resize(int width, int height) {
	if (width <= 0 || height <= 0) {
		return false;
	}

	// Zero-allocation early exit if dimensions are unchanged and FBO is valid
	if (is_valid_ && width_ == width && height_ == height_) {
		return true;
	}

	ReleaseResources();

	fbo_ = std::make_unique<GLFramebuffer>();
	color_texture_ = std::make_unique<GLTextureResource>(GL_TEXTURE_2D);

	// Allocate immutable 2D texture storage for color attachment
	glTextureStorage2D(color_texture_->GetID(), 1, GL_RGBA8, width, height);
	glTextureParameteri(color_texture_->GetID(), GL_TEXTURE_MIN_FILTER, is_smoothed_ ? GL_LINEAR : GL_NEAREST);
	glTextureParameteri(color_texture_->GetID(), GL_TEXTURE_MAG_FILTER, is_smoothed_ ? GL_LINEAR : GL_NEAREST);
	glTextureParameteri(color_texture_->GetID(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(color_texture_->GetID(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Allocate depth24_stencil8 renderbuffer
	depth_stencil_rbo_ = std::make_unique<GLRenderbuffer>();
	glNamedRenderbufferStorage(depth_stencil_rbo_->GetID(), GL_DEPTH24_STENCIL8, width, height);

	// Attach texture and renderbuffer to the framebuffer
	glNamedFramebufferTexture(fbo_->GetID(), GL_COLOR_ATTACHMENT0, color_texture_->GetID(), 0);
	glNamedFramebufferRenderbuffer(fbo_->GetID(), GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_stencil_rbo_->GetID());

	const GLenum status = glCheckNamedFramebufferStatus(fbo_->GetID(), GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		spdlog::error("SceneFramebuffer: Framebuffer incomplete [status={:#x}] for size {}x{}", status, width, height);
		ReleaseResources();
		return false;
	}

	width_ = width;
	height_ = height;
	is_valid_ = true;
	spdlog::info("SceneFramebuffer: successfully initialized [{}x{}]", width_, height_);
	return true;
}

void SceneFramebuffer::Bind() {
	if (!is_valid_) {
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->GetID());
	glViewport(0, 0, width_, height_);
}

void SceneFramebuffer::Unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneFramebuffer::SetSmoothing(bool smooth) {
	if (is_smoothed_ == smooth) {
		return;
	}
	is_smoothed_ = smooth;
	if (is_valid_ && color_texture_) {
		const GLint filter = smooth ? GL_LINEAR : GL_NEAREST;
		glTextureParameteri(color_texture_->GetID(), GL_TEXTURE_MIN_FILTER, filter);
		glTextureParameteri(color_texture_->GetID(), GL_TEXTURE_MAG_FILTER, filter);
	}
}

} // namespace rme::rendering
