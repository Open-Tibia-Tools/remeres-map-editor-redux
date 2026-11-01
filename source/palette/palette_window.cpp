#include <wx/aui/framemanager.h>
//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "palette/palette_window.h"

#include "app/settings.h"
#include "brushes/brush.h"
#include "brushes/creature/creature_brush.h"
#include "palette/hardcoded_palette_registry.h"
#include "palette/house/house_palette.h"
#include "palette/palette_waypoints.h"
#include "palette/palette_zones.h"
#include "palette/town/town_palette.h"
#include "brushes/zone/zone_brush.h"
#include "palette/panels/brush_palette_panel.h"
#include "palette/all_items/all_items_palette.h"
#include "ui/managers/layout_manager.h"
#include "rendering/ui/map_display.h"
#include "game/materials.h"
#include "map/map.h"
#include "ui/gui.h"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace {

[[nodiscard]] wxString dynamicBrushListType() {
	return wxstr(g_settings.getString(Config::PALETTE_DYNAMIC_STYLE));
}

[[nodiscard]] bool paletteNameMatches(const PalettePanel* panel, std::string_view name) {
	return panel && as_lower_str(nstr(panel->GetName())) == as_lower_str(std::string(name));
}

[[nodiscard]] wxWindow* choicebookPageAt(wxChoicebook* choicebook, int index) {
	if (!choicebook || index == wxNOT_FOUND || index < 0 || static_cast<size_t>(index) >= choicebook->GetPageCount()) {
		return nullptr;
	}
	return choicebook->GetPage(static_cast<size_t>(index));
}

class MainPaletteChoicebook final : public wxChoicebook {
public:
	MainPaletteChoicebook(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0) :
		wxChoicebook(parent, id, pos, size, style) {
		Bind(wxEVT_SIZE, &MainPaletteChoicebook::OnSize, this);
	}

	void SetHorizontal(bool horizontal) {
		m_horizontal = horizontal;
	}

	wxRect GetPageRect() const override {
		if (m_horizontal) {
			const int sel = const_cast<MainPaletteChoicebook*>(this)->GetSelection();
			if (sel != wxNOT_FOUND && static_cast<size_t>(sel) < GetPageCount()) {
				if (dynamic_cast<const BrushPalettePanel*>(GetPage(static_cast<size_t>(sel)))) {
					return wxRect(wxPoint(0, 0), GetClientSize());
				}
			}
		}
		return wxChoicebook::GetPageRect();
	}

	bool Layout() override {
		if (m_horizontal) {
			const int sel = GetSelection();
			if (sel != wxNOT_FOUND && static_cast<size_t>(sel) < GetPageCount()) {
				if (wxWindow* page = GetPage(static_cast<size_t>(sel))) {
					if (dynamic_cast<BrushPalettePanel*>(page)) {
						page->SetSize(wxRect(wxPoint(0, 0), GetClientSize()));
						page->Layout();
						return true;
					}
				}
			}
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
} // namespace

PaletteWindow::PaletteWindow(wxWindow* parent, const PaletteCatalog& catalog) :
	wxPanel(parent, wxID_ANY, wxDefaultPosition, parent ? parent->FromDIP(wxSize(230, 250)) : wxSize(230, 250)),
	choicebook(nullptr),
	house_palette(nullptr),
	waypoint_palette(nullptr) {
	SetMinSize(FromDIP(wxSize(220, 100)));

	choicebook = new MainPaletteChoicebook(this, PALETTE_CHOICEBOOK, wxDefaultPosition, FromDIP(wxSize(230, 250)));
	m_paletteChoice = choicebook->GetChoiceCtrl();
	m_paletteChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent& event) {
		if (choicebook) {
			const int sel = m_paletteChoice->GetSelection();
			if (sel != wxNOT_FOUND && sel != choicebook->GetSelection()) {
				choicebook->SetSelection(sel);
			}
		}
		event.Skip();
	});

