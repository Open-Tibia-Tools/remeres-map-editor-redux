#include "palette/panels/brush_palette_panel.h"
#include "ui/gui.h"
#include "game/materials.h"
#include "palette/palette_window.h"
#include "palette/controls/tileset_grid_toolbar.h"

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
	choicebook(nullptr) {
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

	{
		TilesetGridToolbar::Actions actions;
		actions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
			SetSort(key, dir);
		};
		actions.clear_sort = [this]() {
			ClearSort();
		};
		actions.show_labels = [this](bool show) {
			SetShowLabels(show);
		};
		actions.icon_size = [this](int size) {
			SetTileSize(size);
		};
		actions.filter = [this](const std::string& query) {
			m_filterQuery = query;
			ApplyFilter();
		};
		actions.filter_committed = [this]() {
			CommitFilter();
		};
		actions.filter_cleared = [this]() {
			ResetFilter();
		};
		actions.filter_focus_lost = [this]() {
			SavePaletteFilters();
		};
		actions.filter_all = [this](bool all) {
			m_filterAll = all;
			s_defaultFilterAll = all;
			g_settings.setInteger(Config::PALETTE_FILTER_ALL, all ? 1 : 0);
			SavePaletteFilters();
			if (choicebook && choicebook->GetChoiceCtrl()) {
				choicebook->GetChoiceCtrl()->Enable(!all || m_filterQuery.empty());
			}
			ApplyFilter();
		};

		gridToolbar = newd TilesetGridToolbar(tmp_choicebook, std::move(actions), true);
		gridToolbar->SetStacked(true);
		gridToolbar->SetState(m_showLabels, m_tileSize, m_hasSort, m_sortKey, m_sortDir, m_filterAll);
		gridToolbar->SetFilterQuery(m_filterQuery);
	}
	wxChoice* choice = tmp_choicebook->GetChoiceCtrl();
	auto* ctrlSizer = dynamic_cast<wxBoxSizer*>(tmp_choicebook->GetControlSizer());
	if (ctrlSizer && choice) {
		ctrlSizer->Detach(choice);
		ctrlSizer->SetOrientation(wxVERTICAL);

		wxBoxSizer* topRowSizer = newd wxBoxSizer(wxHORIZONTAL);
		topRowSizer->Add(choice, 1, wxALIGN_CENTER_VERTICAL);
		ctrlSizer->Add(topRowSizer, 0, wxEXPAND | wxBOTTOM, 2);
		ctrlSizer->Add(gridToolbar, 0, wxEXPAND | wxBOTTOM, 2);

		if (m_filterAll && !m_filterQuery.empty()) {
			choice->Enable(false);
		}
		tmp_choicebook->Layout();
	}

	for (const auto& tileset : palette.tilesets) {
		if (tileset.size() > 0 || tileset.is_import_target) {
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

	if (gridToolbar) {
		gridToolbar->SetState(m_showLabels, m_tileSize, m_hasSort, m_sortKey, m_sortDir, m_filterAll);
		gridToolbar->SetFilterQuery(m_filterQuery);
	}
	if (choicebook && choicebook->GetChoiceCtrl()) {
		choicebook->GetChoiceCtrl()->Enable(!m_filterAll || m_filterQuery.empty());
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

	if (gridToolbar) {
		gridToolbar->SetState(m_showLabels, m_tileSize, m_hasSort, m_sortKey, m_sortDir, m_filterAll);
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
	if (gridToolbar) {
		gridToolbar->ApplyTheme();
	}
}

void BrushPalettePanel::CommitFilter() {
	if (!choicebook) {
		return;
	}

	SavePaletteFilters();

	wxWindow* w = GetParent();
	PaletteWindow* pw = nullptr;
	while (w) {
		pw = dynamic_cast<PaletteWindow*>(w);
		if (pw) {
			g_gui.ActivatePalette(pw);
			break;
		}
		w = w->GetParent();
	}

	BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetCurrentPage());
	if (!panel) {
		return;
	}

	panel->SelectFirstBrush();
	Brush* brush = panel->GetSelectedBrush();
	if (!brush) {
		return;
	}

	if (m_filterAll && pw) {
		pw->JumpToBrush(brush, palette_name);
	} else {
		g_gui.SelectBrushInternal(brush);
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

	if (m_filterAll && !m_filterQuery.empty()) {
		std::vector<Brush*> allBrushes;
		std::unordered_set<const Brush*> seen;
		const auto& palettes = g_materials.paletteCatalog().dynamicPalettes();
		for (const auto& pal : palettes) {
			for (const auto& ts : pal.tilesets) {
				for (Brush* b : ts.brushes) {
					if (b && seen.insert(b).second) {
						allBrushes.push_back(b);
					}
				}
			}
		}
		panel->SetFilterQuery(m_filterQuery, &allBrushes);
		if (choicebook && choicebook->GetChoiceCtrl()) {
			choicebook->GetChoiceCtrl()->Enable(false);
		}
	} else {
		panel->SetFilterQuery(m_filterQuery, nullptr);
		if (choicebook && choicebook->GetChoiceCtrl()) {
			choicebook->GetChoiceCtrl()->Enable(true);
		}
	}
}

void BrushPalettePanel::ResetFilter() {
	if (gridToolbar) {
		gridToolbar->SetFilterQuery(std::string());
	}
	m_filterQuery.clear();
	SavePaletteFilters();
	ApplyFilter();
}

bool BrushPalettePanel::JumpToTilesetAndBrush(std::string_view tilesetName, const Brush* brush) {
	if (!choicebook || !brush) {
		return false;
	}

	// 1. Clear active filter on this panel
	if (gridToolbar) {
		gridToolbar->SetFilterQuery(std::string());
	}
	m_filterQuery.clear();
	SavePaletteFilters();

	// 2. Find target tileset page
	int targetIndex = wxNOT_FOUND;
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		auto* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		if (panel) {
			if (panel->GetTileset() && panel->GetTileset()->name == tilesetName) {
				targetIndex = static_cast<int>(iz);
				break;
			} else if (nstr(choicebook->GetPageText(iz)) == tilesetName) {
				targetIndex = static_cast<int>(iz);
				break;
			}
		}
	}

	// 3. Switch to target tileset page if found
	if (targetIndex != wxNOT_FOUND) {
		if (choicebook->GetSelection() != targetIndex) {
			choicebook->SetSelection(targetIndex);
		}
	}

	// 4. Ensure filter is cleared and choice control is enabled on current page
	ApplyFilter();

	// 5. Select the brush in the current page
	auto* currentPanel = dynamic_cast<BrushPanel*>(choicebook->GetCurrentPage());
	bool selected = false;
	if (currentPanel) {
		selected = currentPanel->SelectBrush(brush);
	}

	// If not found in current page, search across other pages in this choicebook
	if (!selected) {
		for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
			auto* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
			if (panel && panel->SelectBrush(brush)) {
				choicebook->SetSelection(iz);
				selected = true;
				break;
			}
		}
	}

	// 6. Activate in GUI
	if (selected) {
		g_gui.ActivatePalette(GetParentPalette());
		g_gui.SelectBrushInternal(const_cast<Brush*>(brush));
		Layout();
		Refresh();
	}

	return selected;
}


