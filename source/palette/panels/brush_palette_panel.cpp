#include "palette/panels/brush_palette_panel.h"
#include "ui/gui.h"
#include "game/materials.h"
#include "palette/palette_window.h"
#include "util/image_manager.h"
#include "ui/theme.h"
#include <spdlog/spdlog.h>
#include <wx/menu.h>

TilesetSortKey BrushPalettePanel::s_defaultSortKey = TilesetSortKey::Name;
TilesetSortDirection BrushPalettePanel::s_defaultSortDir = TilesetSortDirection::Ascending;
bool BrushPalettePanel::s_defaultHasSort = false;
bool BrushPalettePanel::s_defaultShowLabels = false;
int BrushPalettePanel::s_defaultTileSize = 32;

// ============================================================================
// Brush Palette Panel
// A common class for terrain/doodad/item/raw palette

BrushPalettePanel::BrushPalettePanel(wxWindow* parent, const DynamicPaletteDefinition& palette, wxWindowID id) :
	PalettePanel(parent, id),
	palette_name(palette.name),
	choicebook(nullptr),
	toolbar(nullptr),
	m_sortKey(s_defaultSortKey),
	m_sortDir(s_defaultSortDir),
	m_hasSort(s_defaultHasSort),
	m_showLabels(s_defaultShowLabels),
	m_tileSize(s_defaultTileSize) {
	Bind(wxEVT_CHOICEBOOK_PAGE_CHANGING, &BrushPalettePanel::OnSwitchingPage, this);
	Bind(wxEVT_CHOICEBOOK_PAGE_CHANGED, &BrushPalettePanel::OnPageChanged, this);
	Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event) {
		ApplyTheme();
		event.Skip();
	});

	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);

	// Create the tileset panel
	wxSizer* ts_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Tileset");
	wxChoicebook* tmp_choicebook = newd wxChoicebook(static_cast<wxStaticBoxSizer*>(ts_sizer)->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxSize(180, 250));
	ts_sizer->Add(tmp_choicebook, 1, wxEXPAND);
	topsizer->Add(ts_sizer, 1, wxEXPAND);

	const wxSize iconSize = wxWindow::FromDIP(wxSize(16, 16), this);
	const long toolbarStyle = (wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_PLAIN_BACKGROUND) & ~wxAUI_TB_GRIPPER;
	toolbar = newd wxAuiToolBar(tmp_choicebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, toolbarStyle);
	toolbar->SetToolBitmapSize(iconSize);
	toolbar->SetMargins(1, 1, 1, 1);
	toolbar->SetToolBorderPadding(2);
	toolbar->SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	const wxColour iconColor = Theme::Get(Theme::Role::Text);
	toolbar->AddTool(TOOL_SORT_AZ, wxEmptyString, IMAGE_MANAGER.GetBitmap(ICON_SORT_ALPHA_DOWN, iconSize, iconColor), "Sort ascending (A-Z)", wxITEM_NORMAL);
	toolbar->AddTool(TOOL_SORT_ZA, wxEmptyString, IMAGE_MANAGER.GetBitmap(ICON_SORT_ALPHA_UP, iconSize, iconColor), "Sort descending (Z-A)", wxITEM_NORMAL);
	toolbar->AddTool(TOOL_TOGGLE_LABELS, wxEmptyString, IMAGE_MANAGER.GetBitmap(ICON_TAG, iconSize, iconColor), "Toggle labels", wxITEM_CHECK);
	toolbar->AddTool(TOOL_CHANGE_SIZE, wxEmptyString, IMAGE_MANAGER.GetBitmap(ICON_MAXIMIZE, iconSize, iconColor), "Tile size", wxITEM_NORMAL);

	toolbar->ToggleTool(TOOL_TOGGLE_LABELS, m_showLabels);
	toolbar->Realize();

	toolbar->Bind(wxEVT_TOOL, &BrushPalettePanel::OnToolClick, this);

	if (auto* ctrlSizer = tmp_choicebook->GetControlSizer()) {
		ctrlSizer->Add(toolbar, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);
	}

	for (const auto& tileset : palette.tilesets) {
		if (tileset.size() > 0 || tileset.creatureImportTarget != CreatureImportTarget::None) {
			BrushPanel* panel = newd BrushPanel(tmp_choicebook);
			// PaletteCatalog keeps tileset objects stable while allowing their brush membership to grow.
			panel->AssignTileset(&tileset);
			if (m_hasSort) {
				panel->SetSort(m_sortKey, m_sortDir);
			}
			panel->SetShowLabels(m_showLabels);
			panel->SetTileSize(m_tileSize);
			tmp_choicebook->AddPage(panel, wxstr(tileset.name));
		}
	}

	SetSizerAndFit(topsizer);

	choicebook = tmp_choicebook;
}

