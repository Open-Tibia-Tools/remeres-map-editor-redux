#include "app/main.h"
#include <wx/log.h>
#include "ui/replace_tool/library_panel.h"
#include "ui/replace_tool/visual_similarity_service.h"
#include "ui/replace_tool/map_item_collector.h"
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
#include "editor/editor.h"
#include "editor/selection.h"
#include "map/map.h"
#include "ui/gui.h"
#include "ui/map_tab.h"
#include "rendering/ui/map_display.h"
#include "rendering/core/floor_visibility_mode.h"
#include "app/settings.h"
#include "util/image_manager.h"
#include <wx/splitter.h>
#include <algorithm>
#include <set>

namespace {

wxButton* MakeIconButton(wxWindow* parent, std::string_view icon, const wxString& tooltip, const wxSize& size) {
	auto* button = new wxButton(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, size, wxBU_NOTEXT | wxBU_EXACTFIT);
	button->SetBitmap(IMAGE_MANAGER.GetBitmap(icon, wxWindow::FromDIP(wxSize(16, 16), parent), Theme::Get(Theme::Role::Text)));
	button->SetToolTip(tooltip);
	return button;
}

} // namespace

LibraryPanel::LibraryPanel(wxWindow* parent, Listener* listener, Editor* editor) :
	wxPanel(parent, wxID_ANY),
	m_listener(listener),
	m_editor(editor) {

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

	if (m_editor) {
		m_editor->selection.addListener(reinterpret_cast<uintptr_t>(this), [this](size_t count) {
			OnMapSelectionChanged(count);
		});
	}
}

void LibraryPanel::DetachEditor() {
	if (m_editor) {
		m_editor->selection.removeListener(reinterpret_cast<uintptr_t>(this));
		m_editor = nullptr;
	}
}

LibraryPanel::~LibraryPanel() {
	DetachEditor();
}

