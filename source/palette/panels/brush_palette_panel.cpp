#include "palette/panels/brush_palette_panel.h"
#include "ui/gui.h"
#include "game/materials.h"
#include "palette/palette_window.h"
#include "util/image_manager.h"
#include "ui/theme.h"
#include <spdlog/spdlog.h>
#include <wx/menu.h>
#include <wx/srchctrl.h>

TilesetSortKey BrushPalettePanel::s_defaultSortKey = TilesetSortKey::Name;
TilesetSortDirection BrushPalettePanel::s_defaultSortDir = TilesetSortDirection::Ascending;
bool BrushPalettePanel::s_defaultHasSort = false;
bool BrushPalettePanel::s_defaultShowLabels = false;
int BrushPalettePanel::s_defaultTileSize = 32;
bool BrushPalettePanel::s_defaultFilterAll = false;
bool BrushPalettePanel::s_defaultsLoaded = false;

namespace {
std::string toLowerString(std::string_view s) {
	std::string res;
	res.reserve(s.size());
	for (char c : s) {
		res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	}
	return res;
}

class PaletteSearchCtrl final : public wxSearchCtrl {
public:
	using wxSearchCtrl::wxSearchCtrl;

	bool IsTopNavigationDomain(NavigationKind kind) const override {
		if (kind == Navigation_Accel) {
			return true;
		}
		return wxSearchCtrl::IsTopNavigationDomain(kind);
	}

#ifdef __WXMSW__
	bool MSWTranslateMessage(WXMSG* msg) override {
		return false;
	}

	bool MSWShouldPreProcessMessage(WXMSG* msg) override {
		return false;
	}
#endif
};

class PaletteChoicebook final : public wxChoicebook {
public:
	PaletteChoicebook(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0) :
		wxChoicebook(parent, id, pos, size, style) {
	}

	wxRect GetPageRect() const override {
		wxRect rectPage(wxPoint(0, 0), GetClientSize());
		int controllerHeight = 0;
		if (auto* ctrlSizer = GetControlSizer()) {
			const int sizerY = std::max(0, ctrlSizer->GetPosition().y);
			const int sizerH = std::max(ctrlSizer->GetSize().y, ctrlSizer->GetMinSize().y);
			controllerHeight = sizerY + sizerH;
		} else if (const auto* choice = GetChoiceCtrl()) {
			controllerHeight = choice->GetPosition().y + choice->GetSize().y;
		}
		const int topOffset = controllerHeight + static_cast<int>(GetInternalBorder());
		rectPage.y = topOffset;
		rectPage.height = std::max(0, rectPage.height - topOffset);
		return rectPage;
	}

	wxSize CalcSizeFromPage(const wxSize& sizePage) const override {
		if (!GetChoiceCtrl() || !GetChoiceCtrl()->IsShown()) {
			return sizePage;
		}
		wxSize size = sizePage;
		int controllerHeight = 0;
		if (auto* ctrlSizer = GetControlSizer()) {
			controllerHeight = std::max(ctrlSizer->GetSize().y, ctrlSizer->GetMinSize().y);
		} else if (const auto* choice = GetChoiceCtrl()) {
			controllerHeight = choice->GetBestHeight(sizePage.x);
		}
		size.y += controllerHeight + static_cast<int>(GetInternalBorder());
		return size;
	}
};
}

void BrushPalettePanel::EnsureDefaultsLoaded() {
	if (s_defaultsLoaded) {
		return;
	}
	s_defaultsLoaded = true;

	s_defaultHasSort = g_settings.getBoolean(Config::PALETTE_HAS_SORT);
	s_defaultSortKey = static_cast<TilesetSortKey>(g_settings.getInteger(Config::PALETTE_SORT_KEY));
	s_defaultSortDir = static_cast<TilesetSortDirection>(g_settings.getInteger(Config::PALETTE_SORT_DIR));
	s_defaultShowLabels = g_settings.getBoolean(Config::PALETTE_SHOW_LABELS);

	int loadedSize = g_settings.getInteger(Config::PALETTE_TILE_SIZE);
	if (loadedSize == 32 || loadedSize == 64 || loadedSize == 128) {
		s_defaultTileSize = loadedSize;
	} else {
		std::string dynStyle = g_settings.getString(Config::PALETTE_DYNAMIC_STYLE);
		if (dynStyle == "64x64 px") {
			s_defaultTileSize = 64;
		} else if (dynStyle == "128x128 px") {
			s_defaultTileSize = 128;
		} else {
			s_defaultTileSize = 32;
		}
	}
	s_defaultFilterAll = g_settings.getBoolean(Config::PALETTE_FILTER_ALL);
}

