#include "app/definitions.h"
#include "brushes/managers/brush_manager.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/light_defaults.h"


#include <algorithm>

DrawingOptions::DrawingOptions() {
	SetDefault();
}

void DrawingOptions::SetDefault() {
	transparent_floors = false;
	transparent_items = false;
	show_ingame_box = false;
	show_lights = false;
	show_light_str = true;
	show_tech_items = true;
	show_invalid_tiles = true;
	show_invalid_zones = true;
	show_waypoints = true;
	ingame = false;
	is_drawing_mode = false;
	dragging = false;
	boundbox_selection = false;

	show_grid = 0;
	show_all_floors = true;
	floor_visibility_mode = FloorVisibilityMode::ClientVisible;
	show_creatures = true;
	show_spawns = true;
	show_houses = true;
	show_shade = true;
	show_special_tiles = true;
	show_items = true;

	highlight_items = false;
	highlight_locked_doors = true;
	show_blocking = false;
	show_tooltips = false;
	show_as_minimap = false;
	show_only_colors = false;
	show_only_modified = false;
	show_preview = false;
	show_hooks = false;
	hide_items_when_zoomed = true;
	current_house_id = 0;
	draw_floor_shadow = show_shade;
	server_light = SpriteLight {
		.intensity = rme::lighting::DEFAULT_SERVER_LIGHT_INTENSITY,
		.color = rme::lighting::DEFAULT_SERVER_LIGHT_COLOR
	};
	minimum_ambient_light = rme::lighting::DEFAULT_MINIMUM_AMBIENT_LIGHT;
	highlight_pulse = 0.0f;
	anti_aliasing = false;
}

void DrawingOptions::SetIngame() {
	transparent_floors = false;
	transparent_items = false;
	show_ingame_box = false;
	show_lights = false;
	show_light_str = false;
	show_tech_items = false;
	show_invalid_tiles = false;
	show_invalid_zones = false;
	show_waypoints = false;
	ingame = true;
	dragging = false;
	boundbox_selection = false;

	show_grid = 0;
	show_all_floors = true;
	floor_visibility_mode = FloorVisibilityMode::ClientVisible;
	show_creatures = true;
	show_spawns = false;
	show_houses = false;
	show_shade = false;
	show_special_tiles = false;
	show_items = true;

	highlight_items = false;
	highlight_locked_doors = false;
	show_blocking = false;
	show_tooltips = false;
	show_as_minimap = false;
	show_only_colors = false;
	show_only_modified = false;
	show_preview = false;
	show_hooks = false;
	hide_items_when_zoomed = false;
	current_house_id = 0;
	draw_floor_shadow = show_shade;
	server_light = SpriteLight {
		.intensity = rme::lighting::DEFAULT_SERVER_LIGHT_INTENSITY,
		.color = rme::lighting::DEFAULT_SERVER_LIGHT_COLOR
	};
	minimum_ambient_light = rme::lighting::DEFAULT_MINIMUM_AMBIENT_LIGHT;
}

#include "app/settings.h"

void DrawingOptions::Update(const Settings& settings, const BrushManager& brush_manager) {
	transparent_floors = settings.getBoolean(Config::TRANSPARENT_FLOORS);
	transparent_items = settings.getBoolean(Config::TRANSPARENT_ITEMS);
	show_ingame_box = settings.getBoolean(Config::SHOW_INGAME_BOX);
	show_lights = settings.getBoolean(Config::SHOW_LIGHTS);
	show_light_str = settings.getBoolean(Config::SHOW_LIGHT_STR);
	show_tech_items = settings.getBoolean(Config::SHOW_TECHNICAL_ITEMS);
	show_invalid_tiles = settings.getBoolean(Config::SHOW_INVALID_TILES);
	show_invalid_zones = settings.getBoolean(Config::SHOW_INVALID_ZONES);
	show_waypoints = settings.getBoolean(Config::SHOW_WAYPOINTS);
	show_grid = settings.getInteger(Config::SHOW_GRID);
	ingame = !settings.getBoolean(Config::SHOW_EXTRA);
	show_all_floors = settings.getBoolean(Config::SHOW_ALL_FLOORS);
	floor_visibility_mode = SanitizeFloorVisibilityMode(settings.getInteger(Config::FLOOR_VISIBILITY_MODE));
	show_creatures = settings.getBoolean(Config::SHOW_CREATURES);
	show_spawns = settings.getBoolean(Config::SHOW_SPAWNS);
	show_houses = settings.getBoolean(Config::SHOW_HOUSES);
	show_shade = settings.getBoolean(Config::SHOW_SHADE);
	show_special_tiles = settings.getBoolean(Config::SHOW_SPECIAL_TILES);
	show_items = settings.getBoolean(Config::SHOW_ITEMS);
	highlight_items = settings.getBoolean(Config::HIGHLIGHT_ITEMS);
	highlight_locked_doors = settings.getBoolean(Config::HIGHLIGHT_LOCKED_DOORS);
	show_blocking = settings.getBoolean(Config::SHOW_BLOCKING);
	show_tooltips = settings.getBoolean(Config::SHOW_TOOLTIPS);
	show_as_minimap = settings.getBoolean(Config::SHOW_AS_MINIMAP);
	show_only_colors = settings.getBoolean(Config::SHOW_ONLY_TILEFLAGS);
	show_only_modified = settings.getBoolean(Config::SHOW_ONLY_MODIFIED_TILES);
	show_preview = settings.getBoolean(Config::SHOW_PREVIEW);
	show_hooks = settings.getBoolean(Config::SHOW_WALL_HOOKS);
	hide_items_when_zoomed = settings.getBoolean(Config::HIDE_ITEMS_WHEN_ZOOMED);
	show_towns = settings.getBoolean(Config::SHOW_TOWNS);
	always_show_zones = settings.getBoolean(Config::ALWAYS_SHOW_ZONES);
	extended_house_shader = settings.getBoolean(Config::EXT_HOUSE_SHADER);
	server_light = SpriteLight {
		.intensity = static_cast<uint8_t>(std::clamp(brush_manager.GetLightIntensity(), 0, 255)),
		.color = static_cast<uint8_t>(std::clamp(brush_manager.GetServerLightColor(), 0, 255))
	};
	minimum_ambient_light = std::clamp(brush_manager.GetAmbientLightLevel(), 0.0f, 1.0f);
	draw_floor_shadow = show_shade;

	anti_aliasing = settings.getBoolean(Config::ANTI_ALIASING);
	dirty_ = false;
}

void DrawingOptions::UpdateIfNeeded(const Settings& settings, const BrushManager& brush_manager) {
	if (dirty_) {
		Update(settings, brush_manager);
	}
}

void DrawingOptions::Update() {
	Update(g_settings, g_brush_manager);
}

void DrawingOptions::UpdateIfNeeded() {
	if (dirty_) {
		Update();
	}
}

bool DrawingOptions::isDrawLight() const noexcept {
	return show_lights && minimum_ambient_light < 1.0f;
}
