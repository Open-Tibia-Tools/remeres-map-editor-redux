#include "app/settings.h"
#include "app/main.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "palette/controls/palette_search_ctrl.h"

#include "ui/theme.h"
#include "ui/replace_tool/visual_similarity_service.h"
#include "util/image_manager.h"

#include <wx/aui/auibar.h>
#include <wx/menu.h>
#include <wx/srchctrl.h>

#include <algorithm>
#include <utility>

namespace {
	enum MenuID {
		MENU_SORT_BY_ID = wxID_HIGHEST + 7011,
		MENU_SORT_BY_NAME,
		MENU_SORT_DEFAULT,
		MENU_SIZE_32,
		MENU_SIZE_64,
		MENU_SIZE_128,
		MENU_SIZE_LIST,
		MENU_INDICATOR_FRAME,
		MENU_INDICATOR_DOGEAR,
		MENU_SPACING_2,
		MENU_SPACING_4,
		MENU_SPACING_6,
	};
} // namespace

TilesetGridToolbar::TilesetGridToolbar(wxWindow* parent, Actions actions, unsigned parts) :
	wxPanel(parent, wxID_ANY),
	m_actions(std::move(actions)),
	m_parts(parts) {
	m_indicatorDogEar = g_settings.getBoolean(Config::PALETTE_INDICATOR_DOGEAR);
	const int loadedSpacing = g_settings.getInteger(Config::PALETTE_GRID_SPACING);
	m_gridSpacing = (loadedSpacing == 2 || loadedSpacing == 6) ? loadedSpacing : 4;

	const wxSize iconSize = FromDIP(wxSize(16, 16));
	const long toolbarStyle = (wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_PLAIN_BACKGROUND) & ~wxAUI_TB_GRIPPER;
	const wxColour iconColor = Theme::Get(Theme::Role::Text);

	m_toolbar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, toolbarStyle);
	m_toolbar->SetToolBitmapSize(iconSize);
	m_toolbar->SetMargins(1, 1, 1, 1);
	m_toolbar->SetToolBorderPadding(2);
	if (parts & Part_Sort) {
		m_toolbar->AddTool(TOOL_SORT_AZ, wxEmptyString, IMAGE_MANAGER.GetBitmapBundle(ICON_SORT_ALPHA_DOWN, iconColor), "Sort ascending (A-Z)");
		m_toolbar->AddTool(TOOL_SORT_ZA, wxEmptyString, IMAGE_MANAGER.GetBitmapBundle(ICON_SORT_ALPHA_UP, iconColor), "Sort descending (Z-A)");
	}
	if (parts & Part_Labels) {
		m_toolbar->AddTool(TOOL_TOGGLE_LABELS, wxEmptyString, IMAGE_MANAGER.GetBitmapBundle(ICON_TAG, iconColor), "Toggle labels", wxITEM_CHECK);
	}
	if (parts & Part_Size) {
		m_toolbar->AddTool(TOOL_CHANGE_SIZE, wxEmptyString, IMAGE_MANAGER.GetBitmapBundle(ICON_MAXIMIZE, iconColor), "Tile size");
	}
	if (parts & Part_Indicators) {
		m_toolbar->AddTool(TOOL_BRUSH_INDICATORS, wxEmptyString, IMAGE_MANAGER.GetBitmapBundle(ICON_PAINTBRUSH, iconColor), "Brush indicator", wxITEM_CHECK);
	}
	if (parts & Part_Spacing) {
		m_toolbar->AddTool(TOOL_GRID_SPACING, wxEmptyString, IMAGE_MANAGER.GetBitmapBundle(ICON_TABLE_CELLS, iconColor), "Grid spacing");
	}
	if (parts & Part_FilterAll) {
		m_toolbar->AddTool(TOOL_FILTER_ALL, wxEmptyString, IMAGE_MANAGER.GetBitmapBundle(ICON_FILTER, iconColor), "Filter all tilesets and palettes", wxITEM_CHECK);
	}
	m_toolbar->ToggleTool(TOOL_TOGGLE_LABELS, m_showLabels && (parts & Part_Labels));
	if (parts & Part_Indicators) {
		m_toolbar->ToggleTool(TOOL_BRUSH_INDICATORS, m_indicatorDogEar);
	}
	m_toolbar->Realize();
	m_toolbar->Bind(wxEVT_TOOL, &TilesetGridToolbar::OnToolClick, this);

	m_toolbarRow = new wxBoxSizer(wxHORIZONTAL);
	m_toolbarRow->Add(m_toolbar, wxSizerFlags().CenterVertical());

	m_searchRow = new wxBoxSizer(wxHORIZONTAL);
	if (parts & Part_Search) {
		m_searchCtrl = new PaletteSearchCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
		m_searchCtrl->SetDescriptiveText("Search...");
		m_searchCtrl->ShowCancelButton(true);
		m_searchCtrl->SetMinSize(FromDIP(wxSize(60, -1)));
		m_searchCtrl->Bind(wxEVT_TEXT, &TilesetGridToolbar::OnSearchChanged, this);
		m_searchCtrl->Bind(wxEVT_SEARCHCTRL_CANCEL_BTN, &TilesetGridToolbar::OnSearchCancel, this);
		m_searchCtrl->Bind(wxEVT_SEARCHCTRL_SEARCH_BTN, &TilesetGridToolbar::OnSearchChanged, this);
		m_searchCtrl->Bind(wxEVT_TEXT_ENTER, &TilesetGridToolbar::OnSearchChanged, this);
		m_searchCtrl->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
			if (event.GetKeyCode() == WXK_ESCAPE && m_searchCtrl && !m_searchCtrl->GetValue().empty()) {
				OnSearchCancelIfNeeded();
				return;
			}
			event.Skip();
		});
		m_searchCtrl->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
			if (m_actions.filter_focus_lost) {
				m_actions.filter_focus_lost();
			}
			event.Skip();
		});
		m_searchRow->Add(m_searchCtrl, wxSizerFlags(1).CenterVertical());
	}

	SetStacked(false);
	ApplyTheme();
}

