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
bool BrushPalettePanel::s_defaultIsListMode = false;
bool BrushPalettePanel::s_defaultIndicatorDogEar = false;
int BrushPalettePanel::s_defaultGridSpacing = 4;
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
		Bind(wxEVT_SIZE, &PaletteChoicebook::OnSize, this);
	}

	void SetHorizontalLayout(bool horizontal) {
		m_horizontal = horizontal;
	}

	wxRect GetPageRect() const override {
		wxRect rectPage(wxPoint(0, 0), GetClientSize());
		if (m_horizontal) {
			const int leftWidth = FromDIP(PALETTE_HORIZONTAL_LEFT_WIDTH);
			rectPage.x = leftWidth + static_cast<int>(GetInternalBorder());
			rectPage.y = 0;
			rectPage.width = std::max(0, rectPage.width - rectPage.x);
			return rectPage;
		}
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
		if (m_horizontal) {
			wxSize size = sizePage;
			size.x += FromDIP(PALETTE_HORIZONTAL_LEFT_WIDTH) + static_cast<int>(GetInternalBorder());
			return size;
		}
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

	bool Layout() override {
		if (m_horizontal) {
			const int leftWidth = FromDIP(PALETTE_HORIZONTAL_LEFT_WIDTH);
			if (auto* ctrlSizer = GetControlSizer()) {
				ctrlSizer->SetDimension(0, 0, leftWidth, GetClientSize().y);
				ctrlSizer->Layout();
			}
			if (auto* choice = GetChoiceCtrl()) {
				choice->SetMaxSize(wxSize(leftWidth, -1));
#ifdef __WXMSW__
				if (choice->GetHWND()) {
					::SendMessage(choice->GetHWND(), CB_SETDROPPEDWIDTH, leftWidth, 0);
				}
#endif
			}
			const int sel = GetSelection();
			if (sel != wxNOT_FOUND && static_cast<size_t>(sel) < GetPageCount()) {
				if (wxWindow* page = GetPage(static_cast<size_t>(sel))) {
					page->SetSize(GetPageRect());
					page->Layout();
				}
			}
			return true;
		}
		return wxChoicebook::Layout();
	}

	void OnSize(wxSizeEvent& event) {
		if (m_horizontal) {
			Layout();
			return;
		}
		event.Skip();
	}

private:
	bool m_horizontal = false;
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

	std::string dynStyle = g_settings.getString(Config::PALETTE_DYNAMIC_STYLE);
	if (dynStyle == "listbox" || dynStyle == "List style") {
		s_defaultIsListMode = true;
	} else {
		s_defaultIsListMode = false;
	}

	int loadedSize = g_settings.getInteger(Config::PALETTE_TILE_SIZE);
	if (loadedSize == 32 || loadedSize == 64 || loadedSize == 128) {
		s_defaultTileSize = loadedSize;
	} else {
		if (dynStyle == "64x64 px") {
			s_defaultTileSize = 64;
		} else if (dynStyle == "128x128 px") {
			s_defaultTileSize = 128;
		} else {
			s_defaultTileSize = 32;
		}
	}
	s_defaultFilterAll = g_settings.getBoolean(Config::PALETTE_FILTER_ALL);
		s_defaultIndicatorDogEar = g_settings.getBoolean(Config::PALETTE_INDICATOR_DOGEAR);
	int loadedSpacing = g_settings.getInteger(Config::PALETTE_GRID_SPACING);
	s_defaultGridSpacing = (loadedSpacing == 2 || loadedSpacing == 6) ? loadedSpacing : 2;
}