void BrushPalettePanel::LoadPaletteFilters() {
	EnsureDefaultsLoaded();
	m_filterAll = s_defaultFilterAll;
	m_filterQuery.clear();

	std::string keyName = toLowerString(palette_name);
	auto& root = g_settings.getTable();
	if (auto* pTable = root.get_as<toml::table>("palette_filters")) {
		if (auto* sub = pTable->get_as<toml::table>(keyName)) {
			m_filterQuery = (*sub)["query"].value_or(std::string{});
			m_filterAll = (*sub)["all"].value_or(s_defaultFilterAll);
		} else {
			m_filterQuery = (*pTable)[keyName + "_query"].value_or(std::string{});
			m_filterAll = (*pTable)[keyName + "_all"].value_or(s_defaultFilterAll);
		}
	} else {
		m_filterQuery = g_settings.getString(Config::PALETTE_FILTER_QUERY);
	}
}

void BrushPalettePanel::SavePaletteFilters() {
	std::string keyName = toLowerString(palette_name);
	if (keyName.empty()) {
		return;
	}
	auto& root = g_settings.getTable();
	auto* pTable = root.get_as<toml::table>("palette_filters");
	if (!pTable) {
		root.insert_or_assign("palette_filters", toml::table{});
		pTable = root.get_as<toml::table>("palette_filters");
	}
	if (pTable) {
		auto* sub = pTable->get_as<toml::table>(keyName);
		if (!sub) {
			pTable->insert_or_assign(keyName, toml::table{});
			sub = pTable->get_as<toml::table>(keyName);
		}
		if (sub) {
			sub->insert_or_assign("query", m_filterQuery);
			sub->insert_or_assign("all", m_filterAll);
		}
	}
	if (!m_filterQuery.empty()) {
		g_settings.setString(Config::PALETTE_FILTER_QUERY, m_filterQuery);
		g_settings.setInteger(Config::PALETTE_FILTER_ALL, m_filterAll ? 1 : 0);
	}
	g_settings.save();
}

// ============================================================================
// Brush Palette Panel
// A common class for terrain/doodad/item/raw palette