	Bind(wxEVT_SIZE, &PaletteWindow::OnSize, this);
	Bind(wxEVT_CHOICEBOOK_PAGE_CHANGING, &PaletteWindow::OnSwitchingPage, this, PALETTE_CHOICEBOOK);
	Bind(wxEVT_CHOICEBOOK_PAGE_CHANGED, &PaletteWindow::OnPageChanged, this, PALETTE_CHOICEBOOK);
	Bind(wxEVT_CLOSE_WINDOW, &PaletteWindow::OnClose, this);
	Bind(wxEVT_KEY_DOWN, &PaletteWindow::OnKey, this);
	Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) {
		Refresh();
		Update();
		event.Skip();
	});

	for (const auto& palette : catalog.dynamicPalettes()) {
		auto* panel = new BrushPalettePanel(choicebook, palette);
		panel->SetListType(dynamicBrushListType());
		palette_pages.push_back(panel);
		choicebook->AddPage(panel, panel->GetName());
	}

	for (const auto& provider : GetHardcodedPaletteProviders()) {
		PalettePanel* panel = provider.createPanel(choicebook);
		if (!panel) {
			continue;
		}
		palette_pages.push_back(panel);
		hardcoded_pages.push_back(HardcodedPage { .panel = panel, .provider = &provider });
		if (auto* housePanel = dynamic_cast<HousePalette*>(panel)) {
			house_palette = housePanel;
			g_gui.house_palette = house_palette;
		} else if (auto* waypointPanel = dynamic_cast<WaypointPalettePanel*>(panel)) {
			waypoint_palette = waypointPanel;
		} else if (auto* zonePanel = dynamic_cast<ZonePalettePanel*>(panel)) {
			zone_palette = zonePanel;
			g_gui.zone_palette = zone_palette;
		} else if (auto* townPanel = dynamic_cast<TownPalettePanel*>(panel)) {
			town_palette = townPanel;
			g_gui.town_palette = town_palette;
		}
		choicebook->AddPage(panel, wxstr(provider.name));
	}

	wxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(choicebook, wxSizerFlags(1).Expand());
	SetSizer(sizer);

	ReloadSettings(g_gui.IsEditorOpen() ? &g_gui.GetCurrentMap() : nullptr);
	Fit();
}

PaletteWindow::~PaletteWindow() {
	spdlog::info("PaletteWindow destructor started");
	spdlog::default_logger()->flush();

	if (g_gui.house_palette == house_palette) {
		g_gui.house_palette = nullptr;
	}
	if (g_gui.zone_palette == zone_palette) {
		g_gui.zone_palette = nullptr;
	}
	if (g_gui.town_palette == town_palette) {
		g_gui.town_palette = nullptr;
	}

	spdlog::info("PaletteWindow destructor finished");
	spdlog::default_logger()->flush();
}

void PaletteWindow::updateZonePage(Map* map) {
	if (!zone_palette || !choicebook) {
		return;
	}

	const bool supports_zones = map && map->supportsZones();
	int zone_page_idx = wxNOT_FOUND;
	for (size_t i = 0; i < choicebook->GetPageCount(); ++i) {
		if (choicebook->GetPage(i) == zone_palette) {
			zone_page_idx = static_cast<int>(i);
			break;
		}
	}

	if (supports_zones && zone_page_idx == wxNOT_FOUND) {
		choicebook->AddPage(zone_palette, "Zone");
		choicebook->Layout();
	} else if (!supports_zones && zone_page_idx != wxNOT_FOUND) {
		const bool was_selected = choicebook->GetSelection() == zone_page_idx;
		choicebook->RemovePage(static_cast<size_t>(zone_page_idx));
		if (was_selected && choicebook->GetPageCount() > 0) {
			choicebook->SetSelection(0);
		}
		if (const Brush* current_brush = g_gui.GetCurrentBrush(); current_brush && current_brush->is<ZoneBrush>()) {
			g_gui.SelectBrush();
		}
		choicebook->Layout();
	}
}

