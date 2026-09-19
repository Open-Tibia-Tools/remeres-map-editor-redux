//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_ICONS_EDITOR_ICON_H_
#define RME_UI_ICONS_EDITOR_ICON_H_

#include <memory>
#include <wx/bitmap.h>
#include "rendering/core/game_sprite.h"

class EditorIcon {
public:
	EditorIcon() = default;
	EditorIcon(std::unique_ptr<wxBitmap> b16x16, std::unique_ptr<wxBitmap> b32x32);
	explicit EditorIcon(std::unique_ptr<wxBitmap> b32x32);
	~EditorIcon() = default;

	EditorIcon(const EditorIcon&) = delete;
	EditorIcon& operator=(const EditorIcon&) = delete;
	EditorIcon(EditorIcon&&) noexcept = default;
	EditorIcon& operator=(EditorIcon&&) noexcept = default;

	[[nodiscard]] wxBitmap* getBitmap(SpriteSize sz) const {
		if (static_cast<size_t>(sz) < SPRITE_SIZE_COUNT) {
			return bm[sz].get();
		}
		return nullptr;
	}

private:
	std::unique_ptr<wxBitmap> bm[SPRITE_SIZE_COUNT];
};

#endif