BrushPalettePanel::BrushPalettePanel(wxWindow* parent, const DynamicPaletteDefinition& palette, wxWindowID id) :
	PalettePanel(parent, id),
	palette_name(palette.name),
	m_paletteDef(&palette),
	choicebook(nullptr),
	toolbar(nullptr),
	m_searchCtrl(nullptr),
	m_searchToolbar(nullptr) {
	EnsureDefaultsLoaded();
	LoadPaletteFilters();
	m_sortKey = s_defaultSortKey;
	m_sortDir = s_defaultSortDir;
	m_hasSort = s_defaultHasSort;
	m_showLabels = s_defaultShowLabels;
	m_tileSize = s_defaultTileSize;
	Bind(wxEVT_CHOICEBOOK_PAGE_CHANGING, &BrushPalettePanel::OnSwitchingPage, this);
	Bind(wxEVT_CHOICEBOOK_PAGE_CHANGED, &BrushPalettePanel::OnPageChanged, this);
	Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event) {
		ApplyTheme();
		event.Skip();
	});

	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);

	// Create the tileset panel
	wxSizer* ts_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Tileset");
	PaletteChoicebook* tmp_choicebook = newd PaletteChoicebook(static_cast<wxStaticBoxSizer*>(ts_sizer)->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxSize(180, 250));
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

	m_searchToolbar = newd wxAuiToolBar(tmp_choicebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, toolbarStyle);
	m_searchToolbar->SetToolBitmapSize(iconSize);
	m_searchToolbar->SetMargins(1, 1, 1, 1);
	m_searchToolbar->SetToolBorderPadding(2);
	m_searchToolbar->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	m_searchToolbar->AddTool(TOOL_FILTER_ALL, wxEmptyString, IMAGE_MANAGER.GetBitmap(ICON_FILTER, iconSize, iconColor), "Filter all tilesets", wxITEM_CHECK);
	m_searchToolbar->ToggleTool(TOOL_FILTER_ALL, m_filterAll);
	m_searchToolbar->Realize();
	m_searchToolbar->Bind(wxEVT_TOOL, &BrushPalettePanel::OnToolClick, this);

	m_searchCtrl = newd PaletteSearchCtrl(tmp_choicebook, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	m_searchCtrl->SetDescriptiveText("Search...");
	m_searchCtrl->ShowCancelButton(true);
	m_searchCtrl->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	m_searchCtrl->SetForegroundColour(Theme::Get(Theme::Role::Text));
	m_searchCtrl->SetMinSize(wxSize(60, -1));
	if (!m_filterQuery.empty()) {
		m_searchCtrl->ChangeValue(wxstr(m_filterQuery));
	}

	m_searchCtrl->Bind(wxEVT_TEXT, &BrushPalettePanel::OnSearchText, this);
	m_searchCtrl->Bind(wxEVT_SEARCHCTRL_CANCEL_BTN, &BrushPalettePanel::OnSearchCancel, this);
	m_searchCtrl->Bind(wxEVT_SEARCHCTRL_SEARCH_BTN, &BrushPalettePanel::OnSearchText, this);
	m_searchCtrl->Bind(wxEVT_TEXT_ENTER, &BrushPalettePanel::OnSearchText, this);

	auto bindCharHook = [this](wxWindow* w) {
		if (!w) {
			return;
		}
		w->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& evt) {
			if (evt.GetKeyCode() == WXK_ESCAPE) {
				if (m_searchCtrl && !m_searchCtrl->GetValue().empty()) {
					m_searchCtrl->ChangeValue(wxEmptyString);
					m_filterQuery.clear();
					SavePaletteFilters();
					ApplyFilter();
					return;
				}
			}
			evt.Skip();
		});
	};
	bindCharHook(m_searchCtrl);
	for (wxWindow* child : m_searchCtrl->GetChildren()) {
		bindCharHook(child);
	}

	wxChoice* choice = tmp_choicebook->GetChoiceCtrl();
	auto* ctrlSizer = dynamic_cast<wxBoxSizer*>(tmp_choicebook->GetControlSizer());
	if (ctrlSizer && choice) {
		ctrlSizer->Detach(choice);
		ctrlSizer->SetOrientation(wxVERTICAL);

		wxBoxSizer* topRowSizer = newd wxBoxSizer(wxHORIZONTAL);
		topRowSizer->Add(choice, 1, wxALIGN_CENTER_VERTICAL);
		topRowSizer->Add(toolbar, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);
		ctrlSizer->Add(topRowSizer, 0, wxEXPAND | wxBOTTOM, 2);

		wxBoxSizer* searchRowSizer = newd wxBoxSizer(wxHORIZONTAL);
		searchRowSizer->Add(m_searchCtrl, 1, wxALIGN_CENTER_VERTICAL);
		searchRowSizer->Add(m_searchToolbar, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);
		ctrlSizer->Add(searchRowSizer, 0, wxEXPAND | wxBOTTOM, 2);

		if (m_filterAll) {
			choice->Enable(false);
		}
		tmp_choicebook->Layout();
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
	ApplyFilter();
}