void PaletteWindow::ReloadSettings(Map* map) {
	for (auto* panel : palette_pages) {
		if (auto* brushPanel = dynamic_cast<BrushPalettePanel*>(panel)) {
			brushPanel->SetListType(dynamicBrushListType());
		}
	}
	for (const auto& hardcodedPage : hardcoded_pages) {
		hardcodedPage.provider->updateMap(hardcodedPage.panel, map);
	}

	updateZonePage(map);

	InvalidateContents();
}

void PaletteWindow::LoadCurrentContents() {
	if (!choicebook) {
		return;
	}
	if (auto* panel = dynamic_cast<PalettePanel*>(choicebook->GetCurrentPage())) {
		panel->LoadCurrentContents();
	}
	Fit();
	Refresh();
	Update();
}

void PaletteWindow::InvalidateContents() {
	if (!choicebook) {
		return;
	}
	for (auto* panel : palette_pages) {
		panel->InvalidateContents();
	}
	LoadCurrentContents();
	for (auto* panel : palette_pages) {
		panel->OnUpdate();
	}
}

void PaletteWindow::ReloadDynamicPages() {
	if (!choicebook) {
		return;
	}

	for (const auto& palette : g_materials.paletteCatalog().dynamicPalettes()) {
		bool found = false;
		for (auto* panel : palette_pages) {
			if (auto* brushPanel = dynamic_cast<BrushPalettePanel*>(panel)) {
				if (as_lower_str(nstr(brushPanel->GetName())) == as_lower_str(palette.name)) {
					found = true;
					break;
				}
			}
		}
		if (!found) {
			auto* panel = new BrushPalettePanel(choicebook, palette);
			panel->SetListType(dynamicBrushListType());
			panel->SetHorizontalLayout(m_isHorizontal);
			palette_pages.push_back(panel);
			choicebook->AddPage(panel, panel->GetName());
		}
	}

	for (auto* panel : palette_pages) {
		if (auto* brushPanel = dynamic_cast<BrushPalettePanel*>(panel)) {
			brushPanel->ReloadPages();
		}
	}
	LoadCurrentContents();
}

void PaletteWindow::SetBrushIndicators(bool dogEar) {
	for (auto* panel : palette_pages) {
		if (auto* bpp = dynamic_cast<BrushPalettePanel*>(panel)) {
			bpp->SetBrushIndicators(dogEar);
		} else if (auto* aip = dynamic_cast<AllItemsPalette*>(panel)) {
			aip->SetBrushIndicators(dogEar);
		}
	}
}

void PaletteWindow::SetGridSpacing(int spacing) {
	for (auto* panel : palette_pages) {
		if (auto* bpp = dynamic_cast<BrushPalettePanel*>(panel)) {
			bpp->SetGridSpacing(spacing);
		} else if (auto* aip = dynamic_cast<AllItemsPalette*>(panel)) {
			aip->SetGridSpacing(spacing);
		}
	}
}

bool PaletteWindow::SelectTilesetInCurrentPalette(std::string_view tilesetName) {
	if (!choicebook) {
		return false;
	}
	if (auto* panel = dynamic_cast<BrushPalettePanel*>(choicebook->GetCurrentPage())) {
		return panel->SelectTileset(tilesetName);
	}
	return false;
}

void PaletteWindow::SelectPage(std::string_view paletteName) {
	if (!choicebook || paletteName.empty()) {
		return;
	}

	if (paletteName == "Zone") {
		updateZonePage(g_gui.IsEditorOpen() ? &g_gui.GetCurrentMap() : nullptr);
	}

	for (size_t index = 0; index < choicebook->GetPageCount(); ++index) {
		auto* panel = dynamic_cast<PalettePanel*>(choicebook->GetPage(index));
		if (paletteNameMatches(panel, paletteName)) {
			choicebook->SetSelection(index);
			return;
		}
	}
}