void BrushPalettePanel::LoadPaletteFilters() {
	EnsureDefaultsLoaded();
	m_filterAll = s_defaultFilterAll;
		m_indicatorDogEar = s_defaultIndicatorDogEar;
	m_gridSpacing = s_defaultGridSpacing;
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
	m_isListMode = s_defaultIsListMode;
	Bind(wxEVT_CHOICEBOOK_PAGE_CHANGING, &BrushPalettePanel::OnSwitchingPage, this);
	Bind(wxEVT_CHOICEBOOK_PAGE_CHANGED, &BrushPalettePanel::OnPageChanged, this);
	Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event) {
		ApplyTheme();
		event.Skip();
	});

	wxSizer* topsizer = new wxBoxSizer(wxVERTICAL);

	// Create the tileset panel
	ts_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Tileset");
	PaletteChoicebook* tmp_cb = new PaletteChoicebook(ts_sizer->GetStaticBox(), wxID_ANY, wxDefaultPosition, FromDIP(wxSize(180, 250)));
	choicebook = tmp_cb;
	ts_sizer->Add(tmp_cb, wxSizerFlags(1).Expand());
	topsizer->Add(ts_sizer, wxSizerFlags(1).Expand());

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
			if (m_isListMode) {
				SetDisplayMode(TilesetDisplayMode::Grid);
			}
			SetTileSize(size);
		};
		actions.list_mode = [this](bool list) {
			SetDisplayMode(list ? TilesetDisplayMode::List : TilesetDisplayMode::Grid);
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
		actions.brush_indicators = [this](bool dogEar) {
			SetBrushIndicators(dogEar);
		};
		actions.grid_spacing = [this](int spacing) {
			SetGridSpacing(spacing);
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

		gridToolbar = new TilesetGridToolbar(tmp_cb, std::move(actions), TilesetGridToolbar::Part_Standard | TilesetGridToolbar::Part_FilterAll);
		gridToolbar->SetStacked(true);
		gridToolbar->SetState(m_showLabels, m_tileSize, m_hasSort, m_sortKey, m_sortDir, m_filterAll, m_isListMode);
		gridToolbar->SetIndicatorState(m_indicatorDogEar);
		gridToolbar->SetGridSpacing(m_gridSpacing);
		gridToolbar->SetFilterQuery(m_filterQuery);
	}
	wxChoice* choice = tmp_cb->GetChoiceCtrl();
	auto* ctrlSizer = dynamic_cast<wxBoxSizer*>(tmp_cb->GetControlSizer());
	if (ctrlSizer && choice) {
		ctrlSizer->Detach(choice);
		ctrlSizer->SetOrientation(wxVERTICAL);

		wxBoxSizer* topRowSizer = new wxBoxSizer(wxHORIZONTAL);
		topRowSizer->Add(choice, wxSizerFlags(1).CenterVertical());
		ctrlSizer->Add(topRowSizer, wxSizerFlags().Expand().Border(wxBOTTOM, 2));
		ctrlSizer->Add(gridToolbar, wxSizerFlags().Expand().Border(wxBOTTOM, 2));

		if (m_filterAll && !m_filterQuery.empty()) {
			choice->Enable(false);
		}
		tmp_cb->Layout();
	}

	for (const auto& tileset : palette.tilesets) {
		if (tileset.size() > 0 || tileset.is_import_target) {
			BrushPanel* panel = new BrushPanel(tmp_cb);
			// PaletteCatalog keeps tileset objects stable while allowing their brush membership to grow.
			panel->AssignTileset(&tileset);
			if (m_hasSort) {
				panel->SetSort(m_sortKey, m_sortDir);
			}
			panel->SetShowLabels(m_showLabels);
			panel->SetTileSize(m_tileSize);
			panel->SetDisplayMode(m_isListMode ? TilesetDisplayMode::List : TilesetDisplayMode::Grid);
			panel->SetBrushIndicators(m_indicatorDogEar);
			panel->SetGridSpacing(m_gridSpacing);
			tmp_cb->AddPage(panel, wxstr(tileset.name));
		}
	}

	SetSizerAndFit(topsizer);

	// choicebook already assigned
	if (choicebook && choicebook->GetPageCount() > 0 && choicebook->GetSelection() == wxNOT_FOUND) {
		choicebook->SetSelection(0);
	}
	ApplyFilter();
}

BrushPalettePanel::~BrushPalettePanel() {
	DetachPaletteChoice();
	SavePaletteFilters();
}

