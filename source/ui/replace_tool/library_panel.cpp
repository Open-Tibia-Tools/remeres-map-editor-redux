#include "app/main.h"
#include <wx/log.h>
#include "ui/replace_tool/library_panel.h"
#include "ui/replace_tool/visual_similarity_service.h"
#include "ui/theme.h"
#include "item_definitions/core/item_definition_store.h"
#include "game/item.h"
#include "brushes/brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include <wx/splitter.h>
#include <algorithm>

LibraryPanel::LibraryPanel(wxWindow* parent, Listener* listener) :
	wxPanel(parent, wxID_ANY),
	m_listener(listener) {

	InitLayout();

	// Pre-fill item list
	std::vector<uint16_t> ids;
	ids.reserve(g_item_definitions.getMaxID());
	for (const ServerItemId id : g_item_definitions.allIds()) {
		if (id != 0) {
			ids.push_back(id);
		}
	}
	m_allItemsGrid->SetItems(ids);

	PopulateBrushGrid();
}

LibraryPanel::~LibraryPanel() { }

void LibraryPanel::InitLayout() {
	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
	int padding = Theme::Grid(2);

	m_notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxBORDER_NONE);

	// PAGE 1: Item List
	wxPanel* itemListPage = new wxPanel(m_notebook);
	wxBoxSizer* itemListSizer = new wxBoxSizer(wxVERTICAL);

	m_allItemsGrid = new ItemGridPanel(itemListPage, this);
	m_allItemsGrid->SetDraggable(true);

	TilesetGridToolbar::Actions itemActions;
	itemActions.filter = [this](const std::string& query) {
		m_allItemsGrid->SetFilter(wxstr(query));
	};
	itemActions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		m_allItemsGrid->SetSort(key, dir);
		m_allItemsGrid->SetFilter(wxstr(m_itemsToolbar->GetFilterQuery()));
	};
	itemActions.clear_sort = [this]() {
		m_allItemsGrid->ClearSort();
		m_allItemsGrid->SetFilter(wxstr(m_itemsToolbar->GetFilterQuery()));
	};
	itemActions.show_labels = [this](bool show) {
		m_allItemsGrid->SetShowLabels(show);
	};
	itemActions.icon_size = [this](int size) {
		m_allItemsGrid->SetIconSize(size);
	};

	m_itemsToolbar = new TilesetGridToolbar(itemListPage, std::move(itemActions));
	m_itemsToolbar->SetState(m_allItemsGrid->GetShowLabels(), m_allItemsGrid->GetIconSize(), false, TilesetSortKey::Name, TilesetSortDirection::Ascending, false);
	itemListSizer->Add(m_itemsToolbar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, padding);

	itemListSizer->Add(m_allItemsGrid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, padding);

	itemListPage->SetSizer(itemListSizer);
	m_notebook->AddPage(itemListPage, "Items");

	// PAGE 2: Brush List
	wxPanel* brushListPage = new wxPanel(m_notebook);
	wxBoxSizer* brushListSizer = new wxBoxSizer(wxVERTICAL);
	wxSplitterWindow* brushSplitter = new wxSplitterWindow(brushListPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);

	// Top: Brushes
	wxPanel* brushesPanel = new wxPanel(brushSplitter);
	wxBoxSizer* brushesSizer = new wxBoxSizer(wxVERTICAL);

	m_brushGrid = new ItemGridPanel(brushesPanel, this);

	TilesetGridToolbar::Actions brushActions;
	brushActions.filter = [this](const std::string&) {
		PopulateBrushGrid();
	};
	brushActions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		m_brushGrid->SetSort(key, dir);
		PopulateBrushGrid();
	};
	brushActions.clear_sort = [this]() {
		m_brushGrid->ClearSort();
		PopulateBrushGrid();
	};
	brushActions.show_labels = [this](bool show) {
		m_brushGrid->SetShowLabels(show);
	};
	brushActions.icon_size = [this](int size) {
		m_brushGrid->SetIconSize(size);
	};

	m_brushesToolbar = new TilesetGridToolbar(brushesPanel, std::move(brushActions));
	m_brushesToolbar->SetState(m_brushGrid->GetShowLabels(), m_brushGrid->GetIconSize(), false, TilesetSortKey::Name, TilesetSortDirection::Ascending, false);
	brushesSizer->Add(m_brushesToolbar, 0, wxEXPAND);
	brushesSizer->Add(m_brushGrid, 1, wxEXPAND);
	brushesPanel->SetSizer(brushesSizer);

	// Bottom: Related Items
	wxPanel* relatedPanel = new wxPanel(brushSplitter);
	wxBoxSizer* relatedSizer = new wxBoxSizer(wxVERTICAL);
	m_relatedGrid = new ItemGridPanel(relatedPanel, this);
	m_relatedGrid->SetDraggable(true);
	relatedSizer->Add(new wxStaticText(relatedPanel, wxID_ANY, "Related Items"), 0, wxALL, 2);
	relatedSizer->Add(m_relatedGrid, 1, wxEXPAND);
	relatedPanel->SetSizer(relatedSizer);

	brushSplitter->SplitHorizontally(brushesPanel, relatedPanel);
	brushSplitter->SetSashGravity(0.5);
	brushListSizer->Add(brushSplitter, 1, wxEXPAND | wxALL, padding);

	brushListPage->SetSizer(brushListSizer);
	m_notebook->AddPage(brushListPage, "Brushes");

	mainSizer->Add(m_notebook, 1, wxEXPAND);
	SetSizer(mainSizer);
}

