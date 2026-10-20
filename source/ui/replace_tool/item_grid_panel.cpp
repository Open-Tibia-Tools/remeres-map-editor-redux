#include "ui/replace_tool/item_grid_panel.h"
#include "ui/theme.h"
#include "item_definitions/core/item_definition_store.h"
#include "ui/gui.h"
#include "app/managers/version_manager.h"
#include "util/common.h"
#include <wx/dcclient.h>
#include <wx/graphics.h>
#include <wx/dnd.h>
#include <wx/dataobj.h>
#include <algorithm>
#include <format>
#include <ranges>
#include <string>
#include "rendering/core/text_renderer.h"

ItemGridPanel::ItemGridPanel(wxWindow* parent, Listener* listener) :
	VirtualItemGrid(parent, wxID_ANY),
	listener(listener),
	m_draggable(false),
	m_showDetails(true) {
	Bind(wxEVT_LEFT_UP, &ItemGridPanel::OnMouseUp, this);
}

ItemGridPanel::~ItemGridPanel() {
}

void ItemGridPanel::SetItems(const std::vector<uint16_t>& items, std::map<uint16_t, wxString> names) {
	allItems = items;
	m_nameOverrides = std::move(names);
	ClearSelectedIds();
	ClearCache(); // Clear old textures
	SetFilter("");
}

void ItemGridPanel::ClearSelectedIds() {
	m_selectedIds.clear();
	m_anchorIndex = -1;
	m_pendingSingleSelectIndex = -1;
	m_pendingCtrlToggleIndex = -1;
}

bool ItemGridPanel::IsItemSelected(int index) const {
	if (index == GetSelection()) {
		return true;
	}
	const uint16_t id = GetItem(static_cast<size_t>(index));
	return std::ranges::find(m_selectedIds, id) != m_selectedIds.end();
}

void ItemGridPanel::UpdateMultiSelection(int index, bool ctrl, bool shift) {
	const uint16_t id = GetItem(static_cast<size_t>(index));
	if (id == 0) {
		return;
	}

	if (shift && m_anchorIndex >= 0 && m_anchorIndex < static_cast<int>(GetItemCount())) {
		// Range from the anchor to here.
		const int first = std::min(m_anchorIndex, index);
		const int last = std::max(m_anchorIndex, index);
		m_selectedIds.clear();
		for (int i = first; i <= last; ++i) {
			const uint16_t rangeId = GetItem(static_cast<size_t>(i));
			if (rangeId != 0) {
				m_selectedIds.push_back(rangeId);
			}
		}
		return;
	}

	if (ctrl) {
		if (m_selectedIds.empty() && GetSelection() >= 0 && GetSelection() < static_cast<int>(GetItemCount())) {
			const uint16_t primaryId = GetItem(static_cast<size_t>(GetSelection()));
			if (primaryId != 0 && primaryId != id) {
				m_selectedIds.push_back(primaryId);
			}
		}

		if (const auto it = std::ranges::find(m_selectedIds, id); it != m_selectedIds.end()) {
			m_selectedIds.erase(it);
		} else {
			m_selectedIds.push_back(id);
		}
	}

	m_anchorIndex = index;
}

void ItemGridPanel::OnMouseDown(wxMouseEvent& event) {
	const int index = HitTest(event.GetX(), event.GetY());
	if (index < 0) {
		event.Skip();
		return;
	}

	m_pendingSingleSelectIndex = -1;
	m_pendingCtrlToggleIndex = -1;

	if (event.ShiftDown()) {
		UpdateMultiSelection(index, false, true);
		Refresh();
		return;
	}

	if (event.ControlDown()) {
		// If clicking an already selected item with Ctrl, defer unselecting to mouse up.
		// If the user starts dragging before releasing the mouse, all selected items are dragged!
		if (IsItemSelected(index)) {
			m_pendingCtrlToggleIndex = index;
			m_anchorIndex = index;
		} else {
			UpdateMultiSelection(index, true, false);
			Refresh();
		}
		return;
	}

	// Normal click (no Ctrl, no Shift):
	// If the user clicks on an item that is ALREADY part of a multi-selection,
	// defer clearing the other items to mouse up. If the user drags, the entire
	// multi-selection is dragged!
	if (!m_selectedIds.empty() && IsItemSelected(index)) {
		m_pendingSingleSelectIndex = index;
		m_anchorIndex = index;
		return;
	}

	ClearSelectedIds();
	m_anchorIndex = index;
	VirtualItemGrid::OnMouseDown(event);
}

