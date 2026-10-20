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
	};
} // namespace

TilesetGridToolbar::TilesetGridToolbar(wxWindow* parent, Actions actions, unsigned parts) :
	wxPanel(parent, wxID_ANY),
	m_actions(std::move(actions)),
	m_parts(parts) {
	const wxSize iconSize = FromDIP(wxSize(16, 16));
	const long toolbarStyle = (wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_PLAIN_BACKGROUND) & ~wxAUI_TB_GRIPPER;
	const wxColour iconColor = Theme::Get(Theme::Role::Text);

	m_toolbar = newd wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, toolbarStyle);
	m_toolbar->SetToolBitmapSize(iconSize);
	m_toolbar->SetMargins(1, 1, 1, 1);
	m_toolbar->SetToolBorderPadding(2);
	if (parts & Part_Sort) {
		m_toolbar->AddTool(TOOL_SORT_AZ, wxEmptyString, IMAGE_MANAGER.GetBitmap(ICON_SORT_ALPHA_DOWN, iconSize, iconColor), "Sort ascending (A-Z)");
		m_toolbar->AddTool(TOOL_SORT_ZA, wxEmptyString, IMAGE_MANAGER.GetBitmap(ICON_SORT_ALPHA_UP, iconSize, iconColor), "Sort descending (Z-A)");
	}
	if (parts & Part_Labels) {
		m_toolbar->AddTool(TOOL_TOGGLE_LABELS, wxEmptyString, IMAGE_MANAGER.GetBitmap(ICON_TAG, iconSize, iconColor), "Toggle labels", wxITEM_CHECK);
	}
	if (parts & Part_Size) {
		m_toolbar->AddTool(TOOL_CHANGE_SIZE, wxEmptyString, IMAGE_MANAGER.GetBitmap(ICON_MAXIMIZE, iconSize, iconColor), "Tile size");
	}
	if (parts & Part_FilterAll) {
		m_toolbar->AddTool(TOOL_FILTER_ALL, wxEmptyString, IMAGE_MANAGER.GetBitmap(ICON_FILTER, iconSize, iconColor), "Filter all tilesets and palettes", wxITEM_CHECK);
	}
	m_toolbar->ToggleTool(TOOL_TOGGLE_LABELS, m_showLabels && (parts & Part_Labels));
	m_toolbar->Realize();
	m_toolbar->Bind(wxEVT_TOOL, &TilesetGridToolbar::OnToolClick, this);

	m_toolbarRow = newd wxBoxSizer(wxHORIZONTAL);
	m_toolbarRow->Add(m_toolbar, 0, wxALIGN_CENTER_VERTICAL);

	m_searchRow = newd wxBoxSizer(wxHORIZONTAL);
	if (parts & Part_Search) {
		m_searchCtrl = newd PaletteSearchCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
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
		m_searchRow->Add(m_searchCtrl, 1, wxALIGN_CENTER_VERTICAL);
	}

	SetStacked(false);
	ApplyTheme();
}

void TilesetGridToolbar::SetStacked(bool stacked) {
	if (!m_toolbarRow || !m_searchRow) {
		return;
	}

	if (!m_rootSizer) {
		m_rootSizer = newd wxBoxSizer(stacked ? wxVERTICAL : wxHORIZONTAL);
		SetSizer(m_rootSizer, false);
	}

	// Only the outer sizer belongs to this window; the rows are detached and
	// re-added so a layout change never deletes them (a second SetSizer with
	// ownership would leave these pointers dangling).
	m_rootSizer->Detach(m_toolbarRow);
	m_rootSizer->Detach(m_searchRow);
	m_rootSizer->SetOrientation(stacked ? wxVERTICAL : wxHORIZONTAL);

	if (stacked) {
		m_rootSizer->Add(m_toolbarRow, 0, wxEXPAND);
		if (m_searchCtrl) {
			m_rootSizer->Add(m_searchRow, 0, wxEXPAND | wxTOP, FromDIP(2));
		}
	} else {
		m_rootSizer->Add(m_toolbarRow, 0, wxALIGN_CENTER_VERTICAL);
		if (m_searchCtrl) {
			m_rootSizer->Add(m_searchRow, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
		}
	}

	Layout();
}

void TilesetGridToolbar::SetState(bool show_labels, int icon_size, bool has_sort, TilesetSortKey sort_key, TilesetSortDirection sort_dir, bool filter_all) {
	m_showLabels = show_labels;
	m_iconSize = std::clamp(icon_size, 16, 128);
	m_hasSort = has_sort;
	m_sortKey = sort_key;
	m_sortDir = sort_dir;
	m_filterAll = filter_all;

	if (m_toolbar) {
		if (m_toolbar->GetToolIndex(TOOL_TOGGLE_LABELS) != wxNOT_FOUND) {
			m_toolbar->ToggleTool(TOOL_TOGGLE_LABELS, m_showLabels);
		}
		if (m_toolbar->GetToolIndex(TOOL_FILTER_ALL) != wxNOT_FOUND) {
			m_toolbar->ToggleTool(TOOL_FILTER_ALL, m_filterAll);
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
			m_toolbar->SetToolBitmap(TOOL_SORT_AZ, IMAGE_MANAGER.GetBitmap(ICON_SORT_ALPHA_DOWN, iconSize, iconColor));
		}
		if (hasTool(TOOL_SORT_ZA)) {
			m_toolbar->SetToolBitmap(TOOL_SORT_ZA, IMAGE_MANAGER.GetBitmap(ICON_SORT_ALPHA_UP, iconSize, iconColor));
		}
		if (hasTool(TOOL_TOGGLE_LABELS)) {
			m_toolbar->SetToolBitmap(TOOL_TOGGLE_LABELS, IMAGE_MANAGER.GetBitmap(ICON_TAG, iconSize, iconColor));
		}
		if (hasTool(TOOL_CHANGE_SIZE)) {
			m_toolbar->SetToolBitmap(TOOL_CHANGE_SIZE, IMAGE_MANAGER.GetBitmap(ICON_MAXIMIZE, iconSize, iconColor));
		}
		if (hasTool(TOOL_FILTER_ALL)) {
			m_toolbar->SetToolBitmap(TOOL_FILTER_ALL, IMAGE_MANAGER.GetBitmap(ICON_FILTER, iconSize, iconColor));
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
	} else if (id == TOOL_FILTER_ALL) {
		m_filterAll = m_toolbar ? m_toolbar->GetToolToggled(TOOL_FILTER_ALL) : event.IsChecked();
		if (m_actions.filter_all) {
			m_actions.filter_all(m_filterAll);
		}
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

	if (m_iconSize == 64) {
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
	if (selected == MENU_SIZE_32) {
		SetIconSize(32);
	} else if (selected == MENU_SIZE_64) {
		SetIconSize(64);
	} else if (selected == MENU_SIZE_128) {
		SetIconSize(128);
	}
}

void TilesetGridToolbar::SetIconSize(int sizePx) {
	m_iconSize = sizePx;
	if (m_actions.icon_size) {
		m_actions.icon_size(m_iconSize);
	}
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

void TilesetGridToolbar::OnSearchCancel(wxCommandEvent& event) {
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
