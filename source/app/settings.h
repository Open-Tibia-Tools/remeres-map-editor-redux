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

#ifndef RME_SETTINGS_H_
#define RME_SETTINGS_H_

#include "app/main.h"

#include <toml++/toml.h>
#include <variant>
#include <functional>
#include <vector>

namespace Config {
	enum Key {
		NONE,
		VERSION_ID,

		USE_CUSTOM_DATA_DIRECTORY,
		DATA_DIRECTORY,
		EXTENSIONS_DIRECTORY,

		MERGE_MOVE,
		TEXTURE_MANAGEMENT,
		TEXTURE_CLEAN_PULSE,
		TEXTURE_CLEAN_THRESHOLD,
		TEXTURE_LONGEVITY,
		HARD_REFRESH_RATE,
		USE_MEMCACHED_SPRITES,
		USE_MEMCACHED_SPRITES_TO_SAVE,
		SOFTWARE_CLEAN_THRESHOLD,
		SOFTWARE_CLEAN_SIZE,
		TRANSPARENT_FLOORS,
		TRANSPARENT_ITEMS,
		SHOW_INGAME_BOX,
		SHOW_GRID,
		SHOW_EXTRA,
		SHOW_ALL_FLOORS,
		SHOW_CREATURES,
		SHOW_SPAWNS,
		SHOW_HOUSES,
		SHOW_SHADE,
		SHOW_SPECIAL_TILES,
		HIGHLIGHT_ITEMS,
		SHOW_ITEMS,
		SHOW_BLOCKING,
		SHOW_TOOLTIPS,
		SHOW_PREVIEW,
		SHOW_WALL_HOOKS,
		SHOW_AS_MINIMAP,
		SHOW_ONLY_TILEFLAGS,
		SHOW_ONLY_MODIFIED_TILES,
		SHOW_AUTOBORDER_PREVIEW,
		HIDE_ITEMS_WHEN_ZOOMED,
		GROUP_ACTIONS,
		SCROLL_SPEED,
		ZOOM_SPEED,
		UNDO_SIZE,
		UNDO_MEM_SIZE,
		MERGE_PASTE,
		SELECTION_TYPE,
		COMPENSATED_SELECT,
		BORDER_IS_GROUND,
		BORDERIZE_PASTE,
		BORDERIZE_DRAG,
		BORDERIZE_DRAG_THRESHOLD,
		BORDERIZE_PASTE_THRESHOLD,
		ICON_BACKGROUND,
		ALWAYS_MAKE_BACKUP,
		USE_AUTOMAGIC,
		HOUSE_BRUSH_REMOVE_ITEMS,
		AUTO_ASSIGN_DOORID,
		ERASER_LEAVE_UNIQUE,
		DOODAD_BRUSH_ERASE_LIKE,
		WARN_FOR_DUPLICATE_ID,
		USE_UPDATER,
		USE_OTBM_4_FOR_ALL_MAPS,
		SAVE_WITH_OTB_MAGIC_NUMBER,
		REPLACE_SIZE,

		USE_LARGE_CONTAINER_ICONS,
		USE_LARGE_CHOOSE_ITEM_ICONS,
		USE_LARGE_TERRAIN_TOOLBAR,
		USE_LARGE_DOODAD_SIZEBAR,
		USE_LARGE_ITEM_SIZEBAR,
		USE_LARGE_HOUSE_SIZEBAR,
		USE_LARGE_RAW_SIZEBAR,
		USE_GUI_SELECTION_SHADOW,
		PALETTE_COL_COUNT,
		PALETTE_TERRAIN_STYLE,
		PALETTE_DOODAD_STYLE,
		PALETTE_ITEM_STYLE,
		PALETTE_RAW_STYLE,
		PALETTE_CREATURE_STYLE,

		ASSETS_DATA_DIRS,
		DEFAULT_CLIENT_VERSION,
		CHECK_SIGNATURES,

		CURSOR_RED,
		CURSOR_GREEN,
		CURSOR_BLUE,
		CURSOR_ALPHA,

		CURSOR_ALT_RED,
		CURSOR_ALT_GREEN,
		CURSOR_ALT_BLUE,
		CURSOR_ALT_ALPHA,

		SCREENSHOT_DIRECTORY,
		SCREENSHOT_FORMAT,
		MAX_SPAWN_RADIUS,
		CURRENT_SPAWN_RADIUS,
		AUTO_CREATE_SPAWN,
		DEFAULT_SPAWNTIME,
		SWITCH_MOUSEBUTTONS,
		DOUBLECLICK_PROPERTIES,
		LISTBOX_EATS_ALL_EVENTS,
		RAW_LIKE_SIMONE,
		WORKER_THREADS,
		COPY_POSITION_FORMAT,

		GOTO_WEBSITE_ON_BOOT,
		INDIRECTORY_INSTALLATION,
		AUTOCHECK_FOR_UPDATES,
		ONLY_ONE_INSTANCE,
		SHOW_TILESET_EDITOR,

