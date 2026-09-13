//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include <wx/wfstream.h>
#include <spdlog/spdlog.h>
#include <wx/display.h>
#include <format>

#include "ui/gui.h"
#include "util/file_system.h"
#include "ui/main_menubar.h"
#include "ui/dialog_util.h"

#include "editor/editor.h"
#include "editor/action_queue.h"
#include "editor/editor_factory.h"
#include "ui/managers/minimap_manager.h"
#include "brushes/managers/doodad_preview_manager.h"
#include "ui/managers/status_manager.h"
#include "brushes/brush.h"
#include "map/map.h"
#include "game/sprites.h"
#include "game/materials.h"
#include "brushes/doodad/doodad_brush.h"
#include "lua/lua_script_manager.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/spawn/spawn_brush.h"

#include "ui/controls/item_buttons.h"
#include "ui/result_window.h"
#include "rendering/ui/minimap_window.h"
#include "palette/palette_window.h"
#include "palette/house/house_palette.h"
#include "rendering/ui/map_display.h"
#include "app/application.h"
#include "ui/welcome_dialog.h"
#include "ui/tool_options_window.h"

#include "live/live_client.h"
#include "live/live_tab.h"
#include "live/live_server.h"

#ifdef __WXOSX__
	#include <AGL/agl.h>
#endif

wxDEFINE_EVENT(EVT_UPDATE_MENUS, wxCommandEvent);

// Global GUI instance
GUI g_gui;

namespace {
void emitBrushChangeIfNeeded(GUI& gui) {
	static const Brush* lastBrush = nullptr;
	static std::string lastBrushName;

	Brush* currentBrush = gui.GetCurrentBrush();
	const bool hasBrush = currentBrush != nullptr;
	const std::string currentName = hasBrush ? currentBrush->getName() : std::string();
	if (currentBrush != lastBrush || currentName != lastBrushName) {
		lastBrush = currentBrush;
		lastBrushName = currentName;
		if (g_luaScripts.isInitialized()) {
			g_luaScripts.emit("brushChange", currentName);
		}
	}
}

[[nodiscard]] bool isCreatureToolBrush(const Brush* brush) {
	return brush && (brush->is<CreatureBrush>() || brush->is<SpawnBrush>());
}

void syncCreatureToolBrushSizeSetting(const GUI& gui) {
	if (!isCreatureToolBrush(gui.GetCurrentBrush())) {
		return;
	}

	g_settings.setInteger(Config::CURRENT_SPAWN_RADIUS, std::max(1, gui.GetBrushSize()));
}
}

// GUI class implementation
GUI::GUI() :
	aui_manager(nullptr),
	tabbook(nullptr),
	root(nullptr),
	house_palette(nullptr),
	tool_options(nullptr),
	tile_properties_panel(nullptr),
	pasting(false),
	disabled_counter(0),
	hotkeys_enabled(true) {
}

GUI::~GUI() {
	spdlog::info("GUI destructor started");
	spdlog::default_logger()->flush();

	// aui_manager and tabbook are owned by MainFrame, we don't delete them here.
	spdlog::info("GUI destructor finished");
	spdlog::default_logger()->flush();
}

// OpenGL context management moved to GLContextManager

void GUI::AddPendingCanvasEvent(wxEvent& event) {
	MapTab* mapTab = GetCurrentMapTab();
	if (mapTab) {
		mapTab->GetCanvas()->GetEventHandler()->AddPendingEvent(event);
	}
}

void GUI::UpdateMenus() {
	wxCommandEvent evt(EVT_UPDATE_MENUS);
	g_gui.root->AddPendingEvent(evt);
}

void GUI::ShowToolbar(ToolBarID id, bool show) {
	if (root && root->GetAuiToolBar()) {
		root->GetAuiToolBar()->Show(id, show);
	}
}

bool GUI::IsSelectionMode() const {
	MapTab* mapTab = GetCurrentMapTab();
	return mapTab ? mapTab->GetMode() == SELECTION_MODE : true;
}

bool GUI::IsDrawingMode() const {
	MapTab* mapTab = GetCurrentMapTab();
	return mapTab ? mapTab->GetMode() == DRAWING_MODE : false;
}

void GUI::SwitchMode() {
	MapTab* mapTab = GetCurrentMapTab();
	if (mapTab) {
		if (mapTab->GetMode() == DRAWING_MODE) {
			SetSelectionMode();
		} else {
			SetDrawingMode();
		}
	}
}

