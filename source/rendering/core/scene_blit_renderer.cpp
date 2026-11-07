#include "rendering/core/scene_blit_renderer.h"
#include "rendering/core/shader_program.h"
#include "rendering/shaders/scene_blit_shader.h"
#include <spdlog/spdlog.h>

namespace rme::rendering {

SceneBlitRenderer::SceneBlitRenderer() = default;
SceneBlitRenderer::~SceneBlitRenderer() = default;

bool SceneBlitRenderer::Initialize() {
	if (is_initialized_) {
		return true;
	}

	shader_ = std::make_unique<ShaderProgram>();
	if (!shader_->Load(std::string(shaders::SCENE_BLIT_VERTEX_SHADER), std::string(shaders::SCENE_BLIT_FRAGMENT_SHADER))) {
		spdlog::error("SceneBlitRenderer: Failed to compile scene blit shader");
		shader_.reset();
		return false;
	}

	shader_->Use();
	shader_->SetInt("u_sceneTexture", 0);
	shader_->Unuse();

	is_initialized_ = true;
	spdlog::info("SceneBlitRenderer: Initialized successfully");
	return true;
}

void SceneBlitRenderer::Draw(GLuint texture_id, int viewport_w, int viewport_h) {
	if (!is_initialized_ || texture_id == 0 || viewport_w <= 0 || viewport_h <= 0) {
		return;
	}

	// Prepare screen state
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, viewport_w, viewport_h);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);

	glBindTextureUnit(0, texture_id);

	shader_->Use();
	glBindVertexArray(empty_vao_.GetID());
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	shader_->Unuse();

	// Restore blending for subsequent UI/NanoVG overlay passes
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

} // namespace rme::rendering
