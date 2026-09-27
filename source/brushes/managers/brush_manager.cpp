//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "brushes/managers/brush_manager.h"
#include "brushes/brush.h"
#include "brushes/managers/doodad_preview_manager.h"
#include "brushes/spawn/spawn_brush.h"
#include "ui/managers/status_manager.h"
#include "palette/managers/palette_manager.h"
#include "palette/palette_window.h"
#include "palette/house/house_palette.h"
#include "map/map.h"
#include "map/basemap.h"
#include "ui/gui.h"
#include "ui/main_toolbar.h"
#include "rendering/core/light_defaults.h"
#include <algorithm>
#include <array>

BrushManager g_brush_manager;

BrushManager::BrushManager() :
	house_brush(nullptr),
	house_exit_brush(nullptr),
	waypoint_brush(nullptr),
	optional_brush(nullptr),
	eraser(nullptr),
	normal_door_brush(nullptr),
	locked_door_brush(nullptr),
	magic_door_brush(nullptr),
	quest_door_brush(nullptr),
	hatch_door_brush(nullptr),
	window_door_brush(nullptr),
	normal_door_alt_brush(nullptr),
	archway_door_brush(nullptr),
	pz_brush(nullptr),
	rook_brush(nullptr),
	nolog_brush(nullptr),
	pvp_brush(nullptr),
	refresh_brush(nullptr),

	current_brush(nullptr),
	previous_brush(nullptr),
	brush_shape(BRUSHSHAPE_SQUARE),
	brush_size_x(1),
	brush_size_y(1),
	exact_brush_size(true),
	aspect_ratio_locked(true),
	brush_variation(0),
	creature_spawntime(0),
	draw_locked_doors(false),
	use_custom_thickness(false),
	custom_thickness_mod(0.0),
	light_intensity(rme::lighting::DEFAULT_SERVER_LIGHT_INTENSITY),
	ambient_light_level(rme::lighting::DEFAULT_MINIMUM_AMBIENT_LIGHT),
	server_light_color(rme::lighting::DEFAULT_SERVER_LIGHT_COLOR) {
}

BrushManager::~BrushManager() {
}

void BrushManager::SelectBrush() {
	if (g_palettes.palettes.empty()) {
		return;
	}

	SelectBrushInternal(g_palettes.palettes.front()->GetSelectedBrush());
	g_gui.RefreshView();
}

bool BrushManager::SelectBrush(const Brush* whatbrush) {
	if (!whatbrush) {
		return false;
	}

	if (g_palettes.palettes.empty()) {
		if (!g_palettes.CreatePalette()) {
			return false;
		}
	}

	const bool foundInPalette = g_palettes.palettes.front()->OnSelectBrush(whatbrush);

	SelectBrushInternal(const_cast<Brush*>(whatbrush));
	g_gui.root->GetAuiToolBar()->UpdateBrushButtons();
	return foundInPalette;
}

void BrushManager::SelectBrushInternal(Brush* brush) {
	if (current_brush != brush && brush) {
		previous_brush = current_brush;
	}

	current_brush = brush;
	if (!current_brush) {
		return;
	}

	g_status.SetStatusText("Selected brush: " + wxstr(brush->getName()));

	brush_variation = std::min(brush_variation, brush->getMaxVariation());
	// If we are switching away from a doodad brush, we need to clear the secondary map
	// Or if the new brush isn't a doodad brush
	MapTab* mapTab = g_gui.GetCurrentMapTab();
	if (brush->is<DoodadBrush>()) {
		UpdateDoodadPreview();
	} else {
		g_gui.SetCurrentMapSecondaryMap(nullptr);
		g_doodad_preview.Clear();
	}

	g_gui.SetDrawingMode();
	g_gui.RefreshView();
}

void BrushManager::SelectPreviousBrush() {
	if (previous_brush) {
		SelectBrush(previous_brush);
	}
}

void BrushManager::Clear() {
	current_brush = nullptr;
	previous_brush = nullptr;

	house_brush = nullptr;
	house_exit_brush = nullptr;
	waypoint_brush = nullptr;
	optional_brush = nullptr;
	eraser = nullptr;
	spawn_brush = nullptr;
	normal_door_brush = nullptr;
	locked_door_brush = nullptr;
	magic_door_brush = nullptr;
	quest_door_brush = nullptr;
	hatch_door_brush = nullptr;
	normal_door_alt_brush = nullptr;
	archway_door_brush = nullptr;
	window_door_brush = nullptr;
	pz_brush = nullptr;
	rook_brush = nullptr;
	nolog_brush = nullptr;
	pvp_brush = nullptr;
	refresh_brush = nullptr;
}