void ItemGridPanel::SetFilter(const wxString& filter) {
	filteredItems.clear();
	wxString lowerFilter = filter.Lower();
	for (uint16_t id : allItems) {
		if (id < 100) {
			continue;
		}
		const auto it = g_item_definitions.get(id);
		if (lowerFilter.IsEmpty()) {
			filteredItems.push_back(id);
		} else {
			const wxString name = it ? wxstr(std::string(it.name())) : wxString("<unknown item>");
			const bool matches_name = name.Lower().Contains(lowerFilter);
			const bool matches_id = wxstr(std::format("{}", id)).Contains(lowerFilter);
			const bool matches_client_id = it && wxstr(std::format("{}", it.clientId())).Contains(lowerFilter);
			if (matches_name || matches_id || matches_client_id) {
				filteredItems.push_back(id);
			}
		}
	}
	ApplySortTo(filteredItems);
	SetScrollPosition(0);
	RefreshGrid();
}

size_t ItemGridPanel::GetItemCount() const {
	return filteredItems.size();
}

uint16_t ItemGridPanel::GetItem(size_t index) const {
	if (index < filteredItems.size()) {
		return filteredItems[index];
	}
	return 0;
}

wxString ItemGridPanel::GetRawName(size_t index) const {
	uint16_t id = GetItem(index);
	if (const auto it = m_nameOverrides.find(id); it != m_nameOverrides.end()) {
		return it->second;
	}
	const auto definition = g_item_definitions.get(id);
	return definition ? wxstr(std::string(definition.name())) : wxString();
}

wxString ItemGridPanel::GetItemName(size_t index) const {
	// Items without a name fall back to their id, mirroring how the palette
	// labels its raw item entries.
	const wxString name = GetRawName(index);
	return name.IsEmpty() ? wxstr(std::format("{}", GetItem(index))) : name;
}

wxString ItemGridPanel::GetItemTooltip(size_t index) const {
	const uint16_t id = GetItem(index);
	const wxString name = GetRawName(index);
	return name.IsEmpty() ? wxstr(std::format("{}", id)) : wxstr(std::format("{} - {}", id, name.ToStdString()));
}

void ItemGridPanel::OnItemSelected(int index) {
	if (index >= 0 && index < (int)filteredItems.size()) {
		uint16_t id = filteredItems[index];
		if (listener) {
			listener->OnItemSelected(this, id);
		}
	}
}

void ItemGridPanel::OnMotion(wxMouseEvent& event) {
	// Call base to handle hover effects
	VirtualItemGrid::OnMotion(event);

	// Drag Check. DoDragDrop runs a modal loop, and wx keeps delivering motion
	// events into it, so a re-entry guard is what stops a drag being restarted
	// on every mouse move.
	if (event.Dragging() && m_draggable && !m_dragInProgress) {
		// A drag gesture has begun: cancel any deferred deselection so the
		// full selection is preserved and dragged.
		m_pendingSingleSelectIndex = -1;
		m_pendingCtrlToggleIndex = -1;

		// A Ctrl or Shift selection drags all of its items at once.
		std::string payload;
		if (!m_selectedIds.empty()) {
			payload = "RME_ITEM:";
			for (size_t i = 0; i < m_selectedIds.size(); ++i) {
				if (i > 0) {
					payload += ',';
				}
				payload += std::to_string(m_selectedIds[i]);
			}
		} else {
			const uint16_t id = GetSelectedItemId();
			if (id != 0) {
				payload = std::format("RME_ITEM:{}", id);
			}
		}

		if (!payload.empty()) {
			m_dragInProgress = true;
			wxTextDataObject data(wxstr(payload));
			wxDropSource dragSource(this);
			dragSource.SetData(data);
			dragSource.DoDragDrop(wxDrag_AllowMove);
			m_dragInProgress = false;
		}
	}
}

void ItemGridPanel::OnMouseUp(wxMouseEvent& event) {
	if (m_pendingSingleSelectIndex != -1) {
		const int index = m_pendingSingleSelectIndex;
		m_pendingSingleSelectIndex = -1;
		ClearSelectedIds();
		m_anchorIndex = index;
		SetSelection(index);
		Refresh();
	} else if (m_pendingCtrlToggleIndex != -1) {
		const int index = m_pendingCtrlToggleIndex;
		m_pendingCtrlToggleIndex = -1;
		UpdateMultiSelection(index, true, false);
		Refresh();
	}
	event.Skip();
}

std::string ItemGridPanel::GetSortName(uint16_t id) const {
	auto it = m_nameOverrides.find(id);
	if (it != m_nameOverrides.end()) {
		return it->second.ToStdString();
	}
	return VirtualItemGrid::GetSortName(id);
}

void ItemGridPanel::SetOverrideNames(const std::map<uint16_t, wxString>& names) {
	m_nameOverrides = names;
	ApplySortTo(filteredItems);
	Refresh();
}

void ItemGridPanel::SetShowDetails(bool show) {
	m_showDetails = show;
	Refresh();
}

void ItemGridPanel::SetDraggable(bool check) {
	m_draggable = check;
}