void GUI::SetSelectionMode() {
	MapTab* mapTab = GetCurrentMapTab();
	if (!mapTab || mapTab->GetMode() == SELECTION_MODE) {
		return;
	}

	if (GetCurrentBrush() && GetCurrentBrush()->is<DoodadBrush>()) {
		SetCurrentMapSecondaryMap(nullptr);
	}

	mapTab->OnSwitchEditorMode(SELECTION_MODE);
	SyncCurrentMapCanvasPreviewState();
}

void GUI::SetDrawingMode() {
	MapTab* mapTab = GetCurrentMapTab();
	if (!mapTab || mapTab->GetMode() == DRAWING_MODE) {
		return;
	}

	mapTab->OnSwitchEditorMode(DRAWING_MODE);

	if (GetCurrentBrush() && GetCurrentBrush()->is<DoodadBrush>()) {
		SetCurrentMapSecondaryMap(g_doodad_preview.GetBufferMap());
	} else if (GetCurrentBrush() && GetCurrentBrush()->needBorders() && g_settings.getInteger(Config::USE_AUTOMAGIC)) {
		// We'll set the map, but it might be empty until first mouse move
		SetCurrentMapSecondaryMap(g_autoborder_preview.GetBufferMap());
	} else {
		SetCurrentMapSecondaryMap(nullptr);
	}

	SyncCurrentMapCanvasPreviewState();
}

void GUI::SetCurrentMapSecondaryMap(BaseMap* secondary_map) {
	MapTab* mapTab = GetCurrentMapTab();
	if (!mapTab) {
		return;
	}

	mapTab->GetSession()->secondary_map = secondary_map;
	SyncCurrentMapCanvasPreviewState();
}

void GUI::RefreshView(bool immediate) {
	MapTab* current_map_tab = GetCurrentMapTab();
	for (int i = 0; i < tabbook->GetTabCount(); ++i) {
		auto* editor_tab = tabbook->GetTab(i);
		auto* map_tab = dynamic_cast<MapTab*>(editor_tab);
		if (!editor_tab) {
			continue;
		}

		if (!current_map_tab || !map_tab || map_tab->HasSameReference(current_map_tab)) {
			if (map_tab) {
				map_tab->GetCanvas()->RequestLocalRefresh(immediate);
			} else {
				editor_tab->GetWindow()->Refresh();
			}
		}
	}
}

void GUI::SyncCurrentMapCanvasPreviewState() {
	if (!tabbook) {
		return;
	}

	for (int index = 0; index < tabbook->GetTabCount(); ++index) {
		auto* map_tab = dynamic_cast<MapTab*>(tabbook->GetTab(index));
		if (!map_tab) {
			continue;
		}

		const bool hover_preview_active = IsPasting()
			|| (map_tab->GetMode() == DRAWING_MODE && GetCurrentBrush() != nullptr)
			|| map_tab->GetSession()->secondary_map != nullptr;
		map_tab->GetCanvas()->SetHoverPreviewActive(hover_preview_active);
	}
}

// Welcome Dialog moved to WelcomeManager

void GUI::UpdateMenubar() {
	root->UpdateMenubar();
}

void GUI::SetScreenCenterPosition(Position position) {
	MapTab* mapTab = GetCurrentMapTab();
	if (mapTab) {
		mapTab->SetScreenCenterPosition(position);
	}
}

void GUI::DoCut() {
	g_editors.DoCut();
}
void GUI::DoCopy() {
	g_editors.DoCopy();
}
void GUI::DoPaste() {
	g_editors.DoPaste();
}
void GUI::PreparePaste() {
	g_editors.PreparePaste();
}
void GUI::StartPasting() {
	g_editors.StartPasting();
	SyncCurrentMapCanvasPreviewState();
}
void GUI::EndPasting() {
	g_editors.EndPasting();
	SyncCurrentMapCanvasPreviewState();
}

bool GUI::CanUndo() {
	return g_editors.CanUndo();
}
bool GUI::CanRedo() {
	return g_editors.CanRedo();
}
bool GUI::DoUndo() {
	return g_editors.DoUndo();
}
bool GUI::DoRedo() {
	return g_editors.DoRedo();
}

int GUI::GetCurrentFloor() {
	MapTab* tab = GetCurrentMapTab();
	ASSERT(tab);
	return tab->GetCanvas()->GetFloor();
}