BrushShape BrushManager::GetBrushShape() const {
	if (current_brush == spawn_brush) {
		return BRUSHSHAPE_SQUARE;
	}
	return brush_shape;
}

int BrushManager::GetBrushSizeLegacy() const {
	if (exact_brush_size && brush_size_x == 1 && brush_size_y == 1) {
		return 0;
	}
	return std::max(brush_size_x, brush_size_y);
}

BrushSizeState BrushManager::GetBrushSizeState() const {
	return BrushSizeState {
		.shape = GetBrushShape(),
		.size_x = brush_size_x,
		.size_y = brush_size_y,
		.exact = exact_brush_size,
		.aspect_locked = aspect_ratio_locked,
	};
}

BrushFootprint BrushManager::GetBrushFootprint() const {
	return MakeBrushFootprint(GetBrushSizeState());
}

void BrushManager::SetBrushSizeInternal(int nz) {
	const int normalized_size = NormalizeBrushAxisValue(nz, false);
	const bool changed = brush_size_x != normalized_size || brush_size_y != normalized_size || exact_brush_size;
	const bool needs_resizable_doodad_preview = changed && HasResizableDoodadBrush();
	if (needs_resizable_doodad_preview) {
		brush_size_x = normalized_size;
		brush_size_y = normalized_size;
		exact_brush_size = false;
		aspect_ratio_locked = true;
		g_doodad_preview.FillBuffer();
		g_gui.SetCurrentMapSecondaryMap(g_doodad_preview.GetBufferMap());
	} else {
		brush_size_x = normalized_size;
		brush_size_y = normalized_size;
		exact_brush_size = false;
		aspect_ratio_locked = true;
	}
}

void BrushManager::SetBrushSize(int nz) {
	SetBrushSizeInternal(nz);
	NotifyBrushSizeChanged();
}

void BrushManager::SetBrushSizeX(int nz) {
	const int normalized = NormalizeBrushAxisValue(nz, exact_brush_size);
	SetBrushSizeAxes(normalized, aspect_ratio_locked ? normalized : brush_size_y);
}

void BrushManager::SetBrushSizeY(int nz) {
	const int normalized = NormalizeBrushAxisValue(nz, exact_brush_size);
	SetBrushSizeAxes(aspect_ratio_locked ? normalized : brush_size_x, normalized);
}

void BrushManager::SetBrushSizeAxes(int x, int y) {
	const int normalized_x = NormalizeBrushAxisValue(x, exact_brush_size);
	const int normalized_y = NormalizeBrushAxisValue(y, exact_brush_size);
	if (brush_size_x == normalized_x && brush_size_y == normalized_y) {
		return;
	}

	brush_size_x = normalized_x;
	brush_size_y = normalized_y;

	if (HasResizableDoodadBrush()) {
		UpdateDoodadPreview();
	}

	NotifyBrushSizeChanged();
}

void BrushManager::SetExactBrushSize(bool exact) {
	if (exact_brush_size == exact) {
		return;
	}

	exact_brush_size = exact;
	brush_size_x = NormalizeBrushAxisValue(brush_size_x, exact_brush_size);
	brush_size_y = NormalizeBrushAxisValue(brush_size_y, exact_brush_size);

	if (HasResizableDoodadBrush()) {
		UpdateDoodadPreview();
	}

	NotifyBrushSizeChanged();
}

void BrushManager::SetBrushAspectRatioLocked(bool locked) {
	if (aspect_ratio_locked == locked) {
		return;
	}

	aspect_ratio_locked = locked;
	if (aspect_ratio_locked) {
		if (brush_size_x == brush_size_y) {
			NotifyBrushSizeChanged();
			return;
		}
		SetBrushSizeAxes(brush_size_x, brush_size_x);
		return;
	}

	NotifyBrushSizeChanged();
}