BrushPalettePanel::~BrushPalettePanel() {
	if (toolbar) {
		toolbar->Unbind(wxEVT_TOOL, &BrushPalettePanel::OnToolClick, this);
	}
}

void BrushPalettePanel::InvalidateContents() {
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		panel->InvalidateContents();
	}
	PalettePanel::InvalidateContents();
}

void BrushPalettePanel::LoadCurrentContents() {
	if (!choicebook) {
		return;
	}
	wxWindow* page = choicebook->GetCurrentPage();
	BrushPanel* panel = dynamic_cast<BrushPanel*>(page);
	if (panel) {
		panel->OnSwitchIn();
	}
	PalettePanel::LoadCurrentContents();
}

void BrushPalettePanel::LoadAllContents() {
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		panel->LoadContents();
	}
	PalettePanel::LoadAllContents();
}

wxString BrushPalettePanel::GetName() const {
	return wxstr(palette_name);
}

void BrushPalettePanel::SetListType(BrushListType ltype) {
	if (ltype == BRUSHLIST_ICONS_32) {
		m_tileSize = 32;
	} else if (ltype == BRUSHLIST_ICONS_64) {
		m_tileSize = 64;
	} else if (ltype == BRUSHLIST_ICONS_128) {
		m_tileSize = 128;
	}
	s_defaultTileSize = m_tileSize;

	if (!choicebook) {
		return;
	}
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		if (panel) {
			panel->SetListType(ltype);
		}
	}
}

void BrushPalettePanel::SetListType(wxString ltype) {
	if (!choicebook) {
		return;
	}
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		panel->SetListType(ltype);
	}
}

Brush* BrushPalettePanel::GetSelectedBrush() const {
	if (!choicebook) {
		return nullptr;
	}
	wxWindow* page = choicebook->GetCurrentPage();
	BrushPanel* panel = dynamic_cast<BrushPanel*>(page);
	Brush* res = nullptr;
	if (panel) {
		for (const auto& toolBar : tool_bars) {
			res = toolBar->GetSelectedBrush();
			if (res) {
				return res;
			}
		}
		res = panel->GetSelectedBrush();
	}
	return res;
}

void BrushPalettePanel::SelectFirstBrush() {
	if (!choicebook) {
		return;
	}
	wxWindow* page = choicebook->GetCurrentPage();
	if (!page) {
		return;
	}
	BrushPanel* panel = dynamic_cast<BrushPanel*>(page);
	if (panel) {
		panel->SelectFirstBrush();
	}
}

bool BrushPalettePanel::SelectBrush(const Brush* whatbrush) {
	if (!choicebook) {
		return false;
	}

	BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetCurrentPage());
	if (!panel) {
		return false;
	}

	for (PalettePanel* toolBar : tool_bars) {
		if (toolBar->SelectBrush(whatbrush)) {
			panel->SelectBrush(nullptr);
			return true;
		}
	}

	if (panel->SelectBrush(whatbrush)) {
		for (PalettePanel* toolBar : tool_bars) {
			toolBar->SelectBrush(nullptr);
		}
		return true;
	}

	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		if ((int)iz == choicebook->GetSelection()) {
			continue;
		}

		panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		if (panel && panel->SelectBrush(whatbrush)) {
			choicebook->ChangeSelection(iz);
			for (PalettePanel* toolBar : tool_bars) {
				toolBar->SelectBrush(nullptr);
			}
			return true;
		}
	}
	return false;
}

void BrushPalettePanel::OnSwitchingPage(wxChoicebookEvent& event) {
	event.Skip();
}

void BrushPalettePanel::OnPageChanged(wxChoicebookEvent& event) {
	if (!choicebook) {
		return;
	}

	BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetCurrentPage());
	Brush* new_brush = nullptr;

	if (panel) {
		panel->OnSwitchIn();
		new_brush = panel->GetSelectedBrush();
	}

	g_gui.ActivatePalette(GetParentPalette());
	if (new_brush) {
		g_gui.SelectBrushInternal(new_brush);
	} else {
		g_gui.SelectBrush();
	}
	Layout();
}

void BrushPalettePanel::OnSwitchIn() {
	g_palettes.ActivatePalette(GetParentPalette());
	g_gui.RestoreBrushSizeState(last_brush_size_state);

	if (m_showLabels != s_defaultShowLabels) {
		SetShowLabels(s_defaultShowLabels);
	}
	if (m_tileSize != s_defaultTileSize) {
		SetTileSize(s_defaultTileSize);
	}
	if (s_defaultHasSort && (!m_hasSort || m_sortKey != s_defaultSortKey || m_sortDir != s_defaultSortDir)) {
		SetSort(s_defaultSortKey, s_defaultSortDir);
	}

	LoadCurrentContents();
}