void LibraryPanel::InitLayout() {
	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
	int padding = Theme::Grid(2);

	m_notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxBORDER_NONE);

	// PAGE 1: Item List
	wxPanel* itemListPage = new wxPanel(m_notebook);
	wxBoxSizer* itemListSizer = new wxBoxSizer(wxVERTICAL);

	m_allItemsGrid = new ItemGridPanel(itemListPage, this);
	m_allItemsGrid->SetDraggable(true);
	m_allItemsGrid->SetSort(TilesetSortKey::ID, TilesetSortDirection::Ascending);

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
	m_itemsToolbar->SetState(m_allItemsGrid->GetShowLabels(), m_allItemsGrid->GetIconSize(), m_allItemsGrid->HasSort(), m_allItemsGrid->GetSortKey(), m_allItemsGrid->GetSortDirection(), false);
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
		// The related items are sorted with their brush, so the selection has to
		// be found again in the new order.
		const uint16_t selected = m_brushGrid->GetSelectedItemId();
		m_brushGrid->SetSort(key, dir);
		m_relatedGrid->SetSort(key, dir);
		PopulateBrushGrid();
		RestoreBrushSelection(selected);
	};
	brushActions.clear_sort = [this]() {
		const uint16_t selected = m_brushGrid->GetSelectedItemId();
		m_brushGrid->ClearSort();
		m_relatedGrid->ClearSort();
		PopulateBrushGrid();
		RestoreBrushSelection(selected);
	};
	brushActions.show_labels = [this](bool show) {
		m_brushGrid->SetShowLabels(show);
		m_relatedGrid->SetShowLabels(show);
	};
	brushActions.icon_size = [this](int size) {
		m_brushGrid->SetIconSize(size);
		m_relatedGrid->SetIconSize(size);
	};

	m_brushesToolbar = new TilesetGridToolbar(brushesPanel, std::move(brushActions));
	m_brushesToolbar->SetState(m_brushGrid->GetShowLabels(), m_brushGrid->GetIconSize(), m_brushGrid->HasSort(), m_brushGrid->GetSortKey(), m_brushGrid->GetSortDirection(), false);
	brushesSizer->Add(m_brushesToolbar, 0, wxEXPAND);
	brushesSizer->Add(m_brushGrid, 1, wxEXPAND);
	brushesPanel->SetSizer(brushesSizer);

	// Bottom: Related Items
	wxPanel* relatedPanel = new wxPanel(brushSplitter);
	wxBoxSizer* relatedSizer = new wxBoxSizer(wxVERTICAL);
	m_relatedGrid = new ItemGridPanel(relatedPanel, this);
	m_relatedGrid->SetDraggable(true);

	const wxSize iconBtnSize = FromDIP(wxSize(30, 26));

	wxBoxSizer* relatedHeaderSizer = new wxBoxSizer(wxHORIZONTAL);
	auto* relatedLabel = new wxStaticText(relatedPanel, wxID_ANY, "Related Items");
	relatedHeaderSizer->Add(relatedLabel, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);

	m_fillUnmatchedBtn = MakeIconButton(relatedPanel, ICON_WAND_MAGIC_SPARKLES, "Fill unmatched rules with items from this brush", iconBtnSize);
	m_fillUnmatchedBtn->Enable(false);
	m_fillUnmatchedBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		if (m_currentBrush && m_listener) {
			m_listener->OnAutoMatchRules(m_currentBrush);
		}
	});
	relatedHeaderSizer->Add(m_fillUnmatchedBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);

	relatedSizer->Add(relatedHeaderSizer, 0, wxEXPAND | wxBOTTOM, 2);
	relatedSizer->Add(m_relatedGrid, 1, wxEXPAND);
	relatedPanel->SetSizer(relatedSizer);

	brushSplitter->SplitHorizontally(brushesPanel, relatedPanel);
	brushSplitter->SetSashGravity(0.5);
	brushListSizer->Add(brushSplitter, 1, wxEXPAND | wxALL, padding);

	brushListPage->SetSizer(brushListSizer);
	m_notebook->AddPage(brushListPage, "Brushes");

	// PAGE 3: Viewport
	wxPanel* viewportPage = new wxPanel(m_notebook);
	wxBoxSizer* viewportSizer = new wxBoxSizer(wxVERTICAL);

	wxBoxSizer* viewportHeaderSizer = new wxBoxSizer(wxHORIZONTAL);
	m_refreshViewportBtn = MakeIconButton(viewportPage, ICON_ARROWS_ROTATE, "Refresh viewport items", iconBtnSize);
	m_refreshViewportBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		RefreshViewportItems();
	});
	viewportHeaderSizer->Add(m_refreshViewportBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Theme::Grid(1));

	m_addViewportRulesBtn = MakeIconButton(viewportPage, ICON_PLUS_SQUARE, "Add every item visible in the viewport as a rule", iconBtnSize);
	m_addViewportRulesBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		if (!m_viewportInitialized) {
			RefreshViewportItems();
		}
		if (m_listener && m_viewportGrid) {
			m_listener->OnAddItemsAsRules(m_viewportGrid->GetItems());
		}
	});
	viewportHeaderSizer->Add(m_addViewportRulesBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Theme::Grid(1));

	m_viewportGrid = new ItemGridPanel(viewportPage, this);
	m_viewportGrid->SetDraggable(true);

	TilesetGridToolbar::Actions viewportActions;
	viewportActions.filter = [this](const std::string& query) {
		m_viewportGrid->SetFilter(wxstr(query));
	};
	viewportActions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		m_viewportGrid->SetSort(key, dir);
		m_viewportGrid->SetFilter(wxstr(m_viewportToolbar->GetFilterQuery()));
	};
	viewportActions.clear_sort = [this]() {
		m_viewportGrid->ClearSort();
		m_viewportGrid->SetFilter(wxstr(m_viewportToolbar->GetFilterQuery()));
	};
	viewportActions.show_labels = [this](bool show) {
		m_viewportGrid->SetShowLabels(show);
	};
	viewportActions.icon_size = [this](int size) {
		m_viewportGrid->SetIconSize(size);
	};

	m_viewportToolbar = new TilesetGridToolbar(viewportPage, std::move(viewportActions));
	m_viewportToolbar->SetState(m_viewportGrid->GetShowLabels(), m_viewportGrid->GetIconSize(), m_viewportGrid->HasSort(), m_viewportGrid->GetSortKey(), m_viewportGrid->GetSortDirection(), false);
	viewportHeaderSizer->Add(m_viewportToolbar, 1, wxEXPAND);

	viewportSizer->Add(viewportHeaderSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, padding);
	viewportSizer->Add(m_viewportGrid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, padding);
	viewportPage->SetSizer(viewportSizer);
	m_notebook->AddPage(viewportPage, "Viewport");

	// PAGE 4: Selection
	wxPanel* selectionPage = new wxPanel(m_notebook);
	wxBoxSizer* selectionSizer = new wxBoxSizer(wxVERTICAL);

	wxBoxSizer* selectionHeaderSizer = new wxBoxSizer(wxHORIZONTAL);
	m_addSelectionRulesBtn = MakeIconButton(selectionPage, ICON_PLUS_SQUARE, "Add every selected item as a rule", iconBtnSize);
	m_addSelectionRulesBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		RefreshSelectionItems();
		if (m_listener && m_selectionGrid) {
			m_listener->OnAddItemsAsRules(m_selectionGrid->GetItems());
		}
	});
	selectionHeaderSizer->Add(m_addSelectionRulesBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Theme::Grid(1));

	m_selectionGrid = new ItemGridPanel(selectionPage, this);
	m_selectionGrid->SetDraggable(true);

	TilesetGridToolbar::Actions selectionActions;
	selectionActions.filter = [this](const std::string& query) {
		m_selectionGrid->SetFilter(wxstr(query));
	};
	selectionActions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		m_selectionGrid->SetSort(key, dir);
		m_selectionGrid->SetFilter(wxstr(m_selectionToolbar->GetFilterQuery()));
	};
	selectionActions.clear_sort = [this]() {
		m_selectionGrid->ClearSort();
		m_selectionGrid->SetFilter(wxstr(m_selectionToolbar->GetFilterQuery()));
	};
	selectionActions.show_labels = [this](bool show) {
		m_selectionGrid->SetShowLabels(show);
	};
	selectionActions.icon_size = [this](int size) {
		m_selectionGrid->SetIconSize(size);
	};

	m_selectionToolbar = new TilesetGridToolbar(selectionPage, std::move(selectionActions));
	m_selectionToolbar->SetState(m_selectionGrid->GetShowLabels(), m_selectionGrid->GetIconSize(), m_selectionGrid->HasSort(), m_selectionGrid->GetSortKey(), m_selectionGrid->GetSortDirection(), false);
	selectionHeaderSizer->Add(m_selectionToolbar, 1, wxEXPAND);

	selectionSizer->Add(selectionHeaderSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, padding);
	selectionSizer->Add(m_selectionGrid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, padding);
	selectionPage->SetSizer(selectionSizer);
	m_notebook->AddPage(selectionPage, "Selection");

	m_notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &LibraryPanel::OnNotebookPageChanged, this);

	mainSizer->Add(m_notebook, 1, wxEXPAND);
	SetSizer(mainSizer);
}

