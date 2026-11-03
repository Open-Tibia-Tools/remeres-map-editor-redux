#ifndef RME_UI_SEARCH_SEARCH_CARD_DRAWER_H_
#define RME_UI_SEARCH_SEARCH_CARD_DRAWER_H_

#include "editor/operations/search_query.h"
#include <wx/gdicmn.h>

struct NVGcontext;
class NanoVGCanvas;

namespace rme::ui::search {

class SearchCardDrawer {
public:
	static void DrawEmptyState(
		NVGcontext* vg,
		int width,
		int height,
		int scroll_pos,
		bool is_editor_open
	);

	static void DrawGroupHeader(
		NVGcontext* vg,
		NanoVGCanvas* canvas,
		const wxRect& rect,
		const SearchResultGroup& group,
		size_t group_idx,
		bool selected,
		bool hovered
	);

	static void DrawContainerCard(
		NVGcontext* vg,
		NanoVGCanvas* canvas,
		const wxRect& rect,
		const SearchResultChildEntry& child,
		size_t child_idx,
		size_t total_children,
		bool selected,
		bool hovered
	);

	static void DrawChildCard(
		NVGcontext* vg,
		NanoVGCanvas* canvas,
		const wxRect& rect,
		const SearchResultChildEntry& child,
		size_t child_idx,
		size_t total_children,
		bool selected,
		bool hovered
	);

	static void DrawAttributeTokens(
		NVGcontext* vg,
		float start_x,
		float y,
		const SearchResultChildEntry& child,
		float gap,
		bool selected
	);

private:
	static int GetTextureForGroup(NVGcontext* vg, NanoVGCanvas* canvas, const SearchResultGroup& group);
};

} // namespace rme::ui::search

#endif // RME_UI_SEARCH_SEARCH_CARD_DRAWER_H_
