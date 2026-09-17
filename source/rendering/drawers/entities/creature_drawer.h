//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CREATURE_DRAWER_H_
#define RME_RENDERING_CREATURE_DRAWER_H_

#include "app/definitions.h"
#include "game/outfit.h"
#include "game/creature.h"

// Forward declarations

class SpriteDrawer;
class SpriteBatch;

#include <optional>
#include "map/position.h"

struct LightBuffer;
struct RenderView;
struct RenderFrameContext;

struct CreatureDrawOptions {
	DrawColor color;
	bool ingame = false;
	int animationPhase = 0;
	Position map_pos;
	std::optional<MapBounds> transient_selection_bounds;
	LightBuffer* light_buffer = nullptr;
	const RenderView* view = nullptr;
	bool preview_local_player = false;
	const RenderFrameContext* ctx = nullptr;
};

class CreatureDrawer {
public:
	CreatureDrawer();
	~CreatureDrawer();

	void BlitCreature(SpriteBatch& sprite_batch, SpriteDrawer* sprite_drawer, int screenx, int screeny, const Creature* c, const CreatureDrawOptions& options = {});
	void BlitCreature(SpriteBatch& sprite_batch, SpriteDrawer* sprite_drawer, int screenx, int screeny, const Outfit& outfit, Direction dir, const CreatureDrawOptions& options = {});
};

#endif
