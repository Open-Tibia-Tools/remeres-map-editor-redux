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

#include "app/settings.h"
#include "ui/gui_ids.h"
#include "app/client_version.h"
#include "app/main.h"
#include "ui/managers/vsync_policy.h"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <toml++/toml.h>

#include <iostream>
#include <fstream>
#include <limits>
#include <string>

static toml::table g_settings_table;

toml::table& Settings::getTable() {
	return g_settings_table;
}

Settings g_settings;

Settings::Settings() :
	store(Config::LAST) {
	setDefaults();
}

Settings::~Settings() {
	////
}

bool Settings::getBoolean(uint32_t key) const {
	if (key > Config::LAST) {
		return false;
	}

	const DynamicValue& dv = store[key];
	if (auto val = std::get_if<int>(&dv.val)) {
		return *val != 0;
	}
	return false;
}

int Settings::getInteger(uint32_t key) const {
	if (key > Config::LAST) {
		return 0;
	}
	const DynamicValue& dv = store[key];
	if (auto val = std::get_if<int>(&dv.val)) {
		return *val;
	}
	return 0;
}

uint32_t Settings::getUnsignedInteger(uint32_t key) const {
	if (key > Config::LAST) {
		return 0;
	}
	const DynamicValue& dv = store[key];
	if (auto val = std::get_if<uint32_t>(&dv.val)) {
		return *val;
	}
	if (auto val = std::get_if<int>(&dv.val)) {
		return static_cast<uint32_t>(std::max(0, *val));
	}
	return 0;
}

float Settings::getFloat(uint32_t key) const {
	if (key > Config::LAST) {
		return 0.0;
	}
	const DynamicValue& dv = store[key];
	if (auto val = std::get_if<float>(&dv.val)) {
		return *val;
	}
	return 0.0;
}

std::string Settings::getString(uint32_t key) const {
	if (key > Config::LAST) {
		return "";
	}
	const DynamicValue& dv = store[key];
	if (auto val = std::get_if<std::string>(&dv.val)) {
		return *val;
	}
	return "";
}

void Settings::setInteger(uint32_t key, int newval) {
	if (key > Config::LAST) {
		return;
	}
	DynamicValue& dv = store[key];
	if (std::holds_alternative<int>(dv.val) || std::holds_alternative<std::monostate>(dv.val)) {
		const auto old_val = std::get_if<int>(&dv.val);
		if (!old_val || *old_val != newval) {
			dv.val = newval;
			notifyObservers(key);
		}
	}
}

void Settings::setUnsignedInteger(uint32_t key, uint32_t newval) {
	if (key > Config::LAST) {
		return;
	}
	DynamicValue& dv = store[key];
	if (std::holds_alternative<uint32_t>(dv.val) || std::holds_alternative<std::monostate>(dv.val)) {
		const auto old_val = std::get_if<uint32_t>(&dv.val);
		if (!old_val || *old_val != newval) {
			dv.val = newval;
			notifyObservers(key);
		}
	}
}

void Settings::setFloat(uint32_t key, float newval) {
	if (key > Config::LAST) {
		return;
	}
	DynamicValue& dv = store[key];
	if (std::holds_alternative<float>(dv.val) || std::holds_alternative<std::monostate>(dv.val)) {
		const auto old_val = std::get_if<float>(&dv.val);
		if (!old_val || *old_val != newval) {
			dv.val = newval;
			notifyObservers(key);
		}
	}
}

void Settings::setString(uint32_t key, std::string newval) {
	if (key > Config::LAST) {
		return;
	}
	DynamicValue& dv = store[key];
	if (std::holds_alternative<std::string>(dv.val) || std::holds_alternative<std::monostate>(dv.val)) {
		const auto old_val = std::get_if<std::string>(&dv.val);
		if (!old_val || *old_val != newval) {
			dv.val = std::move(newval);
			notifyObservers(key);
		}
	}
}

