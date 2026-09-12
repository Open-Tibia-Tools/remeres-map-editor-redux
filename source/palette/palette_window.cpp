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
#include "palette/panels/brush_palette_panel.h"
#include "rendering/ui/map_display.h"
#include "game/materials.h"
#include "ui/gui.h"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace {

[[nodiscard]] wxString dynamicBrushListType() {
	return wxstr(g_settings.getString(Config::PALETTE_DYNAMIC_STYLE));
}

[[nodiscard]] bool paletteNameMatches(const PalettePanel* panel, std::string_view name) {
	return panel && nstr(panel->GetName()) == name;
}

[[nodiscard]] wxWindow* choicebookPageAt(wxChoicebook* choicebook, int index) {
	if (!choicebook || index == wxNOT_FOUND || index < 0 || static_cast<size_t>(index) >= choicebook->GetPageCount()) {
		return nullptr;
	}
	return choicebook->GetPage(static_cast<size_t>(index));
}

} // namespace

PaletteWindow::PaletteWindow(wxWindow* parent, const PaletteCatalog& catalog) :
	wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(230, 250)),
	choicebook(nullptr),
	house_palette(nullptr),
	waypoint_palette(nullptr) {
	SetMinSize(wxSize(225, 250));

	choicebook = newd wxChoicebook(this, PALETTE_CHOICEBOOK, wxDefaultPosition, wxSize(230, 250));

	Bind(wxEVT_CHOICEBOOK_PAGE_CHANGING, &PaletteWindow::OnSwitchingPage, this, PALETTE_CHOICEBOOK);
	Bind(wxEVT_CHOICEBOOK_PAGE_CHANGED, &PaletteWindow::OnPageChanged, this, PALETTE_CHOICEBOOK);
	Bind(wxEVT_CLOSE_WINDOW, &PaletteWindow::OnClose, this);
	Bind(wxEVT_KEY_DOWN, &PaletteWindow::OnKey, this);

	for (const auto& palette : catalog.dynamicPalettes()) {
		auto* panel = newd BrushPalettePanel(choicebook, palette);
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
		}
		choicebook->AddPage(panel, wxstr(provider.name));
	}

	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);
	choicebook->SetMinSize(wxSize(225, 300));
	sizer->Add(choicebook, 1, wxEXPAND);
	SetSizer(sizer);

	LoadCurrentContents();
	Fit();
}

PaletteWindow::~PaletteWindow() {
	spdlog::info("PaletteWindow destructor started");
	spdlog::default_logger()->flush();

	if (g_gui.house_palette == house_palette) {
		g_gui.house_palette = nullptr;
	}

	spdlog::info("PaletteWindow destructor finished");
	spdlog::default_logger()->flush();
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

void PaletteWindow::SelectPage(std::string_view paletteName) {
	if (!choicebook || paletteName.empty()) {
		return;
	}

	for (size_t index = 0; index < choicebook->GetPageCount(); ++index) {
		auto* panel = dynamic_cast<PalettePanel*>(choicebook->GetPage(index));
		if (paletteNameMatches(panel, paletteName)) {
			choicebook->SetSelection(index);
			return;
		}
	}
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

void PaletteWindow::OnSwitchingPage(wxChoicebookEvent& event) {
	event.Skip();
	if (!choicebook) {
		return;
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
	g_gui.ActivatePalette(this);
	g_gui.SelectBrush();
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
	for (const auto& hardcodedPage : hardcoded_pages) {
		hardcodedPage.provider->updateMap(hardcodedPage.panel, map);
	}
	for (auto* panel : palette_pages) {
		panel->OnUpdate();
	}
}

void PaletteWindow::OnKey(wxKeyEvent& event) {
	if (g_gui.GetCurrentTab() != nullptr) {
		g_gui.GetCurrentMapTab()->GetEventHandler()->AddPendingEvent(event);
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
