//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/utilities/light_drawer.h"
#include "app/definitions.h"
#include "rendering/core/light_palette.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/gl_scoped_state.h"
#include "rendering/core/render_view.h"
#include "rendering/core/graphics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>

LightDrawer::LightDrawer() = default;

LightDrawer::~LightDrawer() = default;

void LightDrawer::initGL() {
	constexpr auto vertex_shader = R"(
		#version 450 core
		layout (location = 0) in vec2 aPos;

		uniform mat4 uMVP;
		out vec2 TexCoord;

		void main() {
			gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
			TexCoord = vec2(aPos.x, aPos.y);
		}
	)";

	constexpr auto fragment_shader = R"(
		#version 450 core
		in vec2 TexCoord;
		uniform sampler2D uLightTexture;
		out vec4 OutColor;

		void main() {
			OutColor = texture(uLightTexture, TexCoord);
		}
	)";

	shader_ = std::make_unique<ShaderProgram>();
	shader_->Load(vertex_shader, fragment_shader);

	constexpr float vertices[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	vao_ = std::make_unique<GLVertexArray>();
	vbo_ = std::make_unique<GLBuffer>();

	glNamedBufferStorage(vbo_->GetID(), sizeof(vertices), vertices, 0);
	glVertexArrayVertexBuffer(vao_->GetID(), 0, vbo_->GetID(), 0, 2 * sizeof(float));
	glEnableVertexArrayAttrib(vao_->GetID(), 0);
	glVertexArrayAttribFormat(vao_->GetID(), 0, 2, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao_->GetID(), 0, 0);
	glBindVertexArray(0);
}