std::string PaletteWindow::GetSelectedPageName() const {
	if (!choicebook) {
		return "";
	}
	if (auto* panel = dynamic_cast<PalettePanel*>(choicebook->GetCurrentPage())) {
		return nstr(panel->GetName());
	}
	const int sel = choicebook->GetSelection();
	if (sel != wxNOT_FOUND) {
		return nstr(choicebook->GetPageText(static_cast<size_t>(sel)));
	}
	return "";
}

Brush* PaletteWindow::GetSelectedBrush() const {
	if (!choicebook) {
		return nullptr;
	}
	auto* panel = dynamic_cast<PalettePanel*>(choicebook->GetCurrentPage());
	return panel ? panel->GetSelectedBrush() : nullptr;
}

Brush* PaletteWindow::GetSelectedCreatureBrush() const {
	Brush* brush = GetSelectedBrush();
	return brush && brush->is<CreatureBrush>() ? brush : nullptr;
}

int PaletteWindow::GetSelectedBrushSize() const {
	if (!choicebook) {
		return 0;
	}
	auto* panel = dynamic_cast<PalettePanel*>(choicebook->GetCurrentPage());
	return panel ? panel->GetSelectedBrushSize() : 0;
}

bool PaletteWindow::OnSelectBrush(const Brush* whatbrush) {
	if (!choicebook || !whatbrush) {
		return false;
	}

	for (size_t index = 0; index < choicebook->GetPageCount(); ++index) {
		auto* panel = dynamic_cast<PalettePanel*>(choicebook->GetPage(index));
		if (panel && panel->SelectBrush(whatbrush)) {
			choicebook->SetSelection(index);
			return true;
		}
	}

	return false;
}

bool PaletteWindow::JumpToBrush(const Brush* brush, std::string_view preferredPalette) {
	if (!brush || !choicebook) {
		return false;
	}

	const DynamicPaletteDefinition* targetPalette = nullptr;
	const DynamicTilesetDefinition* targetTileset = nullptr;

	const auto& palettes = g_materials.paletteCatalog().dynamicPalettes();

	if (!preferredPalette.empty()) {
		for (const auto& pal : palettes) {
			if (pal.name == preferredPalette) {
				for (const auto& ts : pal.tilesets) {
					if (ts.containsBrush(brush)) {
						targetPalette = &pal;
						targetTileset = &ts;
						break;
					}
				}
				break;
			}
		}
	}

	if (!targetPalette) {
		for (const auto& pal : palettes) {
			for (const auto& ts : pal.tilesets) {
				if (ts.containsBrush(brush)) {
					targetPalette = &pal;
					targetTileset = &ts;
					break;
				}
			}
			if (targetPalette) {
				break;
			}
		}
	}

	if (!targetPalette || !targetTileset) {
		return OnSelectBrush(brush);
	}

	int targetPageIndex = wxNOT_FOUND;
	BrushPalettePanel* targetBrushPalettePanel = nullptr;

	for (size_t i = 0; i < choicebook->GetPageCount(); ++i) {
		auto* p = dynamic_cast<BrushPalettePanel*>(choicebook->GetPage(i));
		if (p && p->GetName() == targetPalette->name) {
			targetPageIndex = static_cast<int>(i);
			targetBrushPalettePanel = p;
			break;
		}
	}

	if (!targetBrushPalettePanel || targetPageIndex == wxNOT_FOUND) {
		return false;
	}

	auto* currentPanel = dynamic_cast<BrushPalettePanel*>(choicebook->GetCurrentPage());
	if (currentPanel && currentPanel != targetBrushPalettePanel) {
		currentPanel->ResetFilter();
	}

	if (choicebook->GetSelection() != targetPageIndex) {
		choicebook->SetSelection(targetPageIndex);
	}

	return targetBrushPalettePanel->JumpToTilesetAndBrush(targetTileset->name, brush);
}