void BrushPalettePanel::SetHorizontalLayout(bool horizontal) {
	if (m_isHorizontal == horizontal) {
		return;
	}
	m_isHorizontal = horizontal;

	Freeze();
	if (choicebook) {
		if (auto* paletteChoicebook = dynamic_cast<PaletteChoicebook*>(choicebook)) {
			paletteChoicebook->SetHorizontalLayout(horizontal);
		}
	}

	wxChoice* choice = choicebook ? choicebook->GetChoiceCtrl() : nullptr;

	if (horizontal) {
		if (ts_sizer) {
			ts_sizer->Detach(choicebook);
			GetSizer()->Detach(ts_sizer);
		}
		if (choicebook) {
			if (choicebook->GetParent() != this) {
				choicebook->Reparent(this);
			}
			choicebook->Show(true);
		}
		if (ts_sizer) {
			ts_sizer->GetStaticBox()->Show(false);
		}
		GetSizer()->Add(choicebook, wxSizerFlags(1).Expand());
		const int leftWidth = FromDIP(PALETTE_HORIZONTAL_LEFT_WIDTH);
		if (choice) {
			choice->SetMaxSize(wxSize(leftWidth, -1));
#ifdef __WXMSW__
			if (choice->GetHWND()) {
				::SendMessage(choice->GetHWND(), CB_SETDROPPEDWIDTH, leftWidth, 0);
			}
#endif
		}
		if (m_attachedPaletteChoice) {
			m_attachedPaletteChoice->SetMaxSize(wxSize(leftWidth, -1));
#ifdef __WXMSW__
			if (m_attachedPaletteChoice->GetHWND()) {
				::SendMessage(m_attachedPaletteChoice->GetHWND(), CB_SETDROPPEDWIDTH, leftWidth, 0);
			}
#endif
		}
		if (gridToolbar) {
			gridToolbar->SetMaxSize(wxSize(leftWidth, -1));
		}
	} else {
		GetSizer()->Detach(choicebook);
		if (ts_sizer) {
			ts_sizer->GetStaticBox()->Show(true);
			if (choicebook) {
				if (choicebook->GetParent() != ts_sizer->GetStaticBox()) {
					choicebook->Reparent(ts_sizer->GetStaticBox());
				}
				choicebook->Show(true);
			}
			ts_sizer->Add(choicebook, wxSizerFlags(1).Expand());
			GetSizer()->Add(ts_sizer, wxSizerFlags(1).Expand());
		}
		if (choice) {
			choice->SetMaxSize(wxDefaultSize);
#ifdef __WXMSW__
			if (choice->GetHWND()) {
				::SendMessage(choice->GetHWND(), CB_SETDROPPEDWIDTH, 0, 0);
			}
#endif
		}
		if (m_attachedPaletteChoice) {
			m_attachedPaletteChoice->SetMaxSize(wxDefaultSize);
#ifdef __WXMSW__
			if (m_attachedPaletteChoice->GetHWND()) {
				::SendMessage(m_attachedPaletteChoice->GetHWND(), CB_SETDROPPEDWIDTH, 0, 0);
			}
#endif
		}
		if (gridToolbar) {
			gridToolbar->SetMaxSize(wxDefaultSize);
		}
	}

	Thaw();
	Layout();
}

void BrushPalettePanel::AttachPaletteChoice(wxChoice* paletteChoice) {
	if (!paletteChoice || m_attachedPaletteChoice == paletteChoice || !choicebook) {
		return;
	}
	m_attachedPaletteChoice = paletteChoice;
	m_attachedPaletteChoice->Reparent(choicebook);

	if (auto* ctrlSizer = dynamic_cast<wxBoxSizer*>(choicebook->GetControlSizer())) {
		ctrlSizer->Insert(0, m_attachedPaletteChoice, wxSizerFlags().Expand().Border(wxBOTTOM, 2));
		ctrlSizer->Layout();
	}
	if (m_isHorizontal) {
		const int leftWidth = FromDIP(PALETTE_HORIZONTAL_LEFT_WIDTH);
		m_attachedPaletteChoice->SetMaxSize(wxSize(leftWidth, -1));
#ifdef __WXMSW__
		if (m_attachedPaletteChoice->GetHWND()) {
			::SendMessage(m_attachedPaletteChoice->GetHWND(), CB_SETDROPPEDWIDTH, leftWidth, 0);
		}
#endif
	}
	choicebook->Layout();
}