void LightDrawer::updateViewportTexture(
	const RenderView& view,
	const BaseMap& map,
	GraphicManager& gfx,
	const rme::lighting::LightConfig& config,
	int min_cx, int min_cy, int max_cx, int max_cy
) {
	const int chunks_w = max_cx - min_cx + 1;
	const int chunks_h = max_cy - min_cy + 1;
	const int tex_w = chunks_w * rme::lighting::CHUNK_SIZE;
	const int tex_h = chunks_h * rme::lighting::CHUNK_SIZE;

	if (tex_w <= 0 || tex_h <= 0) {
		return;
	}

	const size_t required_texels = static_cast<size_t>(tex_w * tex_h);
	if (viewport_pixels_.size() < required_texels) {
		viewport_pixels_.resize(required_texels);
	}

	for (int cy = min_cy; cy <= max_cy; ++cy) {
		for (int cx = min_cx; cx <= max_cx; ++cx) {
			rme::lighting::CachedLightChunk& chunk = cache_.getOrBakeChunk(
				cx, cy, view.floor, map,
				view.start_z, view.superend_z,
				config, gfx, current_frame_
			);

			const int dest_chunk_x = (cx - min_cx) * rme::lighting::CHUNK_SIZE;
			const int dest_chunk_y = (cy - min_cy) * rme::lighting::CHUNK_SIZE;

			for (int row = 0; row < rme::lighting::CHUNK_SIZE; ++row) {
				const int dest_offset = (dest_chunk_y + row) * tex_w + dest_chunk_x;
				const int src_offset = row * rme::lighting::CHUNK_SIZE;
				std::memcpy(&viewport_pixels_[dest_offset], &chunk.pixels[src_offset], rme::lighting::CHUNK_SIZE * sizeof(uint32_t));
			}
		}
	}

	if (!texture_ || tex_width_ != tex_w || tex_height_ != tex_h) {
		texture_ = std::make_unique<GLTextureResource>(GL_TEXTURE_2D);
		tex_width_ = tex_w;
		tex_height_ = tex_h;
		glTextureStorage2D(texture_->GetID(), 1, GL_RGBA8, tex_w, tex_h);
		glTextureParameteri(texture_->GetID(), GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(texture_->GetID(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(texture_->GetID(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(texture_->GetID(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	glTextureSubImage2D(texture_->GetID(), 0, 0, 0, tex_w, tex_h, GL_RGBA, GL_UNSIGNED_BYTE, viewport_pixels_.data());
}

void LightDrawer::render(
	const RenderView& view,
	const BaseMap& map,
	GraphicManager& gfx,
	const DrawingOptions& options
) {
	if (!options.isDrawLight()) {
		return;
	}

	if (!shader_) {
		initGL();
	}
	if (!shader_) {
		return;
	}

	++current_frame_;
	if (current_frame_ % rme::lighting::LightCache::PRUNE_INTERVAL_FRAMES == 0) {
		cache_.prune(view.floor, current_frame_);
	}

	const rme::lighting::LightConfig config {
		.ambient_color = static_cast<uint8_t>(options.server_light.color),
		.ambient_intensity = static_cast<uint8_t>(options.server_light.intensity),
		.minimum_ambient_light = options.minimum_ambient_light,
		.enabled = options.isDrawLight()
	};

	const ViewBounds bounds = view.getBoundsForFloor(view.floor);
	const int view_min_cx = (bounds.start_x >> 4);
	const int view_max_cx = (bounds.end_x >> 4);
	const int view_min_cy = (bounds.start_y >> 4);
	const int view_max_cy = (bounds.end_y >> 4);

	const bool bounds_outside = (last_floor_ != view.floor ||
	                             !texture_ ||
	                             view_min_cx < last_min_cx_ ||
	                             view_max_cx > last_max_cx_ ||
	                             view_min_cy < last_min_cy_ ||
	                             view_max_cy > last_max_cy_ ||
	                             view_min_cx > last_min_cx_ + 2 ||
	                             view_max_cx < last_max_cx_ - 2 ||
	                             view_min_cy > last_min_cy_ + 2 ||
	                             view_max_cy < last_max_cy_ - 2);
	const bool config_changed = (config != last_config_);

	if (bounds_outside || config_changed || force_texture_rebuild_) {
		const int min_cx = view_min_cx - 1;
		const int max_cx = view_max_cx + 1;
		const int min_cy = view_min_cy - 1;
		const int max_cy = view_max_cy + 1;

		updateViewportTexture(view, map, gfx, config, min_cx, min_cy, max_cx, max_cy);
		last_min_cx_ = min_cx;
		last_min_cy_ = min_cy;
		last_max_cx_ = max_cx;
		last_max_cy_ = max_cy;
		last_floor_ = view.floor;
		last_config_ = config;
		force_texture_rebuild_ = false;
	}

	if (!texture_ || tex_width_ <= 0 || tex_height_ <= 0) {
		return;
	}

	const float world_x = static_cast<float>(last_min_cx_ * rme::lighting::CHUNK_SIZE * TILE_SIZE - view.view_scroll_x);
	const float world_y = static_cast<float>(last_min_cy_ * rme::lighting::CHUNK_SIZE * TILE_SIZE - view.view_scroll_y);
	const float world_w = static_cast<float>((last_max_cx_ - last_min_cx_ + 1) * rme::lighting::CHUNK_SIZE * TILE_SIZE);
	const float world_h = static_cast<float>((last_max_cy_ - last_min_cy_ + 1) * rme::lighting::CHUNK_SIZE * TILE_SIZE);

	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(world_x, world_y, 0.0f));
	model = glm::scale(model, glm::vec3(world_w, world_h, 1.0f));

	shader_->Use();
	shader_->SetInt("uLightTexture", 0);
	shader_->SetMat4("uMVP", view.projectionMatrix * view.viewMatrix * model);

	glBindTextureUnit(0, texture_->GetID());

	{
		ScopedGLCapability blend_cap(GL_BLEND);
		ScopedGLBlend blend_func(GL_DST_COLOR, GL_ZERO);

		glBindVertexArray(vao_->GetID());
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glBindVertexArray(0);
	}
}

// Legacy fallback implementation for IngamePreviewRenderer
void LightDrawer::computeBrightness(const RenderView& view, const LightBuffer& light_buffer, const DrawingOptions& options) {
	const int tw = light_buffer.width;
	const int th = light_buffer.height;
	const size_t tile_count = static_cast<size_t>(tw) * static_cast<size_t>(th);
	legacy_tile_brightness_.resize(tile_count * 4);

	const rme::lighting::LightConfig config {
		.ambient_color = static_cast<uint8_t>(options.server_light.color),
		.ambient_intensity = static_cast<uint8_t>(options.server_light.intensity),
		.minimum_ambient_light = options.minimum_ambient_light,
		.enabled = options.isDrawLight()
	};
	const glm::vec3 ambient = rme::lighting::getAmbientRGB(view.floor, config);
	const uint8_t ambient_r = static_cast<uint8_t>(std::clamp(std::lround(ambient.r * 255.0f), 0l, 255l));
	const uint8_t ambient_g = static_cast<uint8_t>(std::clamp(std::lround(ambient.g * 255.0f), 0l, 255l));
	const uint8_t ambient_b = static_cast<uint8_t>(std::clamp(std::lround(ambient.b * 255.0f), 0l, 255l));

	for (size_t index = 0; index < tile_count; ++index) {
		uint8_t* pixel = legacy_tile_brightness_.data() + index * 4;
		pixel[0] = ambient_r;
		pixel[1] = ambient_g;
		pixel[2] = ambient_b;
		pixel[3] = 0xFF;
	}

	constexpr float inv_tile_size = 1.0f / static_cast<float>(TILE_SIZE);

	for (size_t light_index = 0; light_index < light_buffer.lights.size(); ++light_index) {
		const auto& light = light_buffer.lights[light_index];
		if (light.intensity == 0) {
			continue;
		}

		const float intensity_tiles = static_cast<float>(light.intensity);
		const int radius_pixels = static_cast<int>(std::ceil(intensity_tiles * TILE_SIZE));
		const int min_tx = std::max(0, static_cast<int>(std::floor((light.pixel_x - radius_pixels - light_buffer.origin_x * TILE_SIZE) * inv_tile_size)));
		const int min_ty = std::max(0, static_cast<int>(std::floor((light.pixel_y - radius_pixels - light_buffer.origin_y * TILE_SIZE) * inv_tile_size)));
		const int max_tx = std::min(tw - 1, static_cast<int>(std::floor((light.pixel_x + radius_pixels - light_buffer.origin_x * TILE_SIZE) * inv_tile_size)));
		const int max_ty = std::min(th - 1, static_cast<int>(std::floor((light.pixel_y + radius_pixels - light_buffer.origin_y * TILE_SIZE) * inv_tile_size)));

		if (min_tx > max_tx || min_ty > max_ty) {
			continue;
		}

		const auto& light_rgb = rme::lighting::s_palette_table[light.color];
		const int light_r_base = light_rgb.r;
		const int light_g_base = light_rgb.g;
		const int light_b_base = light_rgb.b;

		const float max_dist_tiles = intensity_tiles - 0.05f;
		if (max_dist_tiles <= 0.0f) {
			continue;
		}
		const float max_dist_pixels = max_dist_tiles * static_cast<float>(TILE_SIZE);
		const float max_dist_sq = max_dist_pixels * max_dist_pixels;

		for (int ty = min_ty; ty <= max_ty; ++ty) {
			const int tile_center_y = (light_buffer.origin_y + ty) * TILE_SIZE + TILE_SIZE / 2;
			const float dy = static_cast<float>(tile_center_y - light.pixel_y);
			const float dy2 = dy * dy;
			if (dy2 >= max_dist_sq) {
				continue;
			}

			const size_t row_base_index = static_cast<size_t>(ty) * static_cast<size_t>(tw);
			const float dx_start = static_cast<float>((light_buffer.origin_x + min_tx) * TILE_SIZE + TILE_SIZE / 2 - light.pixel_x);
			constexpr float tile_step = static_cast<float>(TILE_SIZE);

			const LightBuffer::TileLight* tile_light_row = &light_buffer.tiles[row_base_index + min_tx];
			uint8_t* brightness_row = &legacy_tile_brightness_[(row_base_index + min_tx) * 4];

			float dx = dx_start;
			const int tx_count = max_tx - min_tx;

			for (int offset = 0; offset <= tx_count; ++offset, dx += tile_step) {
				if (light_index < tile_light_row[offset].start) {
					continue;
				}

				const float dist_sq = dx * dx + dy2;
				if (dist_sq >= max_dist_sq) {
					continue;
				}

				const float distance_tiles = std::sqrt(dist_sq) * inv_tile_size;
				float factor = (-distance_tiles + intensity_tiles) * 0.2f;
				if (factor < 0.01f) {
					continue;
				}
				factor = std::min(factor, 1.0f);

				const int factor_256 = static_cast<int>(factor * 256.0f + 0.5f);
				const uint8_t light_r = static_cast<uint8_t>((light_r_base * factor_256) >> 8);
				const uint8_t light_g = static_cast<uint8_t>((light_g_base * factor_256) >> 8);
				const uint8_t light_b = static_cast<uint8_t>((light_b_base * factor_256) >> 8);

				uint8_t* px = brightness_row + (offset * 4);
				px[0] = std::max(px[0], light_r);
				px[1] = std::max(px[1], light_g);
				px[2] = std::max(px[2], light_b);
			}
		}
	}
}

void LightDrawer::draw(const RenderView& view, const LightBuffer& light_buffer, const DrawingOptions& options) {
	if (!shader_) {
		initGL();
	}

	if (!shader_ || light_buffer.width <= 0 || light_buffer.height <= 0) {
		return;
	}

	computeBrightness(view, light_buffer, options);

	if (!texture_ || tex_width_ != light_buffer.width || tex_height_ != light_buffer.height) {
		texture_ = std::make_unique<GLTextureResource>(GL_TEXTURE_2D);
		tex_width_ = light_buffer.width;
		tex_height_ = light_buffer.height;
		glTextureStorage2D(texture_->GetID(), 1, GL_RGBA8, tex_width_, tex_height_);
		glTextureParameteri(texture_->GetID(), GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(texture_->GetID(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(texture_->GetID(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(texture_->GetID(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	glTextureSubImage2D(texture_->GetID(), 0, 0, 0, light_buffer.width, light_buffer.height, GL_RGBA, GL_UNSIGNED_BYTE, legacy_tile_brightness_.data());

	const float draw_x = static_cast<float>(light_buffer.origin_x * TILE_SIZE - view.view_scroll_x);
	const float draw_y = static_cast<float>(light_buffer.origin_y * TILE_SIZE - view.view_scroll_y);
	const float draw_width = static_cast<float>(light_buffer.width * TILE_SIZE);
	const float draw_height = static_cast<float>(light_buffer.height * TILE_SIZE);

	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(draw_x, draw_y, 0.0f));
	model = glm::scale(model, glm::vec3(draw_width, draw_height, 1.0f));

	shader_->Use();
	shader_->SetInt("uLightTexture", 0);
	shader_->SetMat4("uMVP", view.projectionMatrix * view.viewMatrix * model);

	glBindTextureUnit(0, texture_->GetID());

	{
		ScopedGLCapability blend_capability(GL_BLEND);
		ScopedGLBlend blend_state(GL_DST_COLOR, GL_ZERO);

		glBindVertexArray(vao_->GetID());
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glBindVertexArray(0);
	}
}
