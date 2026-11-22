//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_TILE_PROPERTIES_WIDTH_H_
#define RME_UI_TILE_PROPERTIES_WIDTH_H_

#include <algorithm>
#include <span>

namespace TilePropertiesWidth {

	// Widest of the property sections. Hidden sections count too, because any of them can be shown later.
	[[nodiscard]] constexpr int WidestSection(std::span<const int> section_widths) noexcept {
		int widest = 0;
		for (const int width : section_widths) {
			widest = std::max(widest, width);
		}
		return widest;
	}

	// Width a vertical wxSplitterWindow needs so neither pane drops below its minimum.
	// The splitter keeps one border outside each pane and the sash between them.
	[[nodiscard]] constexpr int SplitterMinWidth(int left_min, int right_min, int sash, int border) noexcept {
		return border + left_min + sash + right_min + border;
	}

} // namespace TilePropertiesWidth

#endif