void BrushPalettePanel::DetachPaletteChoice() {
	if (!m_attachedPaletteChoice) {
		return;
	}
	m_attachedPaletteChoice->SetMaxSize(wxDefaultSize);
#ifdef __WXMSW__
	if (m_attachedPaletteChoice->GetHWND()) {
		::SendMessage(m_attachedPaletteChoice->GetHWND(), CB_SETDROPPEDWIDTH, 0, 0);
	}
#endif
	if (choicebook) {
		if (auto* ctrlSizer = dynamic_cast<wxBoxSizer*>(choicebook->GetControlSizer())) {
			ctrlSizer->Detach(m_attachedPaletteChoice);
			ctrlSizer->Layout();
		}
	}
	if (auto* parentPalette = GetParentPalette()) {
		if (auto* parentCb = parentPalette->GetChoicebook()) {
			if (m_attachedPaletteChoice->GetParent() != parentCb) {
				m_attachedPaletteChoice->Reparent(parentCb);
			}
		}
	}
	m_attachedPaletteChoice = nullptr;
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
	if (ltype == BRUSHLIST_LISTBOX || ltype == BRUSHLIST_TEXT_LISTBOX) {
		SetDisplayMode(TilesetDisplayMode::List);
		return;
	}
	SetDisplayMode(TilesetDisplayMode::Grid);
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
	if (panel) {
		panel->OnSwitchIn();
		ApplyFilter();
		if (const Brush* current = g_gui.GetCurrentBrush()) {
			panel->SelectBrush(current);
		}
	}

	Layout();
}

void BrushPalettePanel::OnSwitchIn() {
	g_palettes.ActivatePalette(GetParentPalette());
	g_gui.RestoreBrushSizeState(last_brush_size_state);

	if (m_isListMode != s_defaultIsListMode) {
		SetDisplayMode(s_defaultIsListMode ? TilesetDisplayMode::List : TilesetDisplayMode::Grid);
	}
	if (m_showLabels != s_defaultShowLabels) {
		SetShowLabels(s_defaultShowLabels);
	}
	if (m_tileSize != s_defaultTileSize) {
		SetTileSize(s_defaultTileSize);
	}
	if (m_indicatorDogEar != s_defaultIndicatorDogEar) {
		SetBrushIndicators(s_defaultIndicatorDogEar);
	}
	if (m_gridSpacing != s_defaultGridSpacing) {
		SetGridSpacing(s_defaultGridSpacing);
	}
	if (s_defaultHasSort) {
		if (!m_hasSort || m_sortKey != s_defaultSortKey || m_sortDir != s_defaultSortDir) {
			SetSort(s_defaultSortKey, s_defaultSortDir);
		}
	} else if (m_hasSort) {
		ClearSort();
	}

	if (gridToolbar) {
		gridToolbar->SetState(m_showLabels, m_tileSize, m_hasSort, m_sortKey, m_sortDir, m_filterAll, m_isListMode);
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
		gridToolbar->SetState(m_showLabels, m_tileSize, m_hasSort, m_sortKey, m_sortDir, m_filterAll, m_isListMode);
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
	m_isListMode = false;
	s_defaultIsListMode = false;

	g_settings.setInteger(Config::PALETTE_TILE_SIZE, sizePx);
	std::string style = std::to_string(sizePx) + "x" + std::to_string(sizePx) + " px";
	g_settings.setString(Config::PALETTE_DYNAMIC_STYLE, style);
	g_settings.save();

	if (gridToolbar) {
		gridToolbar->SetState(m_showLabels, m_tileSize, m_hasSort, m_sortKey, m_sortDir, m_filterAll, m_isListMode);
	}

	if (!choicebook) {
		return;
	}
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		if (panel) {
			panel->SetDisplayMode(TilesetDisplayMode::Grid);
			panel->SetTileSize(sizePx);
		}
	}
}

