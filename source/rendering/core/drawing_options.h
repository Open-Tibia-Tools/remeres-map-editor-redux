#ifndef RME_RENDERING_DRAWING_OPTIONS_H_
#define RME_RENDERING_DRAWING_OPTIONS_H_

#include <cstdint>
#include <string>
#include <optional>
#include "map/position.h"
#include "rendering/core/floor_visibility_mode.h"
#include "rendering/core/sprite_light.h"

class Settings;
class BrushManager;

struct DrawingOptions {
	DrawingOptions();

	void SetIngame();
	void SetDefault();
	void Update(const Settings& settings, const BrushManager& brush_manager);
	void UpdateIfNeeded(const Settings& settings, const BrushManager& brush_manager);
	void Update();
	void UpdateIfNeeded();
	void MarkDirty() noexcept { dirty_ = true; }
	[[nodiscard]] bool isDirty() const noexcept { return dirty_; }
	void clearDirty() noexcept { dirty_ = false; }
	bool isDrawLight() const noexcept;

	bool transparent_floors;
	bool transparent_items;
	bool show_ingame_box;
	bool show_lights;
	bool show_light_str;
	bool show_tech_items;
	bool show_invalid_tiles;
	bool show_invalid_zones;
	bool show_waypoints;
	bool ingame;
	bool is_drawing_mode;
	bool dragging;
	bool boundbox_selection;

	std::optional<MapBounds> transient_selection_bounds;

	int show_grid;
	bool show_all_floors;
	FloorVisibilityMode floor_visibility_mode;
	bool show_creatures;
	bool show_spawns;
	bool show_houses;
	bool show_shade;
	bool show_special_tiles;
	bool show_items;

	bool highlight_items;
	bool highlight_locked_doors;
	bool show_blocking;
	bool show_tooltips;

	bool show_as_minimap;
	bool show_only_colors;
	bool show_only_modified;
	bool show_preview;
	bool show_hooks;
	bool hide_items_when_zoomed;
	bool show_towns;
	bool always_show_zones;
	bool extended_house_shader;

	bool draw_floor_shadow;

	uint32_t current_house_id;
	SpriteLight server_light;
	float minimum_ambient_light;
	float highlight_pulse;

	bool anti_aliasing;

	[[nodiscard]] bool hasTileColorModifiers() const noexcept {
		return show_blocking || highlight_items || show_spawns || show_houses || show_special_tiles || show_only_colors;
	}

private:
	bool dirty_ = true;
};

#endif
