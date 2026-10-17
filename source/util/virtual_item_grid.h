#ifndef RME_UTIL_VIRTUAL_ITEM_GRID_H_
#define RME_UTIL_VIRTUAL_ITEM_GRID_H_

#include "app/main.h"
#include "util/nanovg_canvas.h"
#include "util/grid_view_options.h"
#include <vector>

/**
 * @class VirtualItemGrid
 * @brief High-performance grid for displaying items using NanoVG.
 * Based on VirtualBrushGrid logic but tailored for generic Items.
 */
class VirtualItemGrid : public NanoVGCanvas {
public:
	VirtualItemGrid(wxWindow* parent, wxWindowID id = wxID_ANY);
	virtual ~VirtualItemGrid();

	// Data Provider interface - simple virtuals for now
	virtual size_t GetItemCount() const = 0;
	virtual uint16_t GetItem(size_t index) const = 0;
	virtual wxString GetItemName(size_t index) const; // Optional override

	// View options, shared with the palette tileset grid toolbar
	void SetShowLabels(bool show);
	bool GetShowLabels() const {
		return m_showLabels;
	}
	void SetIconSize(int sizePx);
	int GetIconSize() const {
		return m_iconSize;
	}
	void SetSort(TilesetSortKey key, TilesetSortDirection dir);
	void ClearSort();
	bool HasSort() const {
		return m_hasSort;
	}
	TilesetSortKey GetSortKey() const {
		return m_sortKey;
	}
	TilesetSortDirection GetSortDirection() const {
		return m_sortDir;
	}

	// Selection
	void SetSelection(int index);
	int GetSelection() const {
		return m_selectedIndex;
	}
	uint16_t GetSelectedItemId() const;

	// View control
	void RefreshGrid(); // Re-calculates layout and repaints
	void EnsureVisible(int index);
	void ClearCache();

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override;
	wxSize DoGetBestClientSize() const override;

	// Events
	virtual void OnMouseDown(wxMouseEvent& event);
	virtual void OnMotion(wxMouseEvent& event);
	virtual void OnLeave(wxMouseEvent& event);
	void OnSize(wxSizeEvent& event);
	void OnTimer(wxTimerEvent& event);

	// Virtual hooks for interactions
	virtual void OnItemSelected(int index) { }

	// Layout
	void UpdateLayout();
	int HitTest(int x, int y) const;
	wxRect GetItemRect(int index) const;

	/**
	 * Sorts item ids with the active sort options. Derived grids call this
	 * after rebuilding the list they display.
	 */
	void ApplySortTo(std::vector<uint16_t>& items) const;
	virtual std::string GetSortName(uint16_t id) const;

	/**
	 * Card size implied by the icon size and the label toggle.
	 */
	int CardSize() const;

	// Rendering helpers
	int GetOrCreateItemTexture(NVGcontext* vg, uint16_t itemId);

	// Member variables
	int m_padding;
	int m_iconSize;
	bool m_showLabels;
	TilesetSortKey m_sortKey = TilesetSortKey::Name;
	TilesetSortDirection m_sortDir = TilesetSortDirection::Ascending;
	bool m_hasSort = false;
	int m_columns;
	int m_selectedIndex;
	int m_hoverIndex;

	wxTimer m_animTimer;
};

#endif