void BrushPalettePanel::SetDisplayMode(TilesetDisplayMode mode) {
	m_isListMode = (mode == TilesetDisplayMode::List);
	s_defaultIsListMode = m_isListMode;

	if (m_isListMode) {
		g_settings.setString(Config::PALETTE_DYNAMIC_STYLE, "List style");
	} else {
		std::string style = std::to_string(m_tileSize) + "x" + std::to_string(m_tileSize) + " px";
		g_settings.setString(Config::PALETTE_DYNAMIC_STYLE, style);
	}
	g_settings.save();

	if (gridToolbar) {
		gridToolbar->SetState(m_showLabels, m_tileSize, m_hasSort, m_sortKey, m_sortDir, m_filterAll, m_isListMode);
	}

	if (!choicebook) {
		return;
	}
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		if (panel) {
			panel->SetDisplayMode(mode);
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

bool BrushPalettePanel::SelectTileset(std::string_view tilesetName) {
	if (!choicebook) {
		return false;
	}
	std::string targetLower = as_lower_str(std::string(tilesetName));
	for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		if (as_lower_str(nstr(choicebook->GetPageText(iz))) == targetLower) {
			choicebook->SetSelection(iz);
			LoadCurrentContents();
			Layout();
			Refresh();
			return true;
		}
	}
	return false;
}

void BrushPalettePanel::ReloadPages() {
	if (!choicebook) {
		return;
	}

	wxString currentTilesetName;
	if (choicebook->GetSelection() != wxNOT_FOUND) {
		currentTilesetName = choicebook->GetPageText(choicebook->GetSelection());
	}

	const auto& palettes = g_materials.paletteCatalog().dynamicPalettes();
	auto it = std::ranges::find_if(palettes, [this](const auto& pal) {
		return as_lower_str(pal.name) == as_lower_str(palette_name);
	});
	if (it != palettes.end()) {
		m_paletteDef = &(*it);
	}

	if (!m_paletteDef) {
		return;
	}

	remembered_brushes.clear();
	choicebook->DeleteAllPages();

	for (const auto& tileset : m_paletteDef->tilesets) {
		if (tileset.size() > 0 || tileset.is_import_target) {
			BrushPanel* panel = new BrushPanel(choicebook);
			panel->AssignTileset(&tileset);
			if (m_hasSort) {
				panel->SetSort(m_sortKey, m_sortDir);
			}
			panel->SetShowLabels(m_showLabels);
			panel->SetTileSize(m_tileSize);
			panel->SetDisplayMode(m_isListMode ? TilesetDisplayMode::List : TilesetDisplayMode::Grid);
			panel->SetBrushIndicators(m_indicatorDogEar);
			panel->SetGridSpacing(m_gridSpacing);
			choicebook->AddPage(panel, wxstr(tileset.name));
		}
	}

	if (!currentTilesetName.empty()) {
		std::string curLower = as_lower_str(nstr(currentTilesetName));
		for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
			if (as_lower_str(nstr(choicebook->GetPageText(iz))) == curLower) {
				choicebook->SetSelection(iz);
				break;
			}
		}
	}

	if (choicebook->GetPageCount() > 0 && choicebook->GetSelection() == wxNOT_FOUND) {
		choicebook->SetSelection(0);
	}

	ApplyFilter();
	LoadCurrentContents();
	choicebook->Layout();
	Layout();
	Refresh();
}




void BrushPalettePanel::SetBrushIndicators(bool dogEar) {
	m_indicatorDogEar = dogEar;
	s_defaultIndicatorDogEar = dogEar;
	g_settings.setInteger(Config::PALETTE_INDICATOR_DOGEAR, dogEar ? 1 : 0);
	g_settings.save();

	if (gridToolbar) {
		gridToolbar->SetIndicatorState(dogEar);
	}

	if (choicebook) {
		for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
			BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
			if (panel) {
				panel->SetBrushIndicators(dogEar);
			}
		}
	}
}

void BrushPalettePanel::SetGridSpacing(int spacing) {
	const int validSpacing = (spacing == 2 || spacing == 6) ? spacing : 4;
	m_gridSpacing = validSpacing;
	s_defaultGridSpacing = validSpacing;
	g_settings.setInteger(Config::PALETTE_GRID_SPACING, validSpacing);
	g_settings.save();

	if (gridToolbar) {
		gridToolbar->SetGridSpacing(validSpacing);
	}

	if (choicebook) {
		for (size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
			if (auto* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz))) {
				panel->SetGridSpacing(validSpacing);
			}
		}
	}
}
