#include "rendering/drawers/minimap_renderer.h"

#include "rendering/core/gl_scoped_state.h"
#include "rendering/core/minimap_colors.h"
#include "rendering/core/shared_geometry.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

namespace {

const char* minimap_vert = R"(
#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
uniform mat4 uProjection;
uniform vec4 uDestRect;

void main() {
    vec2 pos = uDestRect.xy + aPos * uDestRect.zw;
    gl_Position = uProjection * vec4(pos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* minimap_frag = R"(
#version 450 core
in vec2 TexCoord;
out vec4 FragColor;

uniform usampler2D uMinimapTexture;
uniform sampler1D uPaletteTexture;

void main() {
    uint colorIndex = texture(uMinimapTexture, TexCoord).r;
    if (colorIndex == 0u) {
        discard;
    }

    float paletteUV = (float(colorIndex) + 0.5) / 256.0;
    FragColor = texture(uPaletteTexture, paletteUV);
}
)";

}

MinimapRenderer::MinimapRenderer() = default;

MinimapRenderer::~MinimapRenderer() = default;

bool MinimapRenderer::initialize() {
	shader_ = std::make_unique<ShaderProgram>();
	if (!shader_->Load(minimap_vert, minimap_frag)) {
		spdlog::error("MinimapRenderer: Failed to load shader");
		return false;
	}

	if (!SharedGeometry::Instance().initialize()) {
		spdlog::error("MinimapRenderer: Failed to initialize shared geometry");
		return false;
	}

	createPaletteTexture();
	initializeQuad();
	return true;
}

void MinimapRenderer::bindMap(uint64_t map_generation, int width, int height) {
	cache_.bindMap(map_generation, width, height);
}

void MinimapRenderer::invalidateAll() {
	cache_.invalidateAll();
}

void MinimapRenderer::markDirty(int floor, const MinimapDirtyRect& rect) {
	cache_.markDirty(floor, rect);
}

void MinimapRenderer::flushVisible(const Map& map, int floor, const MinimapDirtyRect& visible_rect) {
	cache_.flushVisible(map, floor, visible_rect);
}

void MinimapRenderer::renderVisible(const glm::mat4& projection, int x, int y, int w, int h, int floor, const MinimapDirtyRect& visible_rect) {
	if (!shader_ || !vao_ || cache_.getWidth() <= 0 || cache_.getHeight() <= 0 || visible_rect.width <= 0 || visible_rect.height <= 0) {
		return;
	}

	const auto pages = cache_.collectVisiblePages(floor, visible_rect);
	if (pages.empty()) {
		return;
	}

	const float scale_x = static_cast<float>(w) / std::max(1, visible_rect.width);
	const float scale_y = static_cast<float>(h) / std::max(1, visible_rect.height);

	ScopedGLCapability blend_cap(GL_BLEND);
	ScopedGLBlend blend_state(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	shader_->Use();
	shader_->SetMat4("uProjection", projection);
	shader_->SetInt("uPaletteTexture", 1);
	shader_->SetInt("uMinimapTexture", 0);
	glBindTextureUnit(1, palette_texture_id_->GetID());
	glBindVertexArray(vao_->GetID());

	GLuint last_bound_texture = 0;
	for (const auto& page : pages) {
		const float page_origin_x = static_cast<float>(page.page_x * MinimapCache::PageSize - visible_rect.x);
		const float page_origin_y = static_cast<float>(page.page_y * MinimapCache::PageSize - visible_rect.y);
		const glm::vec4 dest_rect(
			x + page_origin_x * scale_x,
			y + page_origin_y * scale_y,
			MinimapCache::PageSize * scale_x,
			MinimapCache::PageSize * scale_y);

		if (page.texture_id != last_bound_texture) {
			glBindTextureUnit(0, page.texture_id);
			last_bound_texture = page.texture_id;
		}
		shader_->SetVec4("uDestRect", dest_rect);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	}

	glBindVertexArray(0);
}

void MinimapRenderer::releaseGL() {
	cache_.releaseGL();
	palette_texture_id_.reset();
	vao_.reset();
	shader_.reset();
}

void MinimapRenderer::createPaletteTexture() {
	palette_texture_id_ = std::make_unique<GLTextureResource>(GL_TEXTURE_1D);

	std::vector<uint8_t> palette(256 * 4);
	for (int i = 0; i < 256; ++i) {
		palette[i * 4 + 0] = minimap_color[i].red;
		palette[i * 4 + 1] = minimap_color[i].green;
		palette[i * 4 + 2] = minimap_color[i].blue;
		palette[i * 4 + 3] = 255;
	}
	palette[3] = 0;

	glTextureStorage1D(palette_texture_id_->GetID(), 1, GL_RGBA8, 256);
	glTextureSubImage1D(palette_texture_id_->GetID(), 0, 0, 256, GL_RGBA, GL_UNSIGNED_BYTE, palette.data());
	glTextureParameteri(palette_texture_id_->GetID(), GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(palette_texture_id_->GetID(), GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(palette_texture_id_->GetID(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
}

void MinimapRenderer::initializeQuad() {
	vao_ = std::make_unique<GLVertexArray>();
	const GLuint vao = vao_->GetID();

	glVertexArrayVertexBuffer(vao, 0, SharedGeometry::Instance().getQuadVBO(), 0, 4 * sizeof(float));
	glVertexArrayElementBuffer(vao, SharedGeometry::Instance().getQuadEBO());

	glEnableVertexArrayAttrib(vao, 0);
	glVertexArrayAttribFormat(vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao, 0, 0);

	glEnableVertexArrayAttrib(vao, 1);
	glVertexArrayAttribFormat(vao, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
	glVertexArrayAttribBinding(vao, 1, 0);
}