void TilesetGridToolbar::SetStacked(bool stacked) {
	if (!m_toolbarRow || !m_searchRow) {
		return;
	}

	if (!m_rootSizer) {
		m_rootSizer = new wxBoxSizer(stacked ? wxVERTICAL : wxHORIZONTAL);
		SetSizer(m_rootSizer, false);
	}

	// Only the outer sizer belongs to this window; the rows are detached and
	// re-added so a layout change never deletes them (a second SetSizer with
	// ownership would leave these pointers dangling).
	m_rootSizer->Detach(m_toolbarRow);
	m_rootSizer->Detach(m_searchRow);
	m_rootSizer->SetOrientation(stacked ? wxVERTICAL : wxHORIZONTAL);

	if (stacked) {
		m_rootSizer->Add(m_toolbarRow, wxSizerFlags().Expand());
		if (m_searchCtrl) {
			m_rootSizer->Add(m_searchRow, wxSizerFlags().Expand().Border(wxTOP, FromDIP(2)));
		}
	} else {
		m_rootSizer->Add(m_toolbarRow, wxSizerFlags().CenterVertical());
		if (m_searchCtrl) {
			m_rootSizer->Add(m_searchRow, wxSizerFlags(1).CenterVertical().Border(wxLEFT, FromDIP(4)));
		}
	}

	Layout();
}

void TilesetGridToolbar::SetState(
	bool show_labels,
	int icon_size,
	bool has_sort,
	TilesetSortKey sort_key,
	TilesetSortDirection sort_dir,
	bool filter_all,
	bool is_list_mode
) {
	m_showLabels = show_labels;
	m_iconSize = std::clamp(icon_size, 16, 128);
	m_hasSort = has_sort;
	m_sortKey = sort_key;
	m_sortDir = sort_dir;
	m_filterAll = filter_all;
	m_isListMode = is_list_mode;

	if (m_toolbar) {
		if (m_toolbar->GetToolIndex(TOOL_TOGGLE_LABELS) != wxNOT_FOUND) {
			m_toolbar->ToggleTool(TOOL_TOGGLE_LABELS, m_showLabels);
		}
		if (m_toolbar->GetToolIndex(TOOL_FILTER_ALL) != wxNOT_FOUND) {
			m_toolbar->ToggleTool(TOOL_FILTER_ALL, m_filterAll);
		}
		if (m_toolbar->GetToolIndex(TOOL_BRUSH_INDICATORS) != wxNOT_FOUND) {
			m_toolbar->ToggleTool(TOOL_BRUSH_INDICATORS, m_indicatorDogEar);
		}
	}
}

