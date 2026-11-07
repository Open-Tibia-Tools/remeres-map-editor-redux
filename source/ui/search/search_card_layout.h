#ifndef RME_UI_SEARCH_SEARCH_CARD_LAYOUT_H_
#define RME_UI_SEARCH_SEARCH_CARD_LAYOUT_H_

#include <wx/gdicmn.h>
#include <wx/window.h>
#include <cstddef>
#include <algorithm>

namespace rme::ui::search {

constexpr int kTreeIndent = 20;         // Indentation for child cards in the sub-tree
constexpr int kCardSpacing = 4;        // Vertical spacing between cards
constexpr int kGroupHeaderHeight = 38; // Matches rme::ui::grid::kListRowHeight + 2 for 32x32 icon
constexpr int kNormalChildHeight = 44; // Fits 32x32 icon + 2 lines of text
constexpr int kContainerCardHeight = 74; // Single card fitting Row 1 (32x32 items) + Row 2 (UID/AID/Position)
constexpr int kIconSize = 32;
constexpr int kIconGap = 4;
constexpr int kIconStep = kIconSize + kIconGap;

inline int GetIndexColumnWidth(size_t total_count) {
	if (total_count >= 1000) return 40;
	if (total_count >= 100) return 32;
	return 24;
}

struct CardGeometry {
	int card_x = 0;
	int card_y = 0;
	int card_w = 0;
	int card_h = 0;

	int index_x = 0;
	int index_w = 0;
	float center_y = 0.0f;

	int icon_x = 0;
	int icon_y = 0;
	int icon_sz = kIconSize;

	int content_x = 0;
	float top_y = 0.0f;
	float bottom_y = 0.0f;

	int container_icons_y = 0;

	static CardGeometry ForChild(const wxRect& drawn, size_t total_children, const wxWindow* win) {
		CardGeometry geo;
		geo.card_x = drawn.x;
		geo.card_y = drawn.y;
		geo.card_w = drawn.width;
		geo.card_h = drawn.height;

		const int dip_6 = win ? win->FromDIP(6) : 6;
		const int dip_8 = win ? win->FromDIP(8) : 8;
		const int dip_11 = win ? win->FromDIP(11) : 11;
		const int dip_13 = win ? win->FromDIP(13) : 13;

		geo.index_w = win ? win->FromDIP(GetIndexColumnWidth(total_children)) : GetIndexColumnWidth(total_children);
		geo.index_x = drawn.x + dip_6;
		geo.center_y = static_cast<float>(drawn.y + drawn.height / 2);

		geo.icon_x = geo.index_x + geo.index_w + dip_6;
		geo.icon_y = drawn.y + (drawn.height - geo.icon_sz) / 2;

		geo.content_x = geo.icon_x + geo.icon_sz + dip_8;
		geo.top_y = static_cast<float>(drawn.y + dip_13);
		geo.bottom_y = static_cast<float>(drawn.y + drawn.height - dip_11);

		return geo;
	}

	static CardGeometry ForContainer(const wxRect& drawn, size_t total_children, const wxWindow* win) {
		CardGeometry geo;
		geo.card_x = drawn.x;
		geo.card_y = drawn.y;
		geo.card_w = drawn.width;
		geo.card_h = drawn.height;

		const int dip_6 = win ? win->FromDIP(6) : 6;
		const int dip_8 = win ? win->FromDIP(8) : 8;
		const int dip_48 = win ? win->FromDIP(48) : 48;
		const int dip_64 = win ? win->FromDIP(64) : 64;

		geo.index_w = win ? win->FromDIP(GetIndexColumnWidth(total_children)) : GetIndexColumnWidth(total_children);
		geo.index_x = drawn.x + dip_6;
		geo.center_y = static_cast<float>(drawn.y + drawn.height / 2);

		geo.icon_x = geo.index_x + geo.index_w + dip_6;
		geo.icon_y = drawn.y + (drawn.height - geo.icon_sz) / 2;

		geo.content_x = geo.icon_x + geo.icon_sz + dip_8;
		geo.container_icons_y = drawn.y + dip_6;
		geo.top_y = static_cast<float>(drawn.y + dip_48);
		geo.bottom_y = static_cast<float>(drawn.y + dip_64);

		return geo;
	}

	[[nodiscard]] wxRect GetMainIconRect() const {
		return wxRect(icon_x, icon_y, icon_sz, icon_sz);
	}

	[[nodiscard]] wxRect GetContainedSlotRect(size_t index) const {
		return wxRect(content_x + static_cast<int>(index) * kIconStep, container_icons_y, icon_sz, icon_sz);
	}
};

} // namespace rme::ui::search

#endif // RME_UI_SEARCH_SEARCH_CARD_LAYOUT_H_
