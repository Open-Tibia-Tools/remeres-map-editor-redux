#ifndef RME_RENDERING_GRID_DRAWER_H_
#define RME_RENDERING_GRID_DRAWER_H_

#include <glm/glm.hpp>

struct RenderView;
struct DrawingOptions;
struct ViewBounds;
class SpriteBatch;
class AtlasManager;

class GridDrawer {
public:
	void DrawGrid(SpriteBatch& sprite_batch, const RenderView& view, const DrawingOptions& options, const ViewBounds& bounds, const AtlasManager& atlas);
	void DrawIngameBox(SpriteBatch& sprite_batch, const RenderView& view, const DrawingOptions& options, const ViewBounds& bounds, const AtlasManager& atlas);
	void DrawNodeLoadingPlaceholder(SpriteBatch& sprite_batch, int nd_map_x, int nd_map_y, const RenderView& view, const AtlasManager& atlas);

private:
	void drawRect(SpriteBatch& sprite_batch, int x, int y, int w, int h, const glm::vec4& color, const AtlasManager& atlas, int width = 1);
	void drawFilledRect(SpriteBatch& sprite_batch, int x, int y, int w, int h, const glm::vec4& color, const AtlasManager& atlas);
};

#endif