void BrushManager::SetBrushVariation(int nz) {
	if (nz != brush_variation && current_brush && current_brush->is<DoodadBrush>()) {
		brush_variation = nz;
		UpdateDoodadPreview();
	}
}

void BrushManager::SetBrushShape(BrushShape bs) {
	if (bs != brush_shape && HasResizableDoodadBrush()) {
		brush_shape = bs;
		UpdateDoodadPreview();
	}
	brush_shape = bs;
	NotifyBrushSizeChanged();
}

void BrushManager::RestoreBrushSizeState(const BrushSizeState& state) {
	const int normalized_x = NormalizeBrushAxisValue(state.size_x, state.exact);
	const int normalized_y = NormalizeBrushAxisValue(state.size_y, state.exact);
	const int restored_y = state.aspect_locked ? normalized_x : normalized_y;
	const bool changed = brush_shape != state.shape ||
		brush_size_x != normalized_x ||
		brush_size_y != restored_y ||
		exact_brush_size != state.exact ||
		aspect_ratio_locked != state.aspect_locked;

	brush_shape = state.shape;
	brush_size_x = normalized_x;
	brush_size_y = restored_y;
	exact_brush_size = state.exact;
	aspect_ratio_locked = state.aspect_locked;

	if (!changed) {
		return;
	}

	if (HasResizableDoodadBrush()) {
		UpdateDoodadPreview();
	}

	NotifyBrushSizeChanged();
}

void BrushManager::SetBrushThickness(bool on, int x, int y) {
	use_custom_thickness = on;

	if (x != -1 || y != -1) {
		custom_thickness_mod = static_cast<float>(std::max(x, 1)) / static_cast<float>(std::max(y, 1));
	}

	if (current_brush && current_brush->is<DoodadBrush>()) {
		g_doodad_preview.FillBuffer();
	}

	g_gui.RefreshView();
}

void BrushManager::SetBrushThickness(int low, int ceil) {
	custom_thickness_mod = static_cast<float>(std::max(low, 1)) / static_cast<float>(std::max(ceil, 1));

	if (use_custom_thickness && current_brush && current_brush->is<DoodadBrush>()) {
		g_doodad_preview.FillBuffer();
	}

	g_gui.RefreshView();
}

void BrushManager::DecreaseBrushSize(bool wrap) {
	static constexpr std::array<int, 12> next_sizes = { 11, 0, 1, 1, 2, 2, 4, 4, 6, 6, 6, 8 };
	const int legacy_size = GetBrushSizeLegacy();
	if (legacy_size >= 0 && size_t(legacy_size) < next_sizes.size()) {
		if (legacy_size == 0 && !wrap) {
			return;
		}
		SetBrushSize(next_sizes[legacy_size]);
	} else {
		SetBrushSize(8);
	}
}

void BrushManager::IncreaseBrushSize(bool wrap) {
	static constexpr std::array<int, 12> next_sizes = { 1, 2, 4, 4, 6, 6, 8, 8, 11, 11, 11, 0 };
	const int legacy_size = GetBrushSizeLegacy();
	if (legacy_size >= 0 && size_t(legacy_size) < next_sizes.size()) {
		if (legacy_size == 11 && !wrap) {
			return;
		}
		SetBrushSize(next_sizes[legacy_size]);
	} else {
		if (wrap) {
			SetBrushSize(0);
		}
	}
}

void BrushManager::NotifyBrushSizeChanged() {
	for (auto& palette : g_palettes.palettes) {
		palette->OnUpdateBrushSize(brush_shape, GetBrushSizeLegacy());
	}

	if (g_gui.root && g_gui.root->GetAuiToolBar()) {
		g_gui.root->GetAuiToolBar()->UpdateBrushSize(brush_shape, GetBrushSizeLegacy());
	}
}

bool BrushManager::HasResizableDoodadBrush() const {
	return current_brush && current_brush->is<DoodadBrush>() && !current_brush->oneSizeFitsAll();
}

void BrushManager::SetDoorLocked(bool on) {
	draw_locked_doors = on;
	g_gui.RefreshView();
}

void BrushManager::FillDoodadPreviewBuffer() {
	g_doodad_preview.FillBuffer();
}
void BrushManager::UpdateDoodadPreview() {
	g_doodad_preview.FillBuffer();
	g_gui.SetCurrentMapSecondaryMap(g_doodad_preview.GetBufferMap());
}