void GUI::ChangeFloor(int new_floor) {
	MapTab* tab = GetCurrentMapTab();
	if (tab) {
		int old_floor = GetCurrentFloor();
		if (new_floor < 0 || new_floor > MAP_MAX_LAYER) {
			return;
		}

		if (old_floor != new_floor) {
			tab->GetCanvas()->ChangeFloor(new_floor);
			g_status.SetStatusText(std::format("Floor: {} | Zoom: {:.0f}%", new_floor, GetCurrentZoom() * 100), STATUS_FIELD_FLOOR_ZOOM);
		}
	}
}

double GUI::GetCurrentZoom() {
	MapTab* mapTab = GetCurrentMapTab();
	if (mapTab) {
		return mapTab->GetCanvas()->GetZoom();
	}
	return 1.0;
}

void GUI::SetCurrentZoom(double zoom) {
	MapTab* mapTab = GetCurrentMapTab();
	if (mapTab) {
		mapTab->GetCanvas()->SetZoom(zoom);
		g_status.SetStatusText(std::format("Floor: {} | Zoom: {:.0f}%", GetCurrentFloor(), zoom * 100), STATUS_FIELD_FLOOR_ZOOM);
	}
}

// Search Results moved to SearchManager

void GUI::FitViewToMap() {
	for (int index = 0; index < tabbook->GetTabCount(); ++index) {
		if (auto* tab = dynamic_cast<MapTab*>(tabbook->GetTab(index))) {
			tab->GetView()->FitToMap();
		}
	}
}

void GUI::FitViewToMap(MapTab* mt) {
	for (int index = 0; index < tabbook->GetTabCount(); ++index) {
		if (auto* tab = dynamic_cast<MapTab*>(tabbook->GetTab(index))) {
			if (tab->HasSameReference(mt)) {
				tab->GetView()->FitToMap();
			}
		}
	}
}

void GUI::FillDoodadPreviewBuffer() {
	g_brush_manager.FillDoodadPreviewBuffer();
	SyncCurrentMapCanvasPreviewState();
}

