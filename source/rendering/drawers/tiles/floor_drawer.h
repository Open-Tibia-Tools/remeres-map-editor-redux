//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_FLOOR_DRAWER_H_
#define RME_RENDERING_FLOOR_DRAWER_H_

struct RenderView;
struct DrawingOptions;
class BaseMap;

class ItemDrawer;
class SpriteDrawer;

class CreatureDrawer;
class SpriteBatch;
class PrimitiveRenderer;

class FloorDrawer {
public:
	FloorDrawer();
	~FloorDrawer();

	void draw(SpriteBatch& sprite_batch, ItemDrawer* item_drawer, SpriteDrawer* sprite_drawer, CreatureDrawer* creature_drawer, const RenderView& view, const DrawingOptions& options, BaseMap& map);
};

#endif