BrushPalettePanel::~BrushPalettePanel() {
	SavePaletteFilters();
	if (toolbar) {
		toolbar->Unbind(wxEVT_TOOL, &BrushPalettePanel::OnToolClick, this);
	}
	if (m_searchToolbar) {
		m_searchToolbar->Unbind(wxEVT_TOOL, &BrushPalettePanel::OnToolClick, this);
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
		ApplyFilter();
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

	g_settings.setInteger(Config::PALETTE_TILE_SIZE, m_tileSize);
	std::string style = std::to_string(m_tileSize) + "x" + std::to_string(m_tileSize) + " px";
	g_settings.setString(Config::PALETTE_DYNAMIC_STYLE, style);

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
	if (ltype == "32x32 px" || ltype == "small icons" || ltype == "large icons") {
		SetListType(BRUSHLIST_ICONS_32);
	} else if (ltype == "64x64 px") {
		SetListType(BRUSHLIST_ICONS_64);
	} else if (ltype == "128x128 px") {
		SetListType(BRUSHLIST_ICONS_128);
	} else if (ltype == "listbox" || ltype == "List style") {
		SetListType(BRUSHLIST_LISTBOX);
	} else if (ltype == "textlistbox") {
		SetListType(BRUSHLIST_TEXT_LISTBOX);
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
		ApplyFilter();
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
	if (s_defaultHasSort) {
		if (!m_hasSort || m_sortKey != s_defaultSortKey || m_sortDir != s_defaultSortDir) {
			SetSort(s_defaultSortKey, s_defaultSortDir);
		}
	} else if (m_hasSort) {
		ClearSort();
	}

	if (m_searchToolbar && m_searchToolbar->GetToolToggled(TOOL_FILTER_ALL) != m_filterAll) {
		m_searchToolbar->ToggleTool(TOOL_FILTER_ALL, m_filterAll);
		m_searchToolbar->Refresh();
	}
	if (choicebook && choicebook->GetChoiceCtrl()) {
		choicebook->GetChoiceCtrl()->Enable(!m_filterAll);
	}
	if (m_searchCtrl && m_searchCtrl->GetValue().ToStdString() != m_filterQuery) {
		m_searchCtrl->ChangeValue(wxstr(m_filterQuery));
	}

	LoadCurrentContents();
	ApplyFilter();
}


void BrushPalettePanel::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	m_hasSort = true;
	m_sortKey = key;
	m_sortDir = dir;
	s_defaultHasSort = true;
	s_defaultSortKey = key;
	s_defaultSortDir = dir;

	g_settings.setInteger(Config::PALETTE_HAS_SORT, 1);
	g_settings.setInteger(Config::PALETTE_SORT_KEY, static_cast<int>(key));
	g_settings.setInteger(Config::PALETTE_SORT_DIR, static_cast<int>(dir));
	g_settings.save();

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

void BrushPalettePanel::ClearSort() {
	m_hasSort = false;
	s_defaultHasSort = false;

	g_settings.setInteger(Config::PALETTE_HAS_SORT, 0);
	g_settings.save();

	if (!choicebook) {
		return;
	}
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		if (panel) {
			panel->ClearSort();
		}
	}
}

void BrushPalettePanel::SetShowLabels(bool show) {
	m_showLabels = show;
	s_defaultShowLabels = show;

	g_settings.setInteger(Config::PALETTE_SHOW_LABELS, show ? 1 : 0);
	g_settings.save();

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

	g_settings.setInteger(Config::PALETTE_TILE_SIZE, sizePx);
	std::string style = std::to_string(sizePx) + "x" + std::to_string(sizePx) + " px";
	g_settings.setString(Config::PALETTE_DYNAMIC_STYLE, style);
	g_settings.save();

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
	const wxSize iconSize = wxWindow::FromDIP(wxSize(16, 16), this);
	const wxColour iconColor = Theme::Get(Theme::Role::Text);
	if (toolbar) {
		toolbar->SetToolBitmap(TOOL_SORT_AZ, IMAGE_MANAGER.GetBitmap(ICON_SORT_ALPHA_DOWN, iconSize, iconColor));
		toolbar->SetToolBitmap(TOOL_SORT_ZA, IMAGE_MANAGER.GetBitmap(ICON_SORT_ALPHA_UP, iconSize, iconColor));
		toolbar->SetToolBitmap(TOOL_TOGGLE_LABELS, IMAGE_MANAGER.GetBitmap(ICON_TAG, iconSize, iconColor));
		toolbar->SetToolBitmap(TOOL_CHANGE_SIZE, IMAGE_MANAGER.GetBitmap(ICON_MAXIMIZE, iconSize, iconColor));
		toolbar->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
		toolbar->SetForegroundColour(Theme::Get(Theme::Role::Text));
		toolbar->Refresh();
	}
	if (m_searchToolbar) {
		m_searchToolbar->SetToolBitmap(TOOL_FILTER_ALL, IMAGE_MANAGER.GetBitmap(ICON_FILTER, iconSize, iconColor));
		m_searchToolbar->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
		m_searchToolbar->SetForegroundColour(Theme::Get(Theme::Role::Text));
		m_searchToolbar->Refresh();
	}
	if (m_searchCtrl) {
		m_searchCtrl->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
		m_searchCtrl->SetForegroundColour(Theme::Get(Theme::Role::Text));
		for (wxWindow* child : m_searchCtrl->GetChildren()) {
			if (child) {
				child->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
				child->SetForegroundColour(Theme::Get(Theme::Role::Text));
				child->Refresh();
			}
		}
		m_searchCtrl->Refresh();
	}
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
	} else if (id == TOOL_FILTER_ALL) {
		m_filterAll = m_searchToolbar ? m_searchToolbar->GetToolToggled(TOOL_FILTER_ALL) : event.IsChecked();
		s_defaultFilterAll = m_filterAll;
		g_settings.setInteger(Config::PALETTE_FILTER_ALL, m_filterAll ? 1 : 0);
		SavePaletteFilters();
		if (choicebook && choicebook->GetChoiceCtrl()) {
			choicebook->GetChoiceCtrl()->Enable(!m_filterAll);
		}
		ApplyFilter();
	}
}

