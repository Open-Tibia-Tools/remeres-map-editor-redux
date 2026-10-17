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
#include "rendering/core/text_renderer.h"

ItemGridPanel::ItemGridPanel(wxWindow* parent, Listener* listener) :
	VirtualItemGrid(parent, wxID_ANY),
	listener(listener),
	m_draggable(false),
	m_showDetails(true) {
	// Base class handles bindings
}

ItemGridPanel::~ItemGridPanel() {
}

void ItemGridPanel::SetItems(const std::vector<uint16_t>& items, std::map<uint16_t, wxString> names) {
	allItems = items;
	m_nameOverrides = std::move(names);
	ClearCache(); // Clear old textures
	SetFilter("");
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

wxString ItemGridPanel::GetItemName(size_t index) const {
	uint16_t id = GetItem(index);
	auto it = m_nameOverrides.find(id);
	if (it != m_nameOverrides.end()) {
		return it->second;
	}
	const auto definition = g_item_definitions.get(id);
	const std::string_view name = definition ? definition.name() : std::string_view("<unknown item>");
	return wxstr(std::format("{} - {}", id, name));
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

	// Drag Check
	if (event.Dragging() && m_draggable) {
		uint16_t id = GetSelectedItemId();
		if (id != 0) {
			wxTextDataObject data(wxstr(std::format("RME_ITEM:{}", id)));
			wxDropSource dragSource(this);
			dragSource.SetData(data);
			dragSource.DoDragDrop(wxDrag_AllowMove);
		}
	}
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
