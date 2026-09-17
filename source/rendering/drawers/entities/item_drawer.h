//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_ITEM_DRAWER_H_
#define RME_RENDERING_ITEM_DRAWER_H_

#include "app/definitions.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/position.h"

// Forward declarations
class SpriteDrawer;
class CreatureDrawer;
class Tile;
class Item;
class GameSprite;
struct RenderView;
struct RenderFrameContext;

struct DrawingOptions;
class SpriteBatch;
struct SpritePatterns;

struct BlitItemParams {
	const Tile* tile = nullptr;
	Position pos;
	Item* item = nullptr;
	ItemDefinitionView item_definition;
	GameSprite* sprite = nullptr;
	const DrawingOptions* options = nullptr;
	const SpritePatterns* patterns = nullptr;
	bool ephemeral = false;
	int red = 255;
	int green = 255;
	int blue = 255;
	int alpha = 255;
	const RenderView* view = nullptr;
	const RenderFrameContext* ctx = nullptr;

	BlitItemParams(const Tile* t, Item* i, const DrawingOptions& o);
	BlitItemParams(const Position& p, Item* i, const DrawingOptions& o);
};

class ItemDrawer {
public:
	ItemDrawer();
	~ItemDrawer();

	void BlitItem(SpriteBatch& sprite_batch, SpriteDrawer* sprite_drawer, CreatureDrawer* creature_drawer, int& draw_x, int& draw_y, const BlitItemParams& params);

	void DrawRawBrush(SpriteBatch& sprite_batch, SpriteDrawer* sprite_drawer, int screenx, int screeny, ServerItemId item_id, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha, const RenderFrameContext* ctx = nullptr);
};

#endif
