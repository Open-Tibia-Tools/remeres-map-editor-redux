//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_EDITOR_SPRITE_H_
#define RME_RENDERING_CORE_EDITOR_SPRITE_H_

#include "rendering/core/graphics.h"
#include <memory>

class EditorSprite : public Sprite {
public:
	EditorSprite(std::unique_ptr<wxBitmap> b16x16, std::unique_ptr<wxBitmap> b32x32);
	~EditorSprite() override = default;

	ImageDimensions GetSize() const override {
		return ImageDimensions { 32, 32 };
	}

	wxBitmap* getBitmap(SpriteSize sz) const {
		if (static_cast<size_t>(sz) < SPRITE_SIZE_COUNT) {
			return bm[sz].get();
		}
		return nullptr;
	}

protected:
	std::unique_ptr<wxBitmap> bm[SPRITE_SIZE_COUNT];
};

#endif
