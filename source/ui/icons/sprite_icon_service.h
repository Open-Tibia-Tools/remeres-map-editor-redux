//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_ICONS_SPRITE_ICON_SERVICE_H_
#define RME_UI_ICONS_SPRITE_ICON_SERVICE_H_

#include "rendering/core/game_sprite.h"
#include "game/creature.h"
#include <wx/bitmap.h>
#include <wx/dc.h>

class Sprite;
class GameSprite;
class EditorIcon;

class SpriteIconService {
public:
	static wxBitmap Generate(GameSprite* sprite, SpriteSize size, bool rescale = true);
	static wxBitmap Generate(GameSprite* sprite, SpriteSize size, const Outfit& outfit, bool rescale = true, Direction direction = SOUTH);

	static void DrawTo(Sprite* sprite, wxDC* dc, SpriteSize sz, int start_x, int start_y, int width = -1, int height = -1);
	static void DrawTo(EditorIcon* icon, wxDC* dc, SpriteSize sz, int start_x, int start_y, int width = -1, int height = -1);
	static void DrawTo(GameSprite* sprite, wxDC* dc, SpriteSize sz, const Outfit& outfit, int start_x, int start_y, int width = -1, int height = -1);
};

#endif