Settings::ObserverId Settings::addObserver(ObserverCallback callback) {
	ObserverId id = next_observer_id_++;
	observers_.push_back({ id, std::move(callback) });
	return id;
}

void Settings::removeObserver(ObserverId id) {
	std::erase_if(observers_, [id](const ObserverEntry& entry) {
		return entry.id == id;
	});
}

void Settings::notifyObservers(uint32_t key) {
	const auto observers_copy = observers_;
	for (const auto& entry : observers_copy) {
		if (entry.callback) {
			entry.callback(key);
		}
	}
}

std::string Settings::DynamicValue::str() {
	return std::visit([](auto&& arg) -> std::string {
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, std::monostate>) {
			return "";
		} else if constexpr (std::is_same_v<T, int>) {
			return i2s(arg);
		} else if constexpr (std::is_same_v<T, uint32_t>) {
			return i2s(arg);
		} else if constexpr (std::is_same_v<T, float>) {
			return f2s(arg);
		} else {
			return arg;
		}
	},
					  val);
}

static std::string toLower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
	return s;
}

void Settings::IO(IOMode mode) {
	if (mode == LOAD) {
		try {
			g_settings_table = toml::parse_file("config.toml");
		} catch (const toml::parse_error& err) {
			spdlog::error("Failed to parse config.toml: {}", err.description());
		} catch (const std::exception& err) {
			spdlog::error("Failed to load config.toml: {}", err.what());
		}
	}

	toml::table* root = &g_settings_table;
	toml::table* cur_sec = root;

	using namespace Config;

#define section(s)                                                \
	do {                                                          \
		if (s[0] == '\0') {                                       \
			cur_sec = root;                                       \
		} else if (mode != DEFAULT) {                             \
			std::string sec_name = toLower(s);                    \
			cur_sec = root->get_as<toml::table>(sec_name);        \
			if (!cur_sec) {                                       \
				root->insert_or_assign(sec_name, toml::table {}); \
				cur_sec = root->get_as<toml::table>(sec_name);    \
			}                                                     \
		}                                                         \
	} while (false)

#define Bool(key, dflt)                                                  \
	do {                                                                 \
		std::string k = toLower(#key);                                   \
		if (mode == DEFAULT) {                                           \
			setInteger(key, dflt ? 1 : 0);                               \
		} else if (mode == SAVE) {                                       \
			cur_sec->insert_or_assign(k, getBoolean(key));               \
		} else if (mode == LOAD) {                                       \
			setInteger(key, (*cur_sec)[k].value_or(bool(dflt)) ? 1 : 0); \
		}                                                                \
	} while (false)

#define Int(key, dflt)                                               \
	do {                                                             \
		std::string k = toLower(#key);                               \
		if (mode == DEFAULT) {                                       \
			setInteger(key, dflt);                                   \
		} else if (mode == SAVE) {                                   \
			cur_sec->insert_or_assign(k, getInteger(key));           \
		} else if (mode == LOAD) {                                   \
			setInteger(key, (int)(*cur_sec)[k].value_or(int(dflt))); \
		}                                                            \
	} while (false)

#define UInt(key, dflt)                                                                                                         \
	do {                                                                                                                         \
		std::string k = toLower(#key);                                                                                           \
		if (mode == DEFAULT) {                                                                                                   \
			setUnsignedInteger(key, static_cast<uint32_t>(dflt));                                                                \
		} else if (mode == SAVE) {                                                                                               \
			cur_sec->insert_or_assign(k, getUnsignedInteger(key));                                                               \
		} else if (mode == LOAD) {                                                                                               \
			const auto unsigned_value = (*cur_sec)[k].value<uint64_t>();                                                         \
			const auto signed_value = (*cur_sec)[k].value<int64_t>();                                                            \
			const auto loaded_value = unsigned_value.has_value()                                                                  \
				? static_cast<uint32_t>(std::min<uint64_t>(*unsigned_value, std::numeric_limits<uint32_t>::max()))              \
				: signed_value.has_value() ? static_cast<uint32_t>(std::max<int64_t>(0, *signed_value)) : static_cast<uint32_t>(dflt); \
			setUnsignedInteger(key, loaded_value);                                                                               \
		}                                                                                                                        \
	} while (false)

#define IntToSave(key, dflt)                                         \
	do {                                                             \
		std::string k = toLower(#key);                               \
		if (mode == DEFAULT) {                                       \
			setInteger(key, dflt);                                   \
		} else if (mode == SAVE) {                                   \
			cur_sec->insert_or_assign(k, getInteger(key##_TO_SAVE)); \
		} else if (mode == LOAD) {                                   \
			const auto bool_val = (*cur_sec)[k].value<bool>();       \
			const auto int_val = (*cur_sec)[k].value<int64_t>();     \
			const int val = bool_val.has_value()                     \
				? (*bool_val ? 1 : 0)                                \
				: (int_val.has_value() ? static_cast<int>(*int_val) : (int)dflt); \
			setInteger(key, val);                                    \
			setInteger(key##_TO_SAVE, val);                          \
		}                                                            \
	} while (false)

#define Float(key, dflt)                                               \
	do {                                                               \
		std::string k = toLower(#key);                                 \
		if (mode == DEFAULT) {                                         \
			setFloat(key, dflt);                                       \
		} else if (mode == SAVE) {                                     \
			cur_sec->insert_or_assign(k, getFloat(key));               \
		} else if (mode == LOAD) {                                     \
			setFloat(key, (float)(*cur_sec)[k].value_or(float(dflt))); \
		}                                                              \
	} while (false)

#define String(key, dflt)                                              \
	do {                                                               \
		std::string k = toLower(#key);                                 \
		if (mode == DEFAULT) {                                         \
			setString(key, dflt);                                      \
		} else if (mode == SAVE) {                                     \
			cur_sec->insert_or_assign(k, getString(key));              \
		} else if (mode == LOAD) {                                     \
			setString(key, (*cur_sec)[k].value_or(std::string(dflt))); \
		}                                                              \
	} while (false)

	section("View");
	Bool(TRANSPARENT_FLOORS, false);
	Bool(TRANSPARENT_ITEMS, false);
	Bool(SHOW_ALL_FLOORS, true);
	Bool(SHOW_INGAME_BOX, false);
	Bool(SHOW_LIGHTS, false);
	Bool(SHOW_LIGHT_STR, true);
	Bool(SHOW_TECHNICAL_ITEMS, true);
	Bool(SHOW_INVALID_TILES, true);
	Bool(SHOW_WAYPOINTS, true);
	Bool(SHOW_INVALID_ZONES, true);
	Bool(SHOW_GRID, false);
	Bool(SHOW_EXTRA, true);
	Bool(SHOW_SHADE, true);
	Bool(SHOW_SPECIAL_TILES, true);
	Bool(SHOW_SPAWNS, true);
	Bool(SHOW_ITEMS, true);
	Bool(HIGHLIGHT_ITEMS, false);
	Bool(HIGHLIGHT_LOCKED_DOORS, true);
	Bool(SHOW_CREATURES, true);
	Bool(SHOW_HOUSES, true);
	Bool(SHOW_BLOCKING, false);
	Bool(SHOW_TOOLTIPS, true);
	Bool(SHOW_ONLY_TILEFLAGS, false);
	Bool(SHOW_ONLY_MODIFIED_TILES, false);
	Bool(SHOW_PREVIEW, true);
	Bool(SHOW_AUTOBORDER_PREVIEW, false);
	Bool(SHOW_WALL_HOOKS, false);
	Bool(SHOW_TOWNS, false);
	Bool(ALWAYS_SHOW_ZONES, true);
	Bool(EXT_HOUSE_SHADER, true);
	Bool(DRAW_LOCKED_DOOR, false);
	Bool(SHOW_AS_MINIMAP, false);

	section("General");
	Bool(GOTO_WEBSITE_ON_BOOT, false);
	Bool(USE_UPDATER, true);
	Bool(AUTOCHECK_FOR_UPDATES, false);

	section("Version");
	Int(VERSION_ID, 0);
	Int(CHECK_SIGNATURES, 1);
	Int(INDIRECTORY_INSTALLATION, 0);
	Int(USE_CUSTOM_DATA_DIRECTORY, 0);
	String(DATA_DIRECTORY, "");
	String(EXTENSIONS_DIRECTORY, "");
	String(ASSETS_DATA_DIRS, "");

	section("Editor");
	Int(WORKER_THREADS, 1);
	Bool(MERGE_MOVE, false);
	Bool(MERGE_PASTE, false);
	Int(UNDO_SIZE, 400);
	Int(UNDO_MEM_SIZE, 40);
	Bool(GROUP_ACTIONS, true);
	Int(SELECTION_TYPE, SELECT_CURRENT_FLOOR);
	Bool(COMPENSATED_SELECT, true);
	Float(SCROLL_SPEED, 3.5f);
	Float(ZOOM_SPEED, 1.4f);
	Bool(SWITCH_MOUSEBUTTONS, false);
	Bool(DOUBLECLICK_PROPERTIES, true);
	Bool(LISTBOX_EATS_ALL_EVENTS, true);
	Bool(BORDER_IS_GROUND, true);
	Bool(BORDERIZE_PASTE, true);
	Bool(BORDERIZE_DRAG, true);
	Int(BORDERIZE_DRAG_THRESHOLD, 6000);
	Int(BORDERIZE_PASTE_THRESHOLD, 10000);
	Bool(ALWAYS_MAKE_BACKUP, false);
	Bool(USE_AUTOMAGIC, true);
	Bool(HOUSE_BRUSH_REMOVE_ITEMS, false);
	Bool(AUTO_ASSIGN_DOORID, true);
	Bool(ERASER_LEAVE_UNIQUE, true);
	Bool(DOODAD_BRUSH_ERASE_LIKE, false);
	Bool(WARN_FOR_DUPLICATE_ID, true);
	Bool(AUTO_CREATE_SPAWN, false);
	Int(DEFAULT_SPAWNTIME, 60);
	Int(MAX_SPAWN_RADIUS, 30);
	Int(CURRENT_SPAWN_RADIUS, 5);
	Int(DEFAULT_CLIENT_VERSION, OTB_VERSION_NONE);
	Bool(RAW_LIKE_SIMONE, true);
	Bool(ONLY_ONE_INSTANCE, true);
	Bool(SHOW_TILESET_EDITOR, false);
	Bool(USE_OTBM_4_FOR_ALL_MAPS, false);
	Bool(SAVE_WITH_OTB_MAGIC_NUMBER, false);
	Int(REPLACE_SIZE, 500);
	Int(COPY_POSITION_FORMAT, 0);
	String(RECENT_EDITED_MAP_PATH, "");
	String(RECENT_EDITED_MAP_POSITION, "");
	UInt(ADVANCED_ITEM_FINDER_TYPE_FILTERS, 0);
	UInt(ADVANCED_ITEM_FINDER_PROPERTY_FILTERS, 0);
	UInt(ADVANCED_ITEM_FINDER_INTERACTION_FILTERS, 0);
	UInt(ADVANCED_ITEM_FINDER_VISUAL_FILTERS, 0);
	String(ADVANCED_ITEM_FINDER_QUERY_TEXT, "");
	Int(ADVANCED_ITEM_FINDER_SELECTED_KIND, 0);
	Int(ADVANCED_ITEM_FINDER_SELECTED_SERVER_ID, 0);
	String(ADVANCED_ITEM_FINDER_SELECTED_CREATURE, "");
	Int(SEARCH_RESULTS_LIMIT, 100000);

	section("Graphics");
	Bool(TEXTURE_MANAGEMENT, true);
	Int(TEXTURE_CLEAN_PULSE, 15);
	Int(TEXTURE_LONGEVITY, 20);
	Int(TEXTURE_CLEAN_THRESHOLD, 2500);
	Int(SOFTWARE_CLEAN_THRESHOLD, 1800);
	Int(SOFTWARE_CLEAN_SIZE, 500);
	Int(ICON_BACKGROUND, 0);
	Int(HARD_REFRESH_RATE, 200);
	Bool(HIDE_ITEMS_WHEN_ZOOMED, true);
	String(SCREENSHOT_DIRECTORY, "");
	String(SCREENSHOT_FORMAT, "png");
	IntToSave(USE_MEMCACHED_SPRITES, 1); // Default to enabled (true)
	Int(MINIMAP_UPDATE_DELAY, 333);
	Bool(MINIMAP_VIEW_BOX, true);
	String(MINIMAP_EXPORT_DIR, "");
	String(TILESET_EXPORT_DIR, "");
	Int(FRAME_RATE_LIMIT, 144);
	Bool(SHOW_FPS_COUNTER, false);
	Int(VSYNC_MODE, static_cast<int>(VSyncMode::On));
	Int(ANTI_ALIASING, 0);
	Int(HARDWARE_PROFILE_MODE, 0);

	Int(CURSOR_RED, 0);
	Int(CURSOR_GREEN, 166);
	Int(CURSOR_BLUE, 0);
	Int(CURSOR_ALPHA, 128);
	Int(CURSOR_ALT_RED, 0);
	Int(CURSOR_ALT_GREEN, 166);
	Int(CURSOR_ALT_BLUE, 0);
	Int(CURSOR_ALT_ALPHA, 128);

	section("UI");
	Bool(USE_LARGE_CONTAINER_ICONS, true);
	Bool(USE_LARGE_CHOOSE_ITEM_ICONS, true);
	Bool(USE_LARGE_TERRAIN_TOOLBAR, true);
	Bool(USE_LARGE_COLLECTION_TOOLBAR, true);
	Bool(USE_LARGE_DOODAD_SIZEBAR, true);
	Bool(USE_LARGE_ITEM_SIZEBAR, true);
	Bool(USE_LARGE_HOUSE_SIZEBAR, true);
	Bool(USE_LARGE_RAW_SIZEBAR, true);
	Bool(USE_GUI_SELECTION_SHADOW, false);
	Int(PALETTE_COL_COUNT, 8);
	String(PALETTE_TERRAIN_STYLE, "large icons");
	String(PALETTE_COLLECTION_STYLE, "large icons");
	String(PALETTE_DOODAD_STYLE, "large icons");
	String(PALETTE_ITEM_STYLE, "listbox");
	String(PALETTE_RAW_STYLE, "listbox");
	Int(PALETTE_CREATURE_STYLE, 0);

	section("Window");
	String(PALETTE_LAYOUT, "");
	String(TOOL_OPTIONS_LAYOUT, "");
	Bool(INGAME_PREVIEW_VISIBLE, false);
	String(INGAME_PREVIEW_LAYOUT, "");
	String(HOUSE_PALETTE_LAYOUT, "");
	Bool(MINIMAP_VISIBLE, true);
	String(MINIMAP_LAYOUT, "");
	Int(WINDOW_HEIGHT, 500);
	Int(WINDOW_WIDTH, 700);
	Int(REPLACE_TOOL_WINDOW_WIDTH, 1400);
	Int(REPLACE_TOOL_WINDOW_HEIGHT, 850);
	Bool(WINDOW_MAXIMIZED, false);
	Bool(WELCOME_DIALOG, true);
	Int(PREFERENCES_WINDOW_WIDTH, 1200);
	Int(PREFERENCES_WINDOW_HEIGHT, 500);
	Int(THEME, 0);
	Int(ADVANCED_ITEM_FINDER_WINDOW_X, -1);
	Int(ADVANCED_ITEM_FINDER_WINDOW_Y, -1);
	Int(ADVANCED_ITEM_FINDER_WINDOW_WIDTH, 1480);
	Int(ADVANCED_ITEM_FINDER_WINDOW_HEIGHT, 860);

	section("Hotkeys");
	String(NUMERICAL_HOTKEYS, "");
	String(HOTKEY_0, "");
	String(HOTKEY_1, "");
	String(HOTKEY_2, "");
	String(HOTKEY_3, "");
	String(HOTKEY_4, "");
	String(HOTKEY_5, "");
	String(HOTKEY_6, "");
	String(HOTKEY_7, "");
	String(HOTKEY_8, "");
	String(HOTKEY_9, "");

	section("Toolbars");
	Bool(SHOW_TOOLBAR_STANDARD, true);
	Bool(SHOW_TOOLBAR_BRUSHES, false);
	Bool(SHOW_TOOLBAR_POSITION, false);
	Bool(SHOW_TOOLBAR_SIZES, false);
	String(TOOLBAR_STANDARD_LAYOUT, "");
	String(TOOLBAR_BRUSHES_LAYOUT, "");
	String(TOOLBAR_POSITION_LAYOUT, "");
	String(TOOLBAR_SIZES_LAYOUT, "");

	// Missing items report (added at end to preserve enum stability)
	section("Editor");
	Bool(SHOW_MISSING_ITEMS_WARNING, false);
	Int(FLOOR_VISIBILITY_MODE, 0);
	section("UI");
	String(PALETTE_DYNAMIC_STYLE, "32x32 px");
	Int(PALETTE_SORT_KEY, 0);
	Int(PALETTE_SORT_DIR, 0);
	Bool(PALETTE_HAS_SORT, false);
	Bool(PALETTE_SHOW_LABELS, false);
	Int(PALETTE_TILE_SIZE, 32);
	Bool(PALETTE_FILTER_ALL, false);
	String(PALETTE_FILTER_QUERY, "");

	if (mode == LOAD) {
		const bool hasExplicitTileSize = cur_sec && cur_sec->contains("palette_tile_size");
		std::string style = getString(PALETTE_DYNAMIC_STYLE);
		int tileSize = getInteger(PALETTE_TILE_SIZE);
		if (!hasExplicitTileSize) {
			if (style == "64x64 px") {
				setInteger(PALETTE_TILE_SIZE, 64);
			} else if (style == "128x128 px") {
				setInteger(PALETTE_TILE_SIZE, 128);
			} else if (style == "32x32 px" || style == "small icons" || style == "large icons") {
				setInteger(PALETTE_TILE_SIZE, 32);
				setString(PALETTE_DYNAMIC_STYLE, "32x32 px");
			} else {
				setInteger(PALETTE_TILE_SIZE, 32);
			}
		} else if (tileSize == 32 || tileSize == 64 || tileSize == 128) {
			setString(PALETTE_DYNAMIC_STYLE, std::to_string(tileSize) + "x" + std::to_string(tileSize) + " px");
		}
	}

	if (mode == SAVE) {
		int tileSize = getInteger(PALETTE_TILE_SIZE);
		if (tileSize == 32 || tileSize == 64 || tileSize == 128) {
			setString(PALETTE_DYNAMIC_STYLE, std::to_string(tileSize) + "x" + std::to_string(tileSize) + " px");
			cur_sec->insert_or_assign("palette_dynamic_style", getString(PALETTE_DYNAMIC_STYLE));
		}
		std::ofstream file("config.toml");
		if (file.is_open()) {
			file << g_settings_table;
			if (file.fail()) {
				spdlog::error("Failed to write to config.toml");
			}
			file.close();
		} else {
			spdlog::error("Failed to open config.toml for writing");
		}
	}

#undef section
#undef Bool
#undef Int
#undef IntToSave
#undef Float
#undef String
}

void Settings::load() {
	IO(LOAD);
}

void Settings::save(bool endoftheworld) {
	IO(SAVE);
}
