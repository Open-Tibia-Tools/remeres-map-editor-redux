//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_BRUSH_MANAGER_H_
#define RME_BRUSH_MANAGER_H_

#include "app/main.h"
#include "brushes/brush_enums.h"
#include "brushes/brush_footprint.h"

class Brush;
class HouseBrush;
class HouseExitBrush;
class WaypointBrush;
class OptionalBorderBrush;
class EraserBrush;
class SpawnBrush;
class DoorBrush;
class FlagBrush;

class BrushManager {
public:
	BrushManager();
	~BrushManager();

	// Selects the currently seleceted brush in the active palette
	void SelectBrush();
	// Updates the palette AND selects the brush
	// Returns true if the brush was found and selected
	bool SelectBrush(const Brush* brush);
	// Selects the brush selected before the current brush
	void SelectPreviousBrush();
	// Only selects the brush, doesn't update the palette
	void SelectBrushInternal(Brush* brush);

	void Clear();
	void FillDoodadPreviewBuffer();

	// Get different brush parameters
	[[nodiscard]] Brush* GetCurrentBrush() const {
		return current_brush;
	}
	[[nodiscard]] BrushShape GetBrushShape() const;
	[[nodiscard]] int GetBrushSize() const {
		return GetBrushSizeLegacy();
	}
	[[nodiscard]] int GetBrushSizeLegacy() const;
	[[nodiscard]] int GetBrushSizeX() const {
		return brush_size_x;
	}
	[[nodiscard]] int GetBrushSizeY() const {
		return brush_size_y;
	}
	[[nodiscard]] bool IsExactBrushSize() const {
		return exact_brush_size;
	}
	[[nodiscard]] bool IsBrushAspectRatioLocked() const {
		return aspect_ratio_locked;
	}
	[[nodiscard]] BrushSizeState GetBrushSizeState() const;
	[[nodiscard]] BrushFootprint GetBrushFootprint() const;
	[[nodiscard]] int GetBrushVariation() const {
		return brush_variation;
	}
	[[nodiscard]] int GetSpawnTime() const {
		return creature_spawntime;
	}

	// Additional brush parameters
	void SetSpawnTime(int time) {
		creature_spawntime = time;
	}

	void SetLightIntensity(int v) {
		light_intensity = v;
	}
	[[nodiscard]] int GetLightIntensity() const {
		return light_intensity;
	}

	void SetAmbientLightLevel(float v) {
		ambient_light_level = v;
	}
	[[nodiscard]] float GetAmbientLightLevel() const {
		return ambient_light_level;
	}
	void SetServerLightColor(int v) {
		server_light_color = v;
	}
	[[nodiscard]] int GetServerLightColor() const {
		return server_light_color;
	}

	void SetBrushSize(int nz);
	void SetBrushSizeInternal(int nz);
	void SetBrushSizeX(int nz);
	void SetBrushSizeY(int nz);
	void SetBrushSizeAxes(int x, int y);
	void SetExactBrushSize(bool exact);
	void SetBrushAspectRatioLocked(bool locked);
	void SetBrushShape(BrushShape bs);
	void SetBrushVariation(int nz);
	void SetBrushThickness(int low, int ceil);
	void SetBrushThickness(bool on, int low = -1, int ceil = -1);
	void RestoreBrushSizeState(const BrushSizeState& state);

	// Helper functions for size
	void DecreaseBrushSize(bool wrap = false);
	void IncreaseBrushSize(bool wrap = false);

	// Door brush options
	void SetDoorLocked(bool on);
	[[nodiscard]] bool HasDoorLocked() const {
		return draw_locked_doors;
	}

	[[nodiscard]] float GetCustomThicknessMod() const {
		return custom_thickness_mod;
	}
	[[nodiscard]] bool UseCustomThickness() const {
		return use_custom_thickness;
	}

public:
	// Brush references
	HouseBrush* house_brush;
	HouseExitBrush* house_exit_brush;
	WaypointBrush* waypoint_brush;
	OptionalBorderBrush* optional_brush;
	EraserBrush* eraser;
	SpawnBrush* spawn_brush;
	DoorBrush* normal_door_brush;
	DoorBrush* locked_door_brush;
	DoorBrush* magic_door_brush;
	DoorBrush* quest_door_brush;
	DoorBrush* hatch_door_brush;
	DoorBrush* normal_door_alt_brush;
	DoorBrush* archway_door_brush;
	DoorBrush* window_door_brush;
	FlagBrush* pz_brush;
	FlagBrush* rook_brush;
	FlagBrush* nolog_brush;
	FlagBrush* pvp_brush;
	FlagBrush* refresh_brush;

private:
	Brush* current_brush;
	Brush* previous_brush;
	BrushShape brush_shape;
	int brush_size_x;
	int brush_size_y;
	bool exact_brush_size;
	bool aspect_ratio_locked;
	int brush_variation;
	int creature_spawntime;

	void UpdateDoodadPreview();
	void NotifyBrushSizeChanged();
	[[nodiscard]] bool HasResizableDoodadBrush() const;

	bool draw_locked_doors;
	bool use_custom_thickness;
	float custom_thickness_mod;
	int light_intensity;
	float ambient_light_level;
	int server_light_color;
};

extern BrushManager g_brush_manager;

#endif
