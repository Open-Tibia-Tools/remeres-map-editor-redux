#ifndef RME_UTIL_GRID_CELL_RENDERER_H_
#define RME_UTIL_GRID_CELL_RENDERER_H_

#include "app/main.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include "ui/theme.h"

#include <nanovg.h>
#include <wx/gdicmn.h>
#include <wx/string.h>

#include <string>
#include <unordered_map>

/**
 * One implementation of a tileset/item grid cell.
 *
 * The palette tileset grid and the replace tool item grids used to carry their
 * own copies of the cell metrics, the card chrome and the label layout, which
 * is why the same tile rendered differently in each pane. Everything that
 * decides how a cell looks lives here now, so both grids are identical by
 * construction.
 */
namespace rme::ui::grid {

	// Cell geometry: the icon plus a 2px inset on every side, and an optional
	// fixed-height label band underneath.
	inline constexpr int kPadding = 4; // gutter between cells and around the grid
	inline constexpr int kIconOffset = 2; // inset of the icon inside its cell
	inline constexpr int kLabelHeight = 32; // height of the label band
	inline constexpr int kListRowHeight = 36; // row height in list mode
	inline constexpr int kIconSizeBase = 32; // icon size used by list rows
	inline constexpr int kLabelFontSize = 11;
	inline constexpr int kListLabelFontSize = 14;

	/** Width of a cell: the icon plus its inset. A cell is never wider than this. */
	inline int CellWidth(int iconSizePx) {
		return iconSizePx + 2 * kIconOffset;
	}

	/** Height of a cell: the icon box plus the label band when labels are on. */
	inline int CellHeight(int iconSizePx, bool showLabels) {
		return CellWidth(iconSizePx) + (showLabels ? kLabelHeight : 0);
	}

	/** The icon rect inside a cell: inset by kIconOffset, centred horizontally. */
	inline wxRect IconRect(const wxRect& cell, int iconSizePx) {
		return wxRect(
			cell.x + (cell.width - iconSizePx) / 2,
			cell.y + kIconOffset,
			iconSizePx,
			iconSizePx
		);
	}

	/** Theme role as an NVG colour, with optional alpha modulation. */
	inline NVGcolor NvgColor(Theme::Role role, int alpha = 255) {
		const wxColour colour = Theme::Get(role);
		return nvgRGBA(colour.Red(), colour.Green(), colour.Blue(), static_cast<unsigned char>(alpha));
	}

	/** A cell label, already split into the two lines a cell can show. */
	struct CellLabel {
		std::string line1;
		std::string line2;
	};

	/**
	 * Wraps cell labels to the cell width and caches the result.
	 *
	 * Splits on " - " first, then on a word boundary, then mid-word, and
	 * ellipsises the overflow, so a long name never spills into its neighbours.
	 */
	class LabelCache {
	public:
		void Clear();
		const CellLabel& Get(NVGcontext* vg, const wxString& text, float maxWidth);

	private:
		std::unordered_map<std::string, CellLabel> m_entries;
	};

	/**
	 * Card background and its hover/selection treatment, including the hover
	 * grow shared by both grids. Returns the rect the card was actually drawn
	 * at, so the caller can place the icon and label inside the grown card.
	 */
	wxRect DrawCardChrome(NVGcontext* vg, const wxRect& cell, bool selected, bool hovered, float hoverAnim);

	/**
	 * Cell content: the indicator badge when the entry has one, otherwise the
	 * sprite, otherwise a placeholder.
	 */
	void DrawCellIcon(NVGcontext* vg, const wxRect& iconRect, int texture, rme::rendering::TileIndicatorType badge);

	/** Centred label under the icon, wrapped to the cell width. */
	void DrawCellLabel(NVGcontext* vg, const wxRect& cell, const wxString& text, bool selected, LabelCache& cache);

	inline constexpr int kBadgeSize = 12;

	/** The remove ("x") badge rect for a given card cell. Located at top-right corner. */
	inline wxRect GetBadgeRect(const wxRect& cell) {
		return wxRect(cell.GetRight() - kBadgeSize - 2, cell.y + 2, kBadgeSize, kBadgeSize);
	}

	/** Draws the 18px remove badge with an "x" icon inside a card cell. */
	void DrawRemoveBadge(NVGcontext* vg, const wxRect& cell, bool hovered);

	/** Single-line label beside the icon, used by the palette list mode. */
	void DrawListLabel(NVGcontext* vg, const wxRect& row, const wxString& text);

} // namespace rme::ui::grid

#endif