void BrushPalettePanel::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	m_hasSort = true;
	m_sortKey = key;
	m_sortDir = dir;
	s_defaultHasSort = true;
	s_defaultSortKey = key;
	s_defaultSortDir = dir;

	if (!choicebook) {
		return;
	}
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		if (panel) {
			panel->SetSort(key, dir);
		}
	}
}

void BrushPalettePanel::SetShowLabels(bool show) {
	m_showLabels = show;
	s_defaultShowLabels = show;
	if (toolbar && toolbar->GetToolToggled(TOOL_TOGGLE_LABELS) != show) {
		toolbar->ToggleTool(TOOL_TOGGLE_LABELS, show);
		toolbar->Refresh();
	}

	if (!choicebook) {
		return;
	}
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		if (panel) {
			panel->SetShowLabels(show);
		}
	}
}

void BrushPalettePanel::SetTileSize(int sizePx) {
	m_tileSize = sizePx;
	s_defaultTileSize = sizePx;

	if (!choicebook) {
		return;
	}
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		if (panel) {
			panel->SetTileSize(sizePx);
		}
	}
}

void BrushPalettePanel::ApplyTheme() {
	if (!toolbar) {
		return;
	}
	const wxSize iconSize = wxWindow::FromDIP(wxSize(16, 16), this);
	const wxColour iconColor = Theme::Get(Theme::Role::Text);
	toolbar->SetToolBitmap(TOOL_SORT_AZ, IMAGE_MANAGER.GetBitmap(ICON_SORT_ALPHA_DOWN, iconSize, iconColor));
	toolbar->SetToolBitmap(TOOL_SORT_ZA, IMAGE_MANAGER.GetBitmap(ICON_SORT_ALPHA_UP, iconSize, iconColor));
	toolbar->SetToolBitmap(TOOL_TOGGLE_LABELS, IMAGE_MANAGER.GetBitmap(ICON_TAG, iconSize, iconColor));
	toolbar->SetToolBitmap(TOOL_CHANGE_SIZE, IMAGE_MANAGER.GetBitmap(ICON_MAXIMIZE, iconSize, iconColor));
	toolbar->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	toolbar->SetForegroundColour(Theme::Get(Theme::Role::Text));
	toolbar->Refresh();
}

void BrushPalettePanel::OnToolClick(wxCommandEvent& event) {
	int id = event.GetId();
	if (id == TOOL_SORT_AZ) {
		OnSortButtonClick(TilesetSortDirection::Ascending, id);
	} else if (id == TOOL_SORT_ZA) {
		OnSortButtonClick(TilesetSortDirection::Descending, id);
	} else if (id == TOOL_TOGGLE_LABELS) {
		SetShowLabels(toolbar ? toolbar->GetToolToggled(TOOL_TOGGLE_LABELS) : event.IsChecked());
	} else if (id == TOOL_CHANGE_SIZE) {
		OnSizeButtonClick(id);
	}
}

void BrushPalettePanel::OnSortButtonClick(TilesetSortDirection dir, int toolId) {
	wxMenu menu;
	auto* itemID = menu.AppendCheckItem(MENU_SORT_BY_ID, "By ID");
	auto* itemName = menu.AppendCheckItem(MENU_SORT_BY_NAME, "By Name");
	if (m_sortKey == TilesetSortKey::ID) {
		itemID->Check(true);
	} else {
		itemName->Check(true);
	}

	wxRect rect = toolbar->GetToolRect(toolId);
	wxPoint pos(rect.x, rect.y + rect.height);
	int selected = toolbar->GetPopupMenuSelectionFromUser(menu, pos);
	if (selected == MENU_SORT_BY_ID) {
		SetSort(TilesetSortKey::ID, dir);
	} else if (selected == MENU_SORT_BY_NAME) {
		SetSort(TilesetSortKey::Name, dir);
	}
}

void BrushPalettePanel::OnSizeButtonClick(int toolId) {
	wxMenu menu;
	auto* item32 = menu.AppendCheckItem(MENU_SIZE_32, "32x32");
	auto* item64 = menu.AppendCheckItem(MENU_SIZE_64, "64x64");
	auto* item128 = menu.AppendCheckItem(MENU_SIZE_128, "128x128");

	if (m_tileSize == 64) {
		item64->Check(true);
	} else if (m_tileSize == 128) {
		item128->Check(true);
	} else {
		item32->Check(true);
	}

	wxRect rect = toolbar->GetToolRect(toolId);
	wxPoint pos(rect.x, rect.y + rect.height);
	int selected = toolbar->GetPopupMenuSelectionFromUser(menu, pos);
	if (selected == MENU_SIZE_32) {
		SetTileSize(32);
	} else if (selected == MENU_SIZE_64) {
		SetTileSize(64);
	} else if (selected == MENU_SIZE_128) {
		SetTileSize(128);
	}
}