void BrushPalettePanel::OnSortButtonClick(TilesetSortDirection dir, int toolId) {
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

	wxRect rect = toolbar->GetToolRect(toolId);
	wxPoint pos(rect.x, rect.y + rect.height);
	int selected = toolbar->GetPopupMenuSelectionFromUser(menu, pos);
	if (selected == MENU_SORT_BY_ID) {
		SetSort(TilesetSortKey::ID, dir);
	} else if (selected == MENU_SORT_BY_NAME) {
		SetSort(TilesetSortKey::Name, dir);
	} else if (selected == MENU_SORT_DEFAULT) {
		ClearSort();
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

void BrushPalettePanel::OnSearchText(wxCommandEvent& event) {
	if (m_searchCtrl) {
		m_filterQuery = m_searchCtrl->GetValue().ToStdString();
		SavePaletteFilters();
		ApplyFilter();

		const auto eventType = event.GetEventType();
		if ((eventType == wxEVT_TEXT_ENTER || eventType == wxEVT_SEARCHCTRL_SEARCH_BTN) && choicebook) {
			wxWindow* w = GetParent();
			while (w) {
				PaletteWindow* pw = dynamic_cast<PaletteWindow*>(w);
				if (pw) {
					g_gui.ActivatePalette(pw);
					break;
				}
				w = w->GetParent();
			}

			BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetCurrentPage());
			if (panel) {
				panel->SelectFirstBrush();
				Brush* brush = panel->GetSelectedBrush();
				if (brush) {
					g_gui.SelectBrushInternal(brush);
				}
			}
		}
	}
}

void BrushPalettePanel::OnSearchCancel(wxCommandEvent& event) {
	if (m_searchCtrl) {
		m_searchCtrl->ChangeValue(wxEmptyString);
		m_filterQuery.clear();
		SavePaletteFilters();
		ApplyFilter();
	}
}

void BrushPalettePanel::ApplyFilter() {
	if (!choicebook) {
		return;
	}
	BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetCurrentPage());
	if (!panel) {
		return;
	}

	if (m_filterAll && m_paletteDef) {
		std::vector<Brush*> allBrushes;
		std::unordered_set<const Brush*> seen;
		for (const auto& ts : m_paletteDef->tilesets) {
			for (Brush* b : ts.brushes) {
				if (b && seen.insert(b).second) {
					allBrushes.push_back(b);
				}
			}
		}
		panel->SetFilterQuery(m_filterQuery, &allBrushes);
	} else {
		panel->SetFilterQuery(m_filterQuery, nullptr);
	}
}