void TilesetGridToolbar::SetFilterQuery(const std::string& query) {
	if (m_searchCtrl) {
		m_searchCtrl->ChangeValue(wxstr(query));
	}
}

std::string TilesetGridToolbar::GetFilterQuery() const {
	return m_searchCtrl ? m_searchCtrl->GetValue().ToStdString() : std::string();
}

void TilesetGridToolbar::FocusSearch() {
	if (m_searchCtrl) {
		m_searchCtrl->SetFocus();
	}
}

void TilesetGridToolbar::ApplyTheme() {
	const wxSize iconSize = FromDIP(wxSize(16, 16));
	const wxColour iconColor = Theme::Get(Theme::Role::Text);
	const wxColour surface = Theme::Get(Theme::Role::Surface);
	const wxColour text = Theme::Get(Theme::Role::Text);

	const auto hasTool = [this](int toolId) {
		return m_toolbar && m_toolbar->GetToolIndex(toolId) != wxNOT_FOUND;
	};

	if (m_toolbar) {
		if (hasTool(TOOL_SORT_AZ)) {
			m_toolbar->SetToolBitmap(TOOL_SORT_AZ, IMAGE_MANAGER.GetBitmapBundle(ICON_SORT_ALPHA_DOWN, iconColor));
		}
		if (hasTool(TOOL_SORT_ZA)) {
			m_toolbar->SetToolBitmap(TOOL_SORT_ZA, IMAGE_MANAGER.GetBitmapBundle(ICON_SORT_ALPHA_UP, iconColor));
		}
		if (hasTool(TOOL_TOGGLE_LABELS)) {
			m_toolbar->SetToolBitmap(TOOL_TOGGLE_LABELS, IMAGE_MANAGER.GetBitmapBundle(ICON_TAG, iconColor));
		}
		if (hasTool(TOOL_CHANGE_SIZE)) {
			m_toolbar->SetToolBitmap(TOOL_CHANGE_SIZE, IMAGE_MANAGER.GetBitmapBundle(ICON_MAXIMIZE, iconColor));
		}
		if (hasTool(TOOL_FILTER_ALL)) {
			m_toolbar->SetToolBitmap(TOOL_FILTER_ALL, IMAGE_MANAGER.GetBitmapBundle(ICON_FILTER, iconColor));
		}
		if (hasTool(TOOL_BRUSH_INDICATORS)) {
			m_toolbar->SetToolBitmap(TOOL_BRUSH_INDICATORS, IMAGE_MANAGER.GetBitmapBundle(ICON_PAINTBRUSH, iconColor));
		}
		if (hasTool(TOOL_GRID_SPACING)) {
			m_toolbar->SetToolBitmap(TOOL_GRID_SPACING, IMAGE_MANAGER.GetBitmapBundle(ICON_TABLE_CELLS, iconColor));
		}
		m_toolbar->SetBackgroundColour(surface);
		m_toolbar->SetForegroundColour(text);
		m_toolbar->Refresh();
	}

	if (m_searchCtrl) {
		ApplySearchTheme();
	}

	SetBackgroundColour(surface);
}

void TilesetGridToolbar::ApplySearchTheme() {
	const wxColour surface = Theme::Get(Theme::Role::Surface);
	const wxColour text = Theme::Get(Theme::Role::Text);

	m_searchCtrl->SetBackgroundColour(surface);
	m_searchCtrl->SetForegroundColour(text);
	for (wxWindow* child : m_searchCtrl->GetChildren()) {
		if (child) {
			child->SetBackgroundColour(surface);
			child->SetForegroundColour(text);
			child->Refresh();
		}
	}
	m_searchCtrl->Refresh();
}

