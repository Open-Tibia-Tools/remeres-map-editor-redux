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

	chunk_bake_dirty_ = true;
	lighting_dirty_ = true;
	visual_dirty_ = true;
	dirty_ = true;
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

	chunk_bake_dirty_ = true;
	lighting_dirty_ = true;
	visual_dirty_ = true;
	dirty_ = true;
}

#include "app/settings.h"

void DrawingOptions::MarkSettingDirty(uint32_t key) noexcept {
	switch (key) {
		case Config::TRANSPARENT_ITEMS:
		case Config::SHOW_SPECIAL_TILES:
		case Config::SHOW_HOUSES:
		case Config::EXT_HOUSE_SHADER:
		case Config::SHOW_BLOCKING:
		case Config::SHOW_SPAWNS:
		case Config::HIGHLIGHT_ITEMS:
		case Config::SHOW_ONLY_TILEFLAGS:
		case Config::SHOW_ONLY_MODIFIED_TILES:
		case Config::SHOW_ITEMS:
		case Config::SHOW_AS_MINIMAP:
			chunk_bake_dirty_ = true;
			break;

		case Config::SHOW_LIGHTS:
		case Config::SHOW_LIGHT_STR:
			lighting_dirty_ = true;
			break;

		default:
			visual_dirty_ = true;
			break;
	}
	dirty_ = true;
}