void PaletteWindow::OnSize(wxSizeEvent& event) {
	event.Skip();
	const wxSize sz = event.GetSize();
	if (sz.x <= 0 || sz.y <= 0) {
		return;
	}

	bool shouldBeHorizontal = false;
	if (g_gui.aui_manager) {
		const wxAuiPaneInfo& pane = g_gui.aui_manager->GetPane(this);
		if (pane.IsOk() && pane.IsDocked()) {
			if (pane.dock_direction == wxAUI_DOCK_LEFT || pane.dock_direction == wxAUI_DOCK_RIGHT) {
				shouldBeHorizontal = false;
			} else if (pane.dock_direction == wxAUI_DOCK_BOTTOM || pane.dock_direction == wxAUI_DOCK_TOP) {
				shouldBeHorizontal = (sz.x > FromDIP(450));
			}
		} else {
			shouldBeHorizontal = (sz.x > sz.y * 1.25f && sz.x > FromDIP(500));
		}
	} else {
		shouldBeHorizontal = (sz.x > sz.y * 1.25f && sz.x > FromDIP(500));
	}

	if (shouldBeHorizontal != m_isHorizontal) {
		SetHorizontalLayout(shouldBeHorizontal);
	}
}

void PaletteWindow::DetachPaletteChoiceFromAnySizer() {
	if (!m_paletteChoice) {
		return;
	}
	for (auto* page : palette_pages) {
		if (auto* brushPalette = dynamic_cast<BrushPalettePanel*>(page)) {
			brushPalette->DetachPaletteChoice();
		}
	}
	if (choicebook) {
		if (auto* ctrlSizer = choicebook->GetControlSizer()) {
			ctrlSizer->Detach(m_paletteChoice);
		}
	}
	if (auto* parent = m_paletteChoice->GetParent()) {
		if (auto* sizer = parent->GetSizer()) {
			sizer->Detach(m_paletteChoice);
		}
	}
}

void PaletteWindow::SetHorizontalLayout(bool horizontal) {
	if (m_isHorizontal == horizontal) {
		return;
	}
	m_isHorizontal = horizontal;

	Freeze();
	if (choicebook) {
		if (auto* mainCb = dynamic_cast<MainPaletteChoicebook*>(choicebook)) {
			mainCb->SetHorizontal(horizontal);
		}
	}

	for (auto* page : palette_pages) {
		if (auto* brushPalette = dynamic_cast<BrushPalettePanel*>(page)) {
			brushPalette->SetHorizontalLayout(horizontal);
		}
	}

	DetachPaletteChoiceFromAnySizer();

	if (horizontal) {
		if (auto* currentBrushPanel = dynamic_cast<BrushPalettePanel*>(choicebook->GetCurrentPage())) {
			currentBrushPanel->AttachPaletteChoice(m_paletteChoice);
		} else {
			if (m_paletteChoice) {
				m_paletteChoice->Reparent(choicebook);
				if (auto* ctrlSizer = dynamic_cast<wxBoxSizer*>(choicebook->GetControlSizer())) {
					ctrlSizer->Add(m_paletteChoice, wxSizerFlags(1).Expand());
				}
			}
		}
		if (m_paletteChoice) {
			const int leftWidth = FromDIP(PALETTE_HORIZONTAL_LEFT_WIDTH);
			m_paletteChoice->SetMaxSize(wxSize(leftWidth, -1));
#ifdef __WXMSW__
			if (m_paletteChoice->GetHWND()) {
				::SendMessage(m_paletteChoice->GetHWND(), CB_SETDROPPEDWIDTH, leftWidth, 0);
			}
#endif
		}
	} else {
		if (m_paletteChoice) {
			m_paletteChoice->Reparent(choicebook);
			m_paletteChoice->SetMaxSize(wxDefaultSize);
#ifdef __WXMSW__
			if (m_paletteChoice->GetHWND()) {
				::SendMessage(m_paletteChoice->GetHWND(), CB_SETDROPPEDWIDTH, 0, 0);
			}
#endif
		}
		if (auto* ctrlSizer = dynamic_cast<wxBoxSizer*>(choicebook->GetControlSizer())) {
			ctrlSizer->Add(m_paletteChoice, wxSizerFlags(1).Expand());
			ctrlSizer->Layout();
		}
	}

	choicebook->Layout();
	Thaw();
	Layout();
}

