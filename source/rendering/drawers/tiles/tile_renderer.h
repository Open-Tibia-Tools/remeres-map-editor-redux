#ifndef RME_RENDERING_TILE_RENDERER_H_
#define RME_RENDERING_TILE_RENDERER_H_

#include <cstddef>
#include <memory>
#include <sstream>
#include <stdint.h>

class TileLocation;
class Tile;
class Item;
struct RenderView;
struct DrawingOptions;
class Editor;
class ItemDrawer;
class SpriteDrawer;
class CreatureDrawer;
class CreatureNameDrawer;
class FloorDrawer;
class MarkerDrawer;
class TooltipDrawer;
struct LightBuffer;
class SpriteBatch;
class PrimitiveRenderer;
struct SpritePatterns;
class ItemDefinitionView;

struct RenderFrameContext;

class TileRenderer {
public:
	TileRenderer(ItemDrawer* id, SpriteDrawer* sd, CreatureDrawer* cd, CreatureNameDrawer* cnd, FloorDrawer* fd, MarkerDrawer* md, TooltipDrawer* td, Editor* ed);

	void DrawTile(SpriteBatch& ground_batch, SpriteBatch& item_batch, const TileLocation* location, const RenderFrameContext& ctx, int in_draw_x = -1, int in_draw_y = -1, LightBuffer* light_buffer = nullptr, bool light_collection_only = false) const;
	void DrawTile(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int in_draw_x = -1, int in_draw_y = -1, LightBuffer* light_buffer = nullptr, bool light_collection_only = false) const;
	void DrawTile(SpriteBatch& sprite_batch, const TileLocation* location, const RenderView& view, const DrawingOptions& options, uint32_t current_house_id, int in_draw_x = -1, int in_draw_y = -1, LightBuffer* light_buffer = nullptr, bool light_collection_only = false) const;
	void CollectTooltips(const TileLocation* location, const RenderFrameContext& ctx) const;
	void DrawCreature(SpriteBatch& sprite_batch, const TileLocation* location, const RenderFrameContext& ctx, int draw_x, int draw_y, LightBuffer* light_buffer = nullptr) const;
	void RegisterGroundLightOcclusion(const TileLocation* location, const RenderView& view, LightBuffer& light_buffer, uint32_t floor_light_start) const;

private:
	void PreloadItem(const Tile* tile, Item* item, const ItemDefinitionView& definition, const SpritePatterns* patterns = nullptr);

	ItemDrawer* item_drawer;
	SpriteDrawer* sprite_drawer;
	CreatureDrawer* creature_drawer;
	FloorDrawer* floor_drawer;
	MarkerDrawer* marker_drawer;
	TooltipDrawer* tooltip_drawer;
	CreatureNameDrawer* creature_name_drawer;
	Editor* editor;
};

#endif