void DrawingOptions::Update(const Settings& settings, const BrushManager& brush_manager) {
	const bool new_transparent_floors = settings.getBoolean(Config::TRANSPARENT_FLOORS);
	const bool new_transparent_items = settings.getBoolean(Config::TRANSPARENT_ITEMS);
	const bool new_show_ingame_box = settings.getBoolean(Config::SHOW_INGAME_BOX);
	const bool new_show_lights = settings.getBoolean(Config::SHOW_LIGHTS);
	const bool new_show_light_str = settings.getBoolean(Config::SHOW_LIGHT_STR);
	const bool new_show_tech_items = settings.getBoolean(Config::SHOW_TECHNICAL_ITEMS);
	const bool new_show_invalid_tiles = settings.getBoolean(Config::SHOW_INVALID_TILES);
	const bool new_show_invalid_zones = settings.getBoolean(Config::SHOW_INVALID_ZONES);
	const bool new_show_waypoints = settings.getBoolean(Config::SHOW_WAYPOINTS);
	const int new_show_grid = settings.getInteger(Config::SHOW_GRID);
	const bool new_ingame = !settings.getBoolean(Config::SHOW_EXTRA);
	const bool new_show_all_floors = settings.getBoolean(Config::SHOW_ALL_FLOORS);
	const FloorVisibilityMode new_floor_visibility_mode = SanitizeFloorVisibilityMode(settings.getInteger(Config::FLOOR_VISIBILITY_MODE));
	const bool new_show_creatures = settings.getBoolean(Config::SHOW_CREATURES);
	const bool new_show_spawns = settings.getBoolean(Config::SHOW_SPAWNS);
	const bool new_show_houses = settings.getBoolean(Config::SHOW_HOUSES);
	const bool new_show_shade = settings.getBoolean(Config::SHOW_SHADE);
	const bool new_show_special_tiles = settings.getBoolean(Config::SHOW_SPECIAL_TILES);
	const bool new_show_items = settings.getBoolean(Config::SHOW_ITEMS);
	const bool new_highlight_items = settings.getBoolean(Config::HIGHLIGHT_ITEMS);
	const bool new_highlight_locked_doors = settings.getBoolean(Config::HIGHLIGHT_LOCKED_DOORS);
	const bool new_show_blocking = settings.getBoolean(Config::SHOW_BLOCKING);
	const bool new_show_tooltips = settings.getBoolean(Config::SHOW_TOOLTIPS);
	const bool new_show_as_minimap = settings.getBoolean(Config::SHOW_AS_MINIMAP);
	const bool new_show_only_colors = settings.getBoolean(Config::SHOW_ONLY_TILEFLAGS);
	const bool new_show_only_modified = settings.getBoolean(Config::SHOW_ONLY_MODIFIED_TILES);
	const bool new_show_preview = settings.getBoolean(Config::SHOW_PREVIEW);
	const bool new_show_hooks = settings.getBoolean(Config::SHOW_WALL_HOOKS);
	const bool new_hide_items_when_zoomed = settings.getBoolean(Config::HIDE_ITEMS_WHEN_ZOOMED);
	const bool new_show_towns = settings.getBoolean(Config::SHOW_TOWNS);
	const bool new_always_show_zones = settings.getBoolean(Config::ALWAYS_SHOW_ZONES);
	const bool new_extended_house_shader = settings.getBoolean(Config::EXT_HOUSE_SHADER);
	const SpriteLight new_server_light = SpriteLight {
		.intensity = static_cast<uint8_t>(std::clamp(brush_manager.GetLightIntensity(), 0, 255)),
		.color = static_cast<uint8_t>(std::clamp(brush_manager.GetServerLightColor(), 0, 255))
	};
	const float new_minimum_ambient_light = std::clamp(brush_manager.GetAmbientLightLevel(), 0.0f, 1.0f);
	const bool new_anti_aliasing = settings.getBoolean(Config::ANTI_ALIASING);

	// Differential Change Detection: Only dirty when values actually change!
	if (new_transparent_items != transparent_items ||
		new_show_special_tiles != show_special_tiles ||
		new_show_houses != show_houses ||
		new_extended_house_shader != extended_house_shader ||
		new_show_blocking != show_blocking ||
		new_show_spawns != show_spawns ||
		new_highlight_items != highlight_items ||
		new_show_only_colors != show_only_colors ||
		new_show_only_modified != show_only_modified ||
		new_show_items != show_items ||
		new_show_as_minimap != show_as_minimap) {
		chunk_bake_dirty_ = true;
	}

	if (new_show_lights != show_lights ||
		new_show_light_str != show_light_str ||
		new_server_light != server_light ||
		new_minimum_ambient_light != minimum_ambient_light) {
		lighting_dirty_ = true;
	}

	visual_dirty_ = true;

	transparent_floors = new_transparent_floors;
	transparent_items = new_transparent_items;
	show_ingame_box = new_show_ingame_box;
	show_lights = new_show_lights;
	show_light_str = new_show_light_str;
	show_tech_items = new_show_tech_items;
	show_invalid_tiles = new_show_invalid_tiles;
	show_invalid_zones = new_show_invalid_zones;
	show_waypoints = new_show_waypoints;
	show_grid = new_show_grid;
	ingame = new_ingame;
	show_all_floors = new_show_all_floors;
	floor_visibility_mode = new_floor_visibility_mode;
	show_creatures = new_show_creatures;
	show_spawns = new_show_spawns;
	show_houses = new_show_houses;
	show_shade = new_show_shade;
	show_special_tiles = new_show_special_tiles;
	show_items = new_show_items;
	highlight_items = new_highlight_items;
	highlight_locked_doors = new_highlight_locked_doors;
	show_blocking = new_show_blocking;
	show_tooltips = new_show_tooltips;
	show_as_minimap = new_show_as_minimap;
	show_only_colors = new_show_only_colors;
	show_only_modified = new_show_only_modified;
	show_preview = new_show_preview;
	show_hooks = new_show_hooks;
	hide_items_when_zoomed = new_hide_items_when_zoomed;
	show_towns = new_show_towns;
	always_show_zones = new_always_show_zones;
	extended_house_shader = new_extended_house_shader;
	server_light = new_server_light;
	minimum_ambient_light = new_minimum_ambient_light;
	draw_floor_shadow = show_shade;
	anti_aliasing = new_anti_aliasing;

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
