#include "rendering/drawers/tiles/shade_drawer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/atlas_manager.h"

ShadeDrawer::ShadeDrawer() {
}

ShadeDrawer::~ShadeDrawer() {
}

void ShadeDrawer::draw(SpriteBatch& sprite_batch, const RenderView& view, const DrawingOptions& options, const AtlasManager& atlas) {
	if (view.start_z != view.end_z && options.show_shade) {
		const glm::vec4 color(0.0f, 0.0f, 0.0f, 128.0f / 255.0f);
		const float w = view.screensize_x * view.zoom;
		const float h = view.screensize_y * view.zoom;
		sprite_batch.drawRect(0.0f, 0.0f, w, h, color, atlas);
	}
}
