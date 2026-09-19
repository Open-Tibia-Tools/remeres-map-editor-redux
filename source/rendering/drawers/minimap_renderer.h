#ifndef RME_RENDERING_DRAWERS_MINIMAP_RENDERER_H_
#define RME_RENDERING_DRAWERS_MINIMAP_RENDERER_H_

#include "rendering/drawers/minimap_cache.h"
#include "rendering/core/shader_program.h"
#include "rendering/core/gl_resources.h"

#include <glm/glm.hpp>
#include <memory>

class Map;

class MinimapRenderer {
public:
	MinimapRenderer();
	~MinimapRenderer();

	// Non-copyable
	MinimapRenderer(const MinimapRenderer&) = delete;
	MinimapRenderer& operator=(const MinimapRenderer&) = delete;

	bool initialize();
	void bindMap(uint64_t map_generation, int width, int height);
	void invalidateAll();
	void markDirty(int floor, const MinimapDirtyRect& rect);
	void flushVisible(const Map& map, int floor, const MinimapDirtyRect& visible_rect);
	void renderVisible(const glm::mat4& projection, int x, int y, int w, int h, int floor, const MinimapDirtyRect& visible_rect);
	void releaseGL();

private:
	void createPaletteTexture();
	void initializeQuad();

	std::unique_ptr<GLTextureResource> palette_texture_id_;
	std::unique_ptr<GLVertexArray> vao_;
	std::unique_ptr<ShaderProgram> shader_;
	MinimapCache cache_;
};

#endif
