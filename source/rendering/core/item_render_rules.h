//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_ITEM_RENDER_RULES_H_
#define RME_RENDERING_CORE_ITEM_RENDER_RULES_H_

#include "rendering/core/render_depth.h"

namespace rme::rendering {

    // Single source of truth for the item depth rules shared by the tile renderer,
    // the chunk-cache baker and the selection picker.

    // Resolves the render layer of a stacked item from its definition flags.
    // Border items are handled by the terrain/border passes and are never passed here.
    [[nodiscard]] constexpr RenderSublayer ResolveItemSublayer(bool always_on_bottom, bool top_effect) noexcept
    {
        if (always_on_bottom) {
            return RenderSublayer::BottomItem;
        }
        if (top_effect) {
            return RenderSublayer::TopItem;
        }
        return RenderSublayer::CommonItem;
    }

// Common items receive one of four depth steps (0..3) depending on their position
// in the stack. Values outside that range are clamped, so a caller counter cannot
// silently overflow the depth step.
[[nodiscard]] constexpr int CommonItemElevationStep(int common_index) noexcept {
	if (common_index < 0) {
		return 0;
	}
	if (common_index > 3) {
		return 3;
	}
	return common_index;
}

// Advances the pixel elevation used to place subsequent items on a tile.
// Only items that actually reach the sprite drawer contribute to it.
constexpr void AdvanceItemElevation(int& elevation_px, int draw_height, bool drawn) noexcept {
	if (drawn) {
		elevation_px += draw_height;
	}
}

} // namespace rme::rendering

#endif
