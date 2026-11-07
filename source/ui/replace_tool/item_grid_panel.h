#ifndef RME_ITEM_GRID_PANEL_H_
#define RME_ITEM_GRID_PANEL_H_

#include "util/virtual_item_grid.h"
#include <vector>
#include <map>

class ItemGridPanel : public VirtualItemGrid {
public:
	class Listener {
	public:
		virtual ~Listener() = default;
		virtual void OnItemSelected(ItemGridPanel* source, uint16_t itemId) = 0;
	};

	ItemGridPanel(wxWindow* parent, Listener* listener);
	virtual ~ItemGridPanel();

	// Data Management
	void SetItems(const std::vector<uint16_t>& items, std::map<uint16_t, wxString> names = {});
	void SetFilter(const wxString& filter);

	// VirtualItemGrid Implementation
	size_t GetItemCount() const override;
	uint16_t GetItem(size_t index) const override;
	wxString GetItemName(size_t index) const override;
	wxString GetItemTooltip(size_t index) const override;
	void OnItemSelected(int index) override;
	std::string GetSortName(uint16_t id) const override;
	bool IsItemSelected(int index) const override;

	// Overrides
	void OnMotion(wxMouseEvent& event) override; // Drag&Drop Hook
	void OnMouseDown(wxMouseEvent& event) override; // Ctrl / Shift multi select
	void OnMouseUp(wxMouseEvent& event);

	/** The items selected with Ctrl or Shift, in the order they were picked. */
	const std::vector<uint16_t>& GetSelectedIds() const {
		return m_selectedIds;
	}
	size_t GetSelectedCount() const {
		return m_selectedIds.size();
	}

	const std::vector<uint16_t>& GetItems() const {
		return allItems;
	}

	// Custom properties
	void SetDraggable(bool check);
	void SetOverrideNames(const std::map<uint16_t, wxString>& names);
	void SetShowDetails(bool show);

	/** Drops the Ctrl/Shift selection, called whenever the shown items change. */
	void ClearSelectedIds();

	uint16_t GetSelectedId() const {
		return (uint16_t)GetSelectedItemId();
	}

protected:
	Listener* listener;

	/** The configured name for an entry, empty when the item has none. */
	wxString GetRawName(size_t index) const;
	/** Ctrl and Shift clicks build the bulk selection without touching suggestions. */
	void UpdateMultiSelection(int index, bool ctrl, bool shift);

	std::vector<uint16_t> allItems;
	std::vector<uint16_t> filteredItems;

	bool m_draggable = false;
	bool m_dragInProgress = false;
	bool m_showDetails = true;

	// Bulk selection for multi item drags. Ids, not indices, because the grid
	// re-sorts and re-filters underneath.
	std::vector<uint16_t> m_selectedIds;
	int m_anchorIndex = -1;
	int m_pendingSingleSelectIndex = -1;
	int m_pendingCtrlToggleIndex = -1;
	std::map<uint16_t, wxString> m_nameOverrides;
};

#endif
