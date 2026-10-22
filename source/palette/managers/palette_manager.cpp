//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include <format>
#include "palette/managers/palette_manager.h"
#include "app/managers/version_manager.h"
#include "ui/gui.h"
#include "palette/palette_window.h"
#include "game/materials.h"
#include "brushes/managers/brush_manager.h"

PaletteManager g_palettes;

PaletteManager::PaletteManager() :
	palette_creation_counter(0) {
}

PaletteManager::~PaletteManager() {
	spdlog::info("PaletteManager destructor started");
	spdlog::default_logger()->flush();

	spdlog::info("PaletteManager destructor finished");
	spdlog::default_logger()->flush();
}

PaletteWindow* PaletteManager::GetPalette() {
	if (palettes.empty()) {
		return nullptr;
	}
	return palettes.front();
}

PaletteWindow* PaletteManager::NewPalette() {
	return CreatePalette();
}

void PaletteManager::ActivatePalette(PaletteWindow* p) {
	auto it = std::find(palettes.begin(), palettes.end(), p);
	if (it != palettes.end()) {
		palettes.erase(it);
		palettes.push_front(p);
	}
}

void PaletteManager::RebuildPalettes() {
	std::list<PaletteWindow*> tmp = palettes;
	for (auto& piter : tmp) {
		piter->ReloadSettings(g_gui.IsEditorOpen() ? &g_gui.GetCurrentMap() : nullptr);
	}
	g_gui.aui_manager->Update();
}

void PaletteManager::RefreshPalettes(Map* m, bool usedefault) {
	for (auto& palette : palettes) {
		palette->OnUpdate(m ? m : (usedefault ? (g_gui.IsEditorOpen() ? &g_gui.GetCurrentMap() : nullptr) : nullptr));
	}
	g_brush_manager.SelectBrush();
}

void PaletteManager::RefreshOtherPalettes(PaletteWindow* p) {
	for (auto& palette : palettes) {
		if (palette != p) {
			palette->OnUpdate(g_gui.IsEditorOpen() ? &g_gui.GetCurrentMap() : nullptr);
		}
	}
	g_brush_manager.SelectBrush();
}

void PaletteManager::ShowPalette() {
	if (palettes.empty()) {
		CreatePalette();
	}
	if (palettes.empty()) {
		return;
	}

	for (auto& palette : palettes) {
		if (g_gui.aui_manager->GetPane(palette).IsShown()) {
			return;
		}
	}

	g_gui.aui_manager->GetPane(palettes.front()).Show(true);
	g_gui.aui_manager->Update();
}

void PaletteManager::SelectPalettePage(std::string_view name) {
	if (palettes.empty()) {
		CreatePalette();
	}
	PaletteWindow* p = GetPalette();
	if (!p) {
		return;
	}

	ShowPalette();
	p->SelectPage(name);
	g_gui.aui_manager->Update();
	g_brush_manager.SelectBrushInternal(p->GetSelectedBrush());
}

void PaletteManager::ReloadDynamicPalettes() {
	for (auto* p : palettes) {
		if (p) {
			p->ReloadDynamicPages();
		}
	}
	g_gui.aui_manager->Update();
}

bool PaletteManager::SelectTilesetInPalette(std::string_view paletteName, std::string_view tilesetName) {
	if (palettes.empty()) {
		CreatePalette();
	}
	PaletteWindow* p = GetPalette();
	if (!p) {
		return false;
	}

	ShowPalette();
	p->SelectPage(paletteName);
	bool success = p->SelectTilesetInCurrentPalette(tilesetName);
	g_gui.aui_manager->Update();
	g_brush_manager.SelectBrushInternal(p->GetSelectedBrush());
	return success;
}

void PaletteManager::DestroyPalettes() {
	spdlog::info("PaletteManager::DestroyPalettes called - destroying {} palettes", palettes.size());
	spdlog::default_logger()->flush();
	for (auto palette : palettes) {
		spdlog::info("PaletteManager::DestroyPalettes - detaching and destroying a palette");
		spdlog::default_logger()->flush();
		g_gui.aui_manager->DetachPane(palette);
		palette->Destroy();
	}
	spdlog::info("PaletteManager::DestroyPalettes - clearing palettes list");
	spdlog::default_logger()->flush();
	palettes.clear();

	spdlog::info("PaletteManager::DestroyPalettes - updating aui_manager");
	spdlog::default_logger()->flush();
	g_gui.aui_manager->Update();

	spdlog::info("PaletteManager::DestroyPalettes finished");
	spdlog::default_logger()->flush();
}

PaletteWindow* PaletteManager::CreatePalette() {
	if (!g_version.IsVersionLoaded()) {
		return nullptr;
	}

	auto* palette = newd PaletteWindow(g_gui.root, g_materials.paletteCatalog());

	// The counter alone is not enough: a restored perspective can already own a
	// pane with the generated name (LoadPaneInfo renames panes to the saved
	// name), and wxAuiManager asserts on duplicate pane names, which then
	// blocks the editor. Pick the first free name.
	wxString name;
	do {
		name = wxstr(std::format("Palette_{}", ++palette_creation_counter));
	} while (g_gui.aui_manager->GetPane(name).IsOk());

	g_gui.aui_manager->AddPane(palette, wxAuiPaneInfo().Name(name).Caption("Palette").TopDockable(true).BottomDockable(true));
	g_gui.aui_manager->Update();

	palettes.push_front(palette);
	g_brush_manager.SelectBrushInternal(palette->GetSelectedBrush());
	palette->OnUpdate(g_gui.IsEditorOpen() ? &g_gui.GetCurrentMap() : nullptr);
	return palette;
}