void PaletteWindow::OnSwitchingPage(wxChoicebookEvent& event) {
	event.Skip();
	if (!choicebook) {
		return;
	}

	if (m_isHorizontal) {
		DetachPaletteChoiceFromAnySizer();
	}

	if (wxWindow* oldPage = choicebookPageAt(choicebook, event.GetOldSelection())) {
		if (auto* oldPanel = dynamic_cast<PalettePanel*>(oldPage)) {
			oldPanel->OnSwitchOut();
		}
	}

	if (wxWindow* page = choicebookPageAt(choicebook, event.GetSelection())) {
		if (auto* panel = dynamic_cast<PalettePanel*>(page)) {
			panel->OnSwitchIn();
		}
	}
}

void PaletteWindow::OnPageChanged(wxChoicebookEvent&) {
	if (!choicebook) {
		return;
	}

	if (m_isHorizontal) {
		DetachPaletteChoiceFromAnySizer();
		if (auto* newBrushPanel = dynamic_cast<BrushPalettePanel*>(choicebook->GetCurrentPage())) {
			newBrushPanel->AttachPaletteChoice(m_paletteChoice);
		} else {
			if (m_paletteChoice) {
				m_paletteChoice->Reparent(choicebook);
				if (auto* ctrlSizer = dynamic_cast<wxBoxSizer*>(choicebook->GetControlSizer())) {
					ctrlSizer->Add(m_paletteChoice, wxSizerFlags(1).Expand());
				}
			}
		}
		if (m_paletteChoice && choicebook->GetSelection() != wxNOT_FOUND) {
			m_paletteChoice->SetSelection(choicebook->GetSelection());
		}
		choicebook->Layout();
	}

	g_gui.ActivatePalette(this);
	g_gui.SelectBrush();
	const std::string page_name = GetSelectedPageName();
	if (!page_name.empty()) {
		LayoutManager::UpdatePaletteActivePage(this, page_name);
	}
}

void PaletteWindow::OnUpdateBrushSize(BrushShape shape, int size) {
	if (!choicebook) {
		return;
	}
	if (auto* page = dynamic_cast<PalettePanel*>(choicebook->GetCurrentPage())) {
		page->OnUpdateBrushSize(shape, size);
	}
}

void PaletteWindow::OnUpdate(Map* map) {
	updateZonePage(map);
	for (const auto& hardcodedPage : hardcoded_pages) {
		hardcodedPage.provider->updateMap(hardcodedPage.panel, map);
	}
	for (auto* panel : palette_pages) {
		panel->OnUpdate();
	}
}

void PaletteWindow::OnKey(wxKeyEvent& event) {
	if (g_gui.GetCurrentTab() != nullptr) {
		g_gui.GetCurrentMapTab()->GetEventHandler()->QueueEvent(event.Clone());
	}
}

void PaletteWindow::OnClose(wxCloseEvent& event) {
	spdlog::info("PaletteWindow::OnClose called");
	spdlog::default_logger()->flush();
	if (!event.CanVeto()) {
		spdlog::info("PaletteWindow::OnClose - cannot veto, calling Destroy()");
		spdlog::default_logger()->flush();
		Destroy();
	} else {
		spdlog::info("PaletteWindow::OnClose - vetoing close, hiding window instead");
		spdlog::default_logger()->flush();
		Show(false);
		event.Veto(true);
	}
}