void TilesetGridToolbar::OnToolClick(wxCommandEvent& event) {
	const int id = event.GetId();
	if (id == TOOL_SORT_AZ) {
		OnSortButtonClick(TilesetSortDirection::Ascending, id);
	} else if (id == TOOL_SORT_ZA) {
		OnSortButtonClick(TilesetSortDirection::Descending, id);
	} else if (id == TOOL_TOGGLE_LABELS) {
		m_showLabels = m_toolbar ? m_toolbar->GetToolToggled(TOOL_TOGGLE_LABELS) : event.IsChecked();
		if (m_actions.show_labels) {
			m_actions.show_labels(m_showLabels);
		}
	} else if (id == TOOL_CHANGE_SIZE) {
		OnSizeButtonClick(id);
	} else if (id == TOOL_BRUSH_INDICATORS) {
		m_indicatorDogEar = m_toolbar ? m_toolbar->GetToolToggled(TOOL_BRUSH_INDICATORS) : !m_indicatorDogEar;
		if (m_actions.brush_indicators) {
			m_actions.brush_indicators(m_indicatorDogEar);
		}
	} else if (id == TOOL_GRID_SPACING) {
		OnSpacingButtonClick(id);
	} else if (id == TOOL_FILTER_ALL) {
		m_filterAll = m_toolbar ? m_toolbar->GetToolToggled(TOOL_FILTER_ALL) : event.IsChecked();
		if (m_actions.filter_all) {
			m_actions.filter_all(m_filterAll);
		}
	} else {
		event.Skip();
	}
}

void TilesetGridToolbar::OnSortButtonClick(TilesetSortDirection dir, int toolId) {
	wxMenu menu;
	auto* itemID = menu.AppendCheckItem(MENU_SORT_BY_ID, "By ID");
	auto* itemName = menu.AppendCheckItem(MENU_SORT_BY_NAME, "By Name");
	auto* itemDefault = menu.AppendCheckItem(MENU_SORT_DEFAULT, "Default");
	if (!m_hasSort) {
		itemDefault->Check(true);
	} else if (m_sortKey == TilesetSortKey::ID) {
		itemID->Check(true);
	} else {
		itemName->Check(true);
	}

	wxPoint pos;
	if (m_toolbar) {
		const wxRect rect = m_toolbar->GetToolRect(toolId);
		pos = wxPoint(rect.x, rect.y + rect.height);
	}

	const int selected = m_toolbar ? m_toolbar->GetPopupMenuSelectionFromUser(menu, pos) : wxID_NONE;
	if (selected == MENU_SORT_BY_ID) {
		ApplySort(TilesetSortKey::ID, dir);
	} else if (selected == MENU_SORT_BY_NAME) {
		ApplySort(TilesetSortKey::Name, dir);
	} else if (selected == MENU_SORT_DEFAULT) {
		m_hasSort = false;
		if (m_actions.clear_sort) {
			m_actions.clear_sort();
		}
	}
}

void TilesetGridToolbar::ApplySort(TilesetSortKey key, TilesetSortDirection dir) {
	m_sortKey = key;
	m_sortDir = dir;
	m_hasSort = true;
	if (m_actions.sort) {
		m_actions.sort(key, dir);
	}
}

void TilesetGridToolbar::OnSizeButtonClick(int toolId) {
	wxMenu menu;
	auto* item32 = menu.AppendCheckItem(MENU_SIZE_32, "32x32");
	auto* item64 = menu.AppendCheckItem(MENU_SIZE_64, "64x64");
	auto* item128 = menu.AppendCheckItem(MENU_SIZE_128, "128x128");
	wxMenuItem* itemList = nullptr;
	if (m_actions.list_mode) {
		itemList = menu.AppendCheckItem(MENU_SIZE_LIST, "List");
	}

	if (m_isListMode && itemList) {
		itemList->Check(true);
	} else if (m_iconSize == 64) {
		item64->Check(true);
	} else if (m_iconSize == 128) {
		item128->Check(true);
	} else {
		item32->Check(true);
	}

	wxPoint pos;
	if (m_toolbar) {
		const wxRect rect = m_toolbar->GetToolRect(toolId);
		pos = wxPoint(rect.x, rect.y + rect.height);
	}

	const int selected = m_toolbar ? m_toolbar->GetPopupMenuSelectionFromUser(menu, pos) : wxID_NONE;
	if (selected == MENU_SIZE_32 || selected == MENU_SIZE_64 || selected == MENU_SIZE_128) {
		if (m_isListMode) {
			m_isListMode = false;
			if (m_actions.list_mode) {
				m_actions.list_mode(false);
			}
		}
		const int newSize = (selected == MENU_SIZE_128) ? 128 : ((selected == MENU_SIZE_64) ? 64 : 32);
		m_iconSize = newSize;
		if (m_actions.icon_size) {
			m_actions.icon_size(newSize);
		}
	} else if (selected == MENU_SIZE_LIST) {
		m_isListMode = true;
		if (m_actions.list_mode) {
			m_actions.list_mode(true);
		}
	}
}