		PALETTE_LAYOUT,
		MINIMAP_VISIBLE,
		MINIMAP_LAYOUT,
		MINIMAP_UPDATE_DELAY,
		MINIMAP_VIEW_BOX,
		MINIMAP_EXPORT_DIR,
		TILESET_EXPORT_DIR,
		TOOL_OPTIONS_LAYOUT,
		INGAME_PREVIEW_VISIBLE,
		INGAME_PREVIEW_LAYOUT,
		HOUSE_PALETTE_LAYOUT,
		WINDOW_HEIGHT,
		WINDOW_WIDTH,
		WINDOW_MAXIMIZED,
		REPLACE_TOOL_WINDOW_WIDTH,
		REPLACE_TOOL_WINDOW_HEIGHT,
		WELCOME_DIALOG,

		NUMERICAL_HOTKEYS,
		HOTKEY_0,
		HOTKEY_1,
		HOTKEY_2,
		HOTKEY_3,
		HOTKEY_4,
		HOTKEY_5,
		HOTKEY_6,
		HOTKEY_7,
		HOTKEY_8,
		HOTKEY_9,
		RECENT_FILES,

		RECENT_EDITED_MAP_PATH,
		RECENT_EDITED_MAP_POSITION,

		SHOW_TOOLBAR_STANDARD,
		SHOW_TOOLBAR_BRUSHES,
		SHOW_TOOLBAR_POSITION,
		SHOW_TOOLBAR_SIZES,
		TOOLBAR_STANDARD_LAYOUT,
		TOOLBAR_BRUSHES_LAYOUT,
		TOOLBAR_POSITION_LAYOUT,
		TOOLBAR_SIZES_LAYOUT,

		PREFERENCES_WINDOW_WIDTH,
		PREFERENCES_WINDOW_HEIGHT,

		// add new settings at the end to make sure nothing gets misread
		DRAW_LOCKED_DOOR,
		THEME,
		HIGHLIGHT_LOCKED_DOORS,
		PALETTE_COLLECTION_STYLE,
		USE_LARGE_COLLECTION_TOOLBAR,
		SHOW_LIGHTS,
		SHOW_LIGHT_STR,
		SHOW_TECHNICAL_ITEMS,
		SHOW_WAYPOINTS,

		SHOW_TOWNS,
		ALWAYS_SHOW_ZONES,
		EXT_HOUSE_SHADER,

		FRAME_RATE_LIMIT,
		SHOW_FPS_COUNTER,

		ANTI_ALIASING,
		SCREEN_SHADER,

		ADVANCED_ITEM_FINDER_TYPE_FILTERS,
		ADVANCED_ITEM_FINDER_PROPERTY_FILTERS,
		ADVANCED_ITEM_FINDER_INTERACTION_FILTERS,
		ADVANCED_ITEM_FINDER_VISUAL_FILTERS,
		ADVANCED_ITEM_FINDER_QUERY_TEXT,
		ADVANCED_ITEM_FINDER_SELECTED_KIND,
		ADVANCED_ITEM_FINDER_SELECTED_SERVER_ID,
		ADVANCED_ITEM_FINDER_SELECTED_CREATURE,
		ADVANCED_ITEM_FINDER_WINDOW_X,
		ADVANCED_ITEM_FINDER_WINDOW_Y,
		ADVANCED_ITEM_FINDER_WINDOW_WIDTH,
		ADVANCED_ITEM_FINDER_WINDOW_HEIGHT,
		SEARCH_RESULTS_LIMIT,
		SHOW_INVALID_TILES,
		SHOW_INVALID_ZONES,

		// Add new settings at the end to preserve enum ordering for persisted settings
		SHOW_MISSING_ITEMS_WARNING,
		VSYNC_MODE,
		FLOOR_VISIBILITY_MODE,
		PALETTE_DYNAMIC_STYLE,
		PALETTE_SORT_KEY,
		PALETTE_SORT_DIR,
		PALETTE_HAS_SORT,
		PALETTE_SHOW_LABELS,
		PALETTE_TILE_SIZE,
		PALETTE_FILTER_ALL,
		PALETTE_FILTER_QUERY,

		LAST,
	};
}

class Settings {
public:
	Settings();
	~Settings();

	bool getBoolean(uint32_t key) const;
	int getInteger(uint32_t key) const;
	uint32_t getUnsignedInteger(uint32_t key) const;
	float getFloat(uint32_t key) const;
	std::string getString(uint32_t key) const;

	void setInteger(uint32_t key, int newval);
	void setUnsignedInteger(uint32_t key, uint32_t newval);
	void setFloat(uint32_t key, float newval);
	void setString(uint32_t key, std::string newval);

	using ObserverId = uint64_t;
	using ObserverCallback = std::function<void(uint32_t key)>;

	ObserverId addObserver(ObserverCallback callback);
	void removeObserver(ObserverId id);
	void notifyObservers(uint32_t key);

	toml::table& getTable();
	void setDefaults() {
		IO(DEFAULT);
	}
	void load();
	void save(bool endoftheworld = false);

public:
	class DynamicValue {
	public:
		DynamicValue() = default;

		std::string str();

	private:
		std::variant<std::monostate, int, uint32_t, float, std::string> val;

		friend class Settings;
	};

private:
	enum IOMode {
		DEFAULT,
		LOAD,
		SAVE,
	};
	void IO(IOMode mode);
	std::vector<DynamicValue> store;

	struct ObserverEntry {
		ObserverId id;
		ObserverCallback callback;
	};
	std::vector<ObserverEntry> observers_;
	ObserverId next_observer_id_ = 1;
};

extern Settings g_settings;

#endif