void LibraryPanel::OnNotebookPageChanged(wxNotebookEvent& event) {
	const int page = event.GetSelection();
	if (page == 2) {
		if (!m_viewportInitialized) {
			RefreshViewportItems();
		}
	} else if (page == 3) {
		RefreshSelectionItems();
	}
	event.Skip();
}

void LibraryPanel::OnMapSelectionChanged(size_t /*count*/) {
	if (m_notebook && m_notebook->GetSelection() == 3) {
		RefreshSelectionItems();
	}
}

void LibraryPanel::RefreshViewportItems() {
	if (!m_editor || !m_viewportGrid) {
		return;
	}

	MapTab* activeTab = g_gui.GetCurrentMapTab();
	const bool isSameEditor = (activeTab && activeTab->GetEditor() == m_editor);
	m_viewportGrid->SetItems(isSameEditor ? rme::replace_tool::MapItemCollector::CollectViewportItemIds(m_editor, activeTab->GetCanvas()) : std::vector<uint16_t>{});
	m_viewportInitialized = true;
	if (m_viewportToolbar) {
		m_viewportGrid->SetFilter(wxstr(m_viewportToolbar->GetFilterQuery()));
	}
}

void LibraryPanel::RefreshSelectionItems() {
	if (!m_editor || !m_selectionGrid) {
		return;
	}

	m_selectionGrid->SetItems(rme::replace_tool::MapItemCollector::CollectSelectionItemIds(m_editor));
	if (m_selectionToolbar) {
		m_selectionGrid->SetFilter(wxstr(m_selectionToolbar->GetFilterQuery()));
	}
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
		m_currentBrush = nullptr;
		if (m_fillUnmatchedBtn) {
			m_fillUnmatchedBtn->Enable(false);
		}
		m_relatedGrid->SetItems({});
		return;
	}

	Brush* brush = it->second;
	m_currentBrush = brush;
	if (m_fillUnmatchedBtn) {
		m_fillUnmatchedBtn->Enable(m_currentBrush != nullptr);
	}
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

void LibraryPanel::RestoreBrushSelection(uint16_t brushLookId) {
	if (brushLookId == 0) {
		return;
	}

	for (size_t i = 0; i < m_brushGrid->GetItemCount(); ++i) {
		if (m_brushGrid->GetItem(i) == brushLookId) {
			m_brushGrid->SetSelection(static_cast<int>(i));
			break;
		}
	}

	// SetSelection only reports a change, so the related pane is refreshed here.
	PopulateRelatedItems(brushLookId);
}