void TilesetGridToolbar::SetListMode(bool listMode) {
	m_isListMode = listMode;
}

void TilesetGridToolbar::SetIconSize(int sizePx) {
	m_iconSize = sizePx;
}


void TilesetGridToolbar::OnSearchChanged(wxCommandEvent& event) {
	if (!m_searchCtrl) {
		return;
	}

	if (m_actions.filter) {
		m_actions.filter(m_searchCtrl->GetValue().ToStdString());
	}

	const wxEventType eventType = event.GetEventType();
	if ((eventType == wxEVT_TEXT_ENTER || eventType == wxEVT_SEARCHCTRL_SEARCH_BTN) && m_actions.filter_committed) {
		m_actions.filter_committed();
	}
}

void TilesetGridToolbar::OnSearchCancel(wxCommandEvent&) {
	OnSearchCancelIfNeeded();
}

void TilesetGridToolbar::OnSearchCancelIfNeeded() {
	if (m_searchCtrl) {
		m_searchCtrl->ChangeValue(wxEmptyString);
	}
	const bool hadFilterAll = m_filterAll;
	m_filterAll = false;
	if (m_toolbar && m_toolbar->GetToolIndex(TOOL_FILTER_ALL) != wxNOT_FOUND) {
		m_toolbar->ToggleTool(TOOL_FILTER_ALL, false);
	}
	if (hadFilterAll && m_actions.filter_all) {
		m_actions.filter_all(false);
	}
	if (m_actions.filter) {
		m_actions.filter(std::string());
	}
	if (m_actions.filter_cleared) {
		m_actions.filter_cleared();
	}
}



void TilesetGridToolbar::OnSpacingButtonClick(int toolId) {
	wxMenu menu;
	auto* item2 = menu.AppendCheckItem(MENU_SPACING_2, "Compact (2 px)");
	auto* item4 = menu.AppendCheckItem(MENU_SPACING_4, "Normal (4 px)");
	auto* item6 = menu.AppendCheckItem(MENU_SPACING_6, "Relaxed (6 px)");

	if (m_gridSpacing == 2) {
		item2->Check(true);
	} else if (m_gridSpacing == 6) {
		item6->Check(true);
	} else {
		item4->Check(true);
	}

	wxPoint pos;
	if (m_toolbar) {
		const wxRect rect = m_toolbar->GetToolRect(toolId);
		pos = wxPoint(rect.x, rect.y + rect.height);
	}

	const int selected = m_toolbar ? m_toolbar->GetPopupMenuSelectionFromUser(menu, pos) : wxID_NONE;
	int newSpacing = m_gridSpacing;
	if (selected == MENU_SPACING_2) {
		newSpacing = 2;
	} else if (selected == MENU_SPACING_4) {
		newSpacing = 4;
	} else if (selected == MENU_SPACING_6) {
		newSpacing = 6;
	} else {
		return;
	}

	m_gridSpacing = newSpacing;
	if (m_actions.grid_spacing) {
		m_actions.grid_spacing(m_gridSpacing);
	}
}

void TilesetGridToolbar::SetIndicatorState(bool dogEar) {
	m_indicatorDogEar = dogEar;
	if (m_toolbar && m_toolbar->GetToolIndex(TOOL_BRUSH_INDICATORS) != wxNOT_FOUND) {
		m_toolbar->ToggleTool(TOOL_BRUSH_INDICATORS, m_indicatorDogEar);
	}
}

void TilesetGridToolbar::SetGridSpacing(int spacingPx) {
	m_gridSpacing = (spacingPx == 2 || spacingPx == 6) ? spacingPx : 4;
}
