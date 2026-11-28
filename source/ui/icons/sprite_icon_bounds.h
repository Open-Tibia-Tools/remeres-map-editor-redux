//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_ICONS_SPRITE_ICON_BOUNDS_H_
#define RME_UI_ICONS_SPRITE_ICON_BOUNDS_H_

#include <algorithm>

inline constexpr int kMaxSpriteIconDimension = 1024;

[[nodiscard]] constexpr int ComputeBoundedIconDimension(int width, int height, int max_dimension = kMaxSpriteIconDimension) noexcept
{
    if (width <= 0 || height <= 0 || max_dimension <= 0) {
        return 0;
    }
    return std::min(std::max(width, height), max_dimension);
}

#endif
