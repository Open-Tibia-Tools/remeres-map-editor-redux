//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_DRAG_SHADOW_DRAWER_H_
#define RME_RENDERING_DRAG_SHADOW_DRAWER_H_

#include "map/position.h"
#include <optional>

class Editor;
struct RenderView;
struct DrawingOptions;

class ItemDrawer;
class SpriteDrawer;
class CreatureDrawer;
class SpriteBatch;
class PrimitiveRenderer;
struct RenderFrameContext;

class DragShadowDrawer {
public:
	DragShadowDrawer();
	~DragShadowDrawer();

	void draw(SpriteBatch& sprite_batch, Editor& editor, const std::optional<Position>& drag_start, ItemDrawer* item_drawer, SpriteDrawer* sprite_drawer, CreatureDrawer* creature_drawer, const RenderView& view, const DrawingOptions& options, const RenderFrameContext* ctx = nullptr);
};

#endif
