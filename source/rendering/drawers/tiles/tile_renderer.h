#ifndef RME_RENDERING_TILE_RENDERER_H_
#define RME_RENDERING_TILE_RENDERER_H_

#include <cstddef>
#include <memory>
#include <stdint.h>

class TileLocation;
class Tile;
class Item;
class ItemDrawer;
class SpriteDrawer;
class CreatureDrawer;
class CreatureNameDrawer;
class FloorDrawer;
class MarkerDrawer;
class SpriteBatch;

struct RenderFrameContext;

class Editor;

struct TileElevationState {
	int current_draw_x = 0;
	int current_draw_y = 0;
};

class TileRenderer {
public:
	TileRenderer(ItemDrawer* id, SpriteDrawer* sd, CreatureDrawer* cd, CreatureNameDrawer* cnd, FloorDrawer* fd, MarkerDrawer* md, Editor* ed);

	void DrawTile(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int in_draw_x = -1, int in_draw_y = -1, const Tile* tile_above = nullptr) const;

	// Layered pass rendering
	void RenderStaticTerrain(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int draw_x, int draw_y, const Tile* tile_above = nullptr) const;
	void RenderStaticItems(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, TileElevationState& elevation) const;
	void RenderAnimatedItems(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, TileElevationState& elevation) const;
	void RenderDynamicEntities(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int draw_x, int draw_y, bool render_creature_sprites = true) const;
	void RenderDynamicPasses(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int draw_x, int draw_y, const Tile* tile_above = nullptr) const;

private:
	ItemDrawer* item_drawer;
	SpriteDrawer* sprite_drawer;
	CreatureDrawer* creature_drawer;
	FloorDrawer* floor_drawer;
	MarkerDrawer* marker_drawer;
	CreatureNameDrawer* creature_name_drawer;
	Editor* editor;
};

#endif