void GUI::SelectBrush() {
	g_brush_manager.SelectBrush();
	if (tool_options) {
		tool_options->SetActiveBrush(GetCurrentBrush());
	}
	SyncCurrentMapCanvasPreviewState();
	emitBrushChangeIfNeeded(*this);
}
bool GUI::SelectBrush(const Brush* brush) {
	const bool changed = g_brush_manager.SelectBrush(brush);
	if (tool_options) {
		tool_options->SetActiveBrush(GetCurrentBrush());
	}
	SyncCurrentMapCanvasPreviewState();
	emitBrushChangeIfNeeded(*this);
	return changed;
}
void GUI::SelectPreviousBrush() {
	g_brush_manager.SelectPreviousBrush();
	if (tool_options) {
		tool_options->SetActiveBrush(GetCurrentBrush());
	}
	SyncCurrentMapCanvasPreviewState();
	emitBrushChangeIfNeeded(*this);
}
void GUI::SelectBrushInternal(Brush* brush) {
	g_brush_manager.SelectBrushInternal(brush);
	if (tool_options) {
		tool_options->SetActiveBrush(GetCurrentBrush());
	}
	SyncCurrentMapCanvasPreviewState();
	emitBrushChangeIfNeeded(*this);
}
Brush* GUI::GetCurrentBrush() const {
	return g_brush_manager.GetCurrentBrush();
}
BrushShape GUI::GetBrushShape() const {
	return g_brush_manager.GetBrushShape();
}
int GUI::GetBrushSize() const {
	return g_brush_manager.GetBrushSize();
}
int GUI::GetBrushSizeX() const {
	return g_brush_manager.GetBrushSizeX();
}
int GUI::GetBrushSizeY() const {
	return g_brush_manager.GetBrushSizeY();
}
bool GUI::IsExactBrushSize() const {
	return g_brush_manager.IsExactBrushSize();
}
bool GUI::IsBrushAspectRatioLocked() const {
	return g_brush_manager.IsBrushAspectRatioLocked();
}
BrushSizeState GUI::GetBrushSizeState() const {
	return g_brush_manager.GetBrushSizeState();
}
BrushFootprint GUI::GetBrushFootprint() const {
	return g_brush_manager.GetBrushFootprint();
}
int GUI::GetBrushVariation() const {
	return g_brush_manager.GetBrushVariation();
}
int GUI::GetSpawnTime() const {
	return g_brush_manager.GetSpawnTime();
}
void GUI::SetSpawnTime(int time) {
	g_brush_manager.SetSpawnTime(time);
	g_settings.setInteger(Config::DEFAULT_SPAWNTIME, time);
	if (tool_options) {
		tool_options->ReloadSettings();
	}
}
void GUI::SetLightIntensity(int v) {
	g_brush_manager.SetLightIntensity(std::clamp(v, 0, 255));
	g_settings.notifyObservers(Config::SHOW_LIGHTS);
}
int GUI::GetLightIntensity() const {
	return g_brush_manager.GetLightIntensity();
}
void GUI::SetAmbientLightLevel(float v) {
	g_brush_manager.SetAmbientLightLevel(std::clamp(v, 0.0f, 1.0f));
	g_settings.notifyObservers(Config::SHOW_LIGHTS);
}
float GUI::GetAmbientLightLevel() const {
	return g_brush_manager.GetAmbientLightLevel();
}
void GUI::SetServerLightColor(int v) {
	g_brush_manager.SetServerLightColor(std::clamp(v, 0, 255));
	g_settings.notifyObservers(Config::SHOW_LIGHTS);
}
int GUI::GetServerLightColor() const {
	return g_brush_manager.GetServerLightColor();
}
void GUI::SetBrushSize(int nz) {
	g_brush_manager.SetBrushSize(nz);
	syncCreatureToolBrushSizeSetting(*this);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), nz);
	}
}
void GUI::SetBrushSizeInternal(int nz) {
	g_brush_manager.SetBrushSizeInternal(nz);
	syncCreatureToolBrushSizeSetting(*this);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), nz);
	}
}
void GUI::SetBrushSizeX(int nz) {
	g_brush_manager.SetBrushSizeX(nz);
	syncCreatureToolBrushSizeSetting(*this);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), GetBrushSize());
	}
}
void GUI::SetBrushSizeY(int nz) {
	g_brush_manager.SetBrushSizeY(nz);
	syncCreatureToolBrushSizeSetting(*this);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), GetBrushSize());
	}
}
void GUI::SetBrushSizeAxes(int x, int y) {
	g_brush_manager.SetBrushSizeAxes(x, y);
	syncCreatureToolBrushSizeSetting(*this);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), GetBrushSize());
	}
}
void GUI::SetExactBrushSize(bool exact) {
	g_brush_manager.SetExactBrushSize(exact);
	syncCreatureToolBrushSizeSetting(*this);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), GetBrushSize());
	}
}
void GUI::SetBrushAspectRatioLocked(bool locked) {
	g_brush_manager.SetBrushAspectRatioLocked(locked);
	syncCreatureToolBrushSizeSetting(*this);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), GetBrushSize());
	}
}
void GUI::RestoreBrushSizeState(const BrushSizeState& state) {
	g_brush_manager.RestoreBrushSizeState(state);
	syncCreatureToolBrushSizeSetting(*this);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), GetBrushSize());
	}
}
void GUI::SetBrushShape(BrushShape bs) {
	g_brush_manager.SetBrushShape(bs);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), GetBrushSize());
	}
}
void GUI::SetBrushVariation(int nz) {
	g_brush_manager.SetBrushVariation(nz);
}
void GUI::SetBrushThickness(int low, int ceil) {
	g_brush_manager.SetBrushThickness(low, ceil);
}
void GUI::SetBrushThickness(bool on, int low, int ceil) {
	g_brush_manager.SetBrushThickness(on, low, ceil);
}
void GUI::DecreaseBrushSize(bool wrap) {
	g_brush_manager.DecreaseBrushSize(wrap);
	syncCreatureToolBrushSizeSetting(*this);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), GetBrushSize());
	}
}
void GUI::IncreaseBrushSize(bool wrap) {
	g_brush_manager.IncreaseBrushSize(wrap);
	syncCreatureToolBrushSizeSetting(*this);
	if (tool_options) {
		tool_options->UpdateBrushSize(GetBrushShape(), GetBrushSize());
	}
}
void GUI::SetDoorLocked(bool on) {
	g_brush_manager.SetDoorLocked(on);
}
bool GUI::HasDoorLocked() {
	return g_brush_manager.HasDoorLocked();
}

