#ifndef RME_RENDERING_BRUSH_CURSOR_DRAWER_H_
#define RME_RENDERING_BRUSH_CURSOR_DRAWER_H_

#include <cstdint>

class Brush;
class SpriteBatch;
class PrimitiveRenderer;
class AtlasManager;

class BrushCursorDrawer {
public:
	void draw(SpriteBatch& sprite_batch, PrimitiveRenderer& primitive_renderer, const AtlasManager& atlas, int x, int y, Brush* brush, uint8_t r, uint8_t g, uint8_t b);
};

#endif
