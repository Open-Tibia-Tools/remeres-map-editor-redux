#include "ui/controls/collection_item_grid.h"

#include "util/common.h"
#include "item_definitions/core/item_definition_store.h"
#include <algorithm>

namespace rme::ui {

CollectionItemGrid::CollectionItemGrid(wxWindow* parent, wxWindowID id) :
	VirtualItemGrid(parent, id) {
}

void CollectionItemGrid::SetItems(const std::vector<uint16_t>& items) {
	m_allItems = items;
	ApplyFilterAndSort();
}

void CollectionItemGrid::SetFilter(const std::string& query) {
	m_filterQuery = query;
	ApplyFilterAndSort();
	SetScrollPosition(0);
}

void CollectionItemGrid::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	VirtualItemGrid::SetSort(key, dir);
	ApplyFilterAndSort();
}

void CollectionItemGrid::ClearSort() {
	VirtualItemGrid::ClearSort();
	ApplyFilterAndSort();
}

void CollectionItemGrid::ApplyFilterAndSort() {
	if (m_filterQuery.empty()) {
		m_filteredItems = m_allItems;
	} else {
		m_filteredItems.clear();
		std::string lowerQuery = as_lower_str(m_filterQuery);
		for (uint16_t id : m_allItems) {
			std::string name = as_lower_str(GetSortName(id));
			std::string idStr = std::to_string(id);
			if (name.find(lowerQuery) != std::string::npos || idStr.find(lowerQuery) != std::string::npos) {
				m_filteredItems.push_back(id);
			}
		}
	}

	if (HasSort()) {
		ApplySortTo(m_filteredItems);
	}

	UpdateLayout();
	Refresh();
}

void CollectionItemGrid::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	VirtualItemGrid::OnNanoVGPaint(vg, width, height);

	if (m_hoverIndex >= 0 && m_hoverIndex < static_cast<int>(m_filteredItems.size())) {
		wxRect cell = GetItemRect(m_hoverIndex);
		rme::ui::grid::DrawRemoveBadge(vg, cell, m_badgeHovered);
	}
}

void CollectionItemGrid::OnMotion(wxMouseEvent& event) {
	int idx = HitTest(event.GetX(), event.GetY());
	bool badgeHovered = false;
	if (idx != -1 && idx < static_cast<int>(m_filteredItems.size())) {
		wxRect cell = GetItemRect(idx);
		cell.y -= GetScrollPosition();
		wxRect badgeRect = rme::ui::grid::GetBadgeRect(cell);
		badgeHovered = badgeRect.Contains(event.GetPosition());
	}

	if (badgeHovered != m_badgeHovered) {
		m_badgeHovered = badgeHovered;
		Refresh();
	}

	VirtualItemGrid::OnMotion(event);
}

void CollectionItemGrid::OnMouseDown(wxMouseEvent& event) {
	int idx = HitTest(event.GetX(), event.GetY());
	if (idx != -1 && idx < static_cast<int>(m_filteredItems.size())) {
		wxRect cell = GetItemRect(idx);
		cell.y -= GetScrollPosition();
		wxRect badgeRect = rme::ui::grid::GetBadgeRect(cell);
		if (badgeRect.Contains(event.GetPosition())) {
			uint16_t id = m_filteredItems[idx];
			if (m_discardCallback) {
				m_discardCallback(id);
			}
			return;
		}
	}

	VirtualItemGrid::OnMouseDown(event);
}

void CollectionItemGrid::OnLeave(wxMouseEvent& event) {
	m_badgeHovered = false;
	VirtualItemGrid::OnLeave(event);
}

} // namespace rme::ui