EditorTab* GUI::GetCurrentTab() {
	return g_editors.GetCurrentTab();
}
EditorTab* GUI::GetTab(int idx) {
	return g_editors.GetTab(idx);
}
int GUI::GetTabCount() const {
	return g_editors.GetTabCount();
}
bool GUI::IsAnyEditorOpen() const {
	return g_editors.IsAnyEditorOpen();
}
bool GUI::IsEditorOpen() const {
	return g_editors.IsEditorOpen();
}
void GUI::CloseCurrentEditor() {
	g_editors.CloseCurrentEditor();
}
Editor* GUI::GetCurrentEditor() {
	return g_editors.GetCurrentEditor();
}
MapTab* GUI::GetCurrentMapTab() const {
	return g_editors.GetCurrentMapTab();
}
void GUI::CycleTab(bool forward) {
	g_editors.CycleTab(forward);
}
bool GUI::CloseLiveEditors(LiveSocket* sock) {
	return g_editors.CloseLiveEditors(sock);
}
bool GUI::CloseAllEditors() {
	return g_editors.CloseAllEditors();
}
void GUI::NewMapView() {
	g_editors.NewMapView();
}

void GUI::AddPendingLiveClient(std::unique_ptr<LiveClient> client) {
	std::lock_guard<std::mutex> lock(pending_live_clients_mutex);
	pending_live_clients.push_back(std::move(client));
}

std::unique_ptr<LiveClient> GUI::PopPendingLiveClient(LiveClient* ptr) {
	std::lock_guard<std::mutex> lock(pending_live_clients_mutex);
	auto it = std::find_if(pending_live_clients.begin(), pending_live_clients.end(), [ptr](const std::unique_ptr<LiveClient>& c) { return c.get() == ptr; });

	if (it != pending_live_clients.end()) {
		std::unique_ptr<LiveClient> client = std::move(*it);
		pending_live_clients.erase(it);
		return client;
	}
	return nullptr;
}

bool GUI::NewMap() {
	return g_editors.NewMap();
}
void GUI::OpenMap() {
	g_editors.OpenMap();
}
void GUI::SaveMap() {
	g_editors.SaveMap();
}
void GUI::SaveMapAs() {
	g_editors.SaveMapAs();
}
bool GUI::LoadMap(const FileName& fileName, const MapLoadOptions& load_options) {
	return g_editors.LoadMap(fileName, load_options);
}

Map& GUI::GetCurrentMap() {
	return g_editors.GetCurrentMap();
}
int GUI::GetOpenMapCount() {
	return g_editors.GetOpenMapCount();
}
bool GUI::ShouldSave() {
	return g_editors.ShouldSave();
}
void GUI::SaveCurrentMap(FileName filename, bool showdialog) {
	g_editors.SaveCurrentMap(filename, showdialog);
}
void GUI::SaveCurrentMap(bool showdialog) {
	g_editors.SaveCurrentMap(FileName(""), showdialog);
}

PaletteWindow* GUI::NewPalette() {
	return g_palettes.NewPalette();
}
void GUI::ActivatePalette(PaletteWindow* p) {
	g_palettes.ActivatePalette(p);
	if (p && tool_options) {
		tool_options->SetActiveBrush(GetCurrentBrush());
	}
}
void GUI::RebuildPalettes() {
	g_palettes.RebuildPalettes();
}
void GUI::RefreshPalettes(Map* m, bool usedfault) {
	g_palettes.RefreshPalettes(m, usedfault);
	if (house_palette) {
		house_palette->SetMap(m ? m : (usedfault ? (IsEditorOpen() ? &GetCurrentMap() : nullptr) : nullptr));
	}
}
void GUI::RefreshOtherPalettes(PaletteWindow* p) {
	g_palettes.RefreshOtherPalettes(p);
}
void GUI::ShowPalette() {
	g_palettes.ShowPalette();
}
void GUI::SelectPalettePage(std::string_view name) {
	g_palettes.SelectPalettePage(name);
}
PaletteWindow* GUI::GetPalette() {
	return g_palettes.GetPalette();
}
const std::list<PaletteWindow*>& GUI::GetPalettes() {
	return g_palettes.palettes;
}
void GUI::DestroyPalettes() {
	g_palettes.DestroyPalettes();
}
PaletteWindow* GUI::CreatePalette() {
	return g_palettes.CreatePalette();
}

void SetWindowToolTip(wxWindow* a, const wxString& tip) {
	a->SetToolTip(tip);
}

void SetWindowToolTip(wxWindow* a, wxWindow* b, const wxString& tip) {
	a->SetToolTip(tip);
	b->SetToolTip(tip);
}