void LibraryPanel::OnItemSelected(ItemGridPanel* source, uint16_t itemId) {
	if (itemId == 0) {
		return;
	}

	if (source == m_brushGrid) {
		PopulateRelatedItems(itemId);
	} else if (m_listener) {
		m_listener->OnLibraryItemSelected(itemId);
	}
}
uint16_t LibraryPanel::GetSidFromCid(uint16_t cid) {
	if (m_cidToSidCache.empty()) {
		const auto all_ids = g_item_definitions.allIds();
		m_cidToSidCache.reserve(all_ids.size());
		for (const ServerItemId id : all_ids) {
			const auto it = g_item_definitions.get(id);
			if (it && it.clientId() != 0) {
				m_cidToSidCache.emplace(it.clientId(), it.serverId());
			}
		}
	}
	auto it = m_cidToSidCache.find(cid);
	return (it != m_cidToSidCache.end()) ? it->second : 0;
}

void LibraryPanel::PopulateBrushGrid() {
	std::vector<uint16_t> brushIds;
	brushIds.reserve(g_brushes.getMap().size());
	std::map<uint16_t, wxString> overrides;
	m_brushLookup.clear();

	wxString query = m_brushesToolbar ? wxstr(m_brushesToolbar->GetFilterQuery()).Lower() : wxString();

	for (const auto& pair : g_brushes.getMap()) {
		Brush* brush = pair.second.get();
		if (!brush || brush->is<EraserBrush>() || brush->is<RAWBrush>()) {
			continue;
		}

		std::string name = brush->getName();
		if (name.empty()) {
			continue;
		}

		if (!query.IsEmpty() && !wxString(name).Lower().Contains(query)) {
			continue;
		}

		int rawLookId = brush->getLookID();
		if (rawLookId < 0 || rawLookId > 0xFFFF) {
			continue;
		}
		uint16_t lookId = static_cast<uint16_t>(rawLookId);
		uint16_t serverId = GetSidFromCid(lookId);

		if (serverId != 0) {
			if (auto [it, inserted] = m_brushLookup.try_emplace(serverId, brush); inserted) {
				brushIds.push_back(serverId);
				overrides[serverId] = name;
			}
		}
	}
	m_brushGrid->SetItems(brushIds, std::move(overrides));
}

void LibraryPanel::PopulateRelatedItems(uint16_t brushLookId) {
	std::vector<uint16_t> related;
	auto it = m_brushLookup.find(brushLookId);
	if (it == m_brushLookup.end()) {
		m_relatedGrid->SetItems({});
		return;
	}

	Brush* brush = it->second;
	if (!brush) {
		return;
	}

	try {
		brush->getRelatedItems(related);
	} catch (const std::exception& e) {
		wxLogError("Error getting related items for brush %d: %s", brushLookId, e.what());
	} catch (...) {
		wxLogError("Unknown error getting related items for brush %d", brushLookId);
	}

	if (brushLookId != 0) {
		related.push_back(brushLookId);
	}

	std::sort(related.begin(), related.end());
	related.erase(std::unique(related.begin(), related.end()), related.end());

	m_relatedGrid->SetItems(related);
}
