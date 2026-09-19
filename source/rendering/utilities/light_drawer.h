//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_UTILITIES_LIGHT_DRAWER_H_
#define RME_RENDERING_UTILITIES_LIGHT_DRAWER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "rendering/core/gl_resources.h"
#include "rendering/core/shader_program.h"
#include "rendering/core/light_types.h"
#include "rendering/core/light_cache.h"
#include "rendering/core/light_buffer.h"

class BaseMap;
class GraphicManager;
struct RenderView;
struct DrawingOptions;

class LightDrawer {
public:
	LightDrawer();
	~LightDrawer();

	LightDrawer(const LightDrawer&) = delete;
	LightDrawer& operator=(const LightDrawer&) = delete;
	LightDrawer(LightDrawer&&) noexcept = default;
	LightDrawer& operator=(LightDrawer&&) noexcept = default;

	void render(
		const RenderView& view,
		const BaseMap& map,
		GraphicManager& gfx,
		const DrawingOptions& options
	);

	void invalidateAll() noexcept {
		cache_.invalidateAll();
		force_texture_rebuild_ = true;
	}

	void updateDirtyState(SpatialChangeTracker& tracker) {
		if (cache_.updateDirtyState(tracker)) {
			force_texture_rebuild_ = true;
		}
	}

	void markDirty() {
		invalidateAll();
	}

	// Legacy draw compatibility for IngamePreviewRenderer
	void draw(const RenderView& view, const LightBuffer& light_buffer, const DrawingOptions& options);

private:
	void initGL();
	void updateViewportTexture(
		const RenderView& view,
		const BaseMap& map,
		GraphicManager& gfx,
		const rme::lighting::LightConfig& config,
		int min_cx, int min_cy, int max_cx, int max_cy
	);

	void computeBrightness(const RenderView& view, const LightBuffer& light_buffer, const DrawingOptions& options);

	rme::lighting::LightCache cache_;
	std::unique_ptr<ShaderProgram> shader_;
	std::unique_ptr<GLVertexArray> vao_;
	std::unique_ptr<GLBuffer> vbo_;
	std::unique_ptr<GLTextureResource> texture_;

	std::vector<uint32_t> viewport_pixels_;

	// Margin around viewport to avoid GPU texture re-uploads while panning
	static constexpr int MARGIN_CHUNKS = 8;

	// Cached bounds to eliminate redundant texture updates
	int last_min_cx_ = 0;
	int last_min_cy_ = 0;
	int last_max_cx_ = 0;
	int last_max_cy_ = 0;
	int last_floor_ = -1;
	rme::lighting::LightConfig last_config_;
	bool force_texture_rebuild_ = true;
	uint64_t current_frame_ = 0;

	// Viewport active texture dimensions
	int tex_width_ = 0;
	int tex_height_ = 0;

	// Persistent GPU allocated texture storage dimensions
	int gpu_tex_width_ = 0;
	int gpu_tex_height_ = 0;

	std::vector<uint8_t> legacy_tile_brightness_;
};

namespace rme::lighting {
using LightDrawer = ::LightDrawer;
}

#endif // RME_RENDERING_UTILITIES_LIGHT_DRAWER_H_
