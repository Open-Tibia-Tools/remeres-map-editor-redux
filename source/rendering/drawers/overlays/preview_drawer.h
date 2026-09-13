#ifndef RME_PREVIEW_DRAWER_H_
#define RME_PREVIEW_DRAWER_H_

#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include <cstdint>

class MapCanvas;
class Editor;
class ItemDrawer;
class SpriteDrawer;
class CreatureDrawer;
class SpriteBatch;

class PrimitiveRenderer;
struct RenderFrameContext;

class PreviewDrawer {
public:
	PreviewDrawer();
	~PreviewDrawer();

	void draw(SpriteBatch& sprite_batch, MapCanvas* canvas, const RenderView& view, int map_z, const DrawingOptions& options, Editor& editor, ItemDrawer* item_drawer, SpriteDrawer* sprite_drawer, CreatureDrawer* creature_drawer, uint32_t current_house_id, const RenderFrameContext* ctx = nullptr);
};

#endif
