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
#include "lua_api_app.h"
#include "lua_script_manager.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "map/map.h"
#include "brushes/brush.h"
#include "editor/action.h"
#include "map/tile.h"
#include "editor/selection.h"
#include "item_definitions/core/item_definition_store.h"
#include "game/complexitem.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/ground/auto_border.h"
#include "util/file_system.h"

#include <wx/msgdlg.h>
#include <wx/app.h>
#include <wx/clipbrd.h>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>
#include <cstdio>
#include <thread>
#include <chrono>

namespace LuaAPI {

// ============================================================================
// Transaction System for Undo/Redo Support
// ============================================================================

// Transaction implementation
#include "game/house.h"

	// Helper to sync map metadata when swapping tiles
	static void updateTileMetadata(Editor* editor, Tile* tile, bool adding) {
		if (!editor || !tile) {
			return;
		}
		Map* map = editor->getMap();
		if (!map) {
			return;
		}

		if (adding) {
			if (tile->spawn) {
				map->addSpawn(tile);
			}
			if (tile->getHouseID()) {
				House* h = map->houses.getHouse(tile->getHouseID());
				if (h) {
					h->addTile(tile);
				}
			}
		} else {
			if (tile->spawn) {
				map->removeSpawn(tile);
			}
			if (tile->getHouseID()) {
				House* h = map->houses.getHouse(tile->getHouseID());
				if (h) {
					h->removeTile(tile);
				}
			}
			// Also clean up selection if removing
			if (tile->isSelected()) {
				// We use internal session to avoid creating undo actions for this cleanup
				editor->selection.start(Selection::INTERNAL);
				editor->selection.removeInternal(tile);
				editor->selection.finish(Selection::INTERNAL);
			}
		}
	}

	class LuaTransaction {
		bool active;
		Editor* editor;
		std::unique_ptr<BatchAction> batch;
		std::unique_ptr<Action> action;
		std::unordered_map<uint64_t, std::unique_ptr<Tile>> originalTiles;

		uint64_t positionKey(const Position& pos) const {
			return (static_cast<uint64_t>(pos.x) << 32) | (static_cast<uint64_t>(pos.y) << 16) | static_cast<uint64_t>(pos.z);
		}

		Position positionFromKey(uint64_t key) const {
			Position pos;
			pos.x = static_cast<int>(static_cast<int32_t>(key >> 32));
			pos.y = static_cast<int>(static_cast<uint16_t>((key >> 16) & 0xFFFFu));
			pos.z = static_cast<int>(static_cast<uint16_t>(key & 0xFFFFu));
			return pos;
		}

	public:
		static LuaTransaction& getInstance() {
			static LuaTransaction instance;
			return instance;
		}

		LuaTransaction() :
			active(false), editor(nullptr), batch(nullptr), action(nullptr) { }

		void begin(Editor* ed, const std::string& name = "") {
			if (active) {
				throw sol::error("Transaction already in progress");
			}
 
			editor = ed;
			if (!editor || !editor->actionQueue) {
				throw sol::error("No editor or action queue available");
			}
 
			active = true;
			batch = editor->actionQueue->createBatch(ACTION_LUA_SCRIPT);
			if (!name.empty()) {
				batch->setLabel(name);
			}
			action = editor->actionQueue->createAction(ACTION_LUA_SCRIPT);
			originalTiles.clear();
		}

		void commit() {
			if (!active) {
				return;
			}

			// Process each modified tile
			for (auto& pair : originalTiles) {
				const uint64_t key = pair.first;
				std::unique_ptr<Tile>& originalTile = pair.second;
				Position pos = positionFromKey(key);

				// Swap the original back into the map, or remove the tile entirely if it did not exist before
				std::unique_ptr<Tile> modifiedTile = editor->getMap()->swapTile(
					pos,
					originalTile ? std::move(originalTile) : std::unique_ptr<Tile>()
				);

				// Swapped-out state should be cleaned up from metadata and selection
				if (modifiedTile) {
					updateTileMetadata(editor, modifiedTile.get(), false);
				}

				// Add the current map tile back to metadata if one now exists
				Tile* tileInMap = editor->getMap()->getTile(pos);
				if (tileInMap) {
					updateTileMetadata(editor, tileInMap, true);
				}

				// Create Change with the actual modified tile object so pointer identity is preserved
				action->addChange(std::make_unique<Change>(std::move(modifiedTile), pos));
			}

			// Clear - ownership has been transferred or tiles discarded
			originalTiles.clear();

			if (action->size() > 0) {
				batch->addAndCommitAction(std::move(action));
				editor->addBatch(std::move(batch));
				editor->getMap()->doChange();
				g_gui.RefreshView(); // Force redraw immediately
			} else {
				// No changes, clean up
				action.reset();
				batch.reset();
			}

			cleanup();
		}

		void rollback() {
			if (!active) {
				return;
			}

			// Restore original tiles (discard any changes made)
			for (auto& pair : originalTiles) {
				const uint64_t key = pair.first;
				std::unique_ptr<Tile>& originalTile = pair.second;
				Position pos = positionFromKey(key);

				std::unique_ptr<Tile> modifiedTile = originalTile ? editor->getMap()->swapTile(pos, std::move(originalTile)) : editor->getMap()->swapTile(pos, std::unique_ptr<Tile>());

				// Clean up modified tile
				if (modifiedTile) {
					updateTileMetadata(editor, modifiedTile.get(), false);
				}

				// Restore original tile metadata if one exists
				Tile* tileInMap = editor->getMap()->getTile(pos);
				if (tileInMap) {
					updateTileMetadata(editor, tileInMap, true);
				}
			}
			originalTiles.clear();

			// Discard without committing
			action.reset();
			batch.reset();

			cleanup();
		}

		void markTileModified(Tile* tile, bool originallyExisted) {
			if (!active || !tile) {
				return;
			}

			Position pos = tile->getPosition();
			uint64_t key = positionKey(pos);

			// Only snapshot the tile once per transaction (first time it's modified)
			if (originalTiles.find(key) == originalTiles.end()) {
				// Preserve whether the tile originally existed so delete/create can undo cleanly
				originalTiles[key] = originallyExisted ? tile->deepCopy() : std::unique_ptr<Tile>();
			}
		}

		bool isActive() const {
			return active;
		}
		Editor* getEditor() const {
			return editor;
		}

	private:
		void cleanup() {
			active = false;
			editor = nullptr;
			batch.reset();
			action.reset();
		}
	};

	// Global accessor for tile modification tracking (used by lua_api_tile.cpp)
	void markTileForUndo(Tile* tile, bool originallyExisted) {
		if (LuaTransaction::getInstance().isActive()) {
			LuaTransaction::getInstance().markTileModified(tile, originallyExisted);
		}
	}

	// ============================================================================
	// Helper Functions
	// ============================================================================

	// Helper function to show alert dialog
	static int showAlert(sol::this_state ts, sol::object arg) {
		sol::state_view lua(ts);

		std::string title = "Script";
		std::string message;
		std::vector<std::string> buttons;

		// Handle different argument types
		if (arg.is<std::string>()) {
			message = arg.as<std::string>();
			buttons.push_back("OK");
		} else if (arg.is<sol::table>()) {
			sol::table opts = arg.as<sol::table>();

			if (opts["title"].valid()) {
				title = opts["title"].get<std::string>();
			}
			if (opts["text"].valid()) {
				message = opts["text"].get<std::string>();
			}
			if (opts["buttons"].valid()) {
				sol::table btns = opts["buttons"];
				for (size_t i = 1; i <= btns.size(); ++i) {
					if (btns[i].valid()) {
						buttons.push_back(btns[i].get<std::string>());
					}
				}
			}

			if (buttons.empty()) {
				buttons.push_back("OK");
			}
		} else {
			sol::function tostring = lua["tostring"];
			message = tostring(arg);
			buttons.push_back("OK");
		}

		// Determine dialog style based on buttons
		long style = wxCENTRE;
		if (buttons.size() == 1) {
			style |= wxOK;
		} else if (buttons.size() == 2) {
			std::string btn1 = buttons[0];
			std::string btn2 = buttons[1];
			std::transform(btn1.begin(), btn1.end(), btn1.begin(), ::tolower);
			std::transform(btn2.begin(), btn2.end(), btn2.begin(), ::tolower);

			if ((btn1 == "ok" && btn2 == "cancel") || (btn1 == "cancel" && btn2 == "ok")) {
				style |= wxOK | wxCANCEL;
			} else if ((btn1 == "yes" && btn2 == "no") || (btn1 == "no" && btn2 == "yes")) {
				style |= wxYES_NO;
			} else {
				style |= wxOK | wxCANCEL;
			}
		} else if (buttons.size() >= 3) {
			style |= wxYES_NO | wxCANCEL;
		}

		wxWindow* parent = g_gui.root;
		wxMessageDialog dlg(parent, wxString(message), wxString(title), style);

		int result = dlg.ShowModal();

		switch (result) {
			case wxID_OK:
			case wxID_YES:
				return 1;
			case wxID_NO:
				return 2;
			case wxID_CANCEL:
				return buttons.size() >= 3 ? 3 : 2;
			default:
				return 0;
		}
	}

	// Check if a map is currently open
	static bool hasMap() {
		Editor* editor = g_gui.GetCurrentEditor();
		return editor != nullptr && editor->getMap() != nullptr;
	}

	// Refresh the map view
	static void refresh() {
		g_gui.RefreshView();
	}

	// Get the current Map object
	static Map* getMap() {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return nullptr;
		}
		return editor->getMap();
	}

	// Get the current Selection object
	static Selection* getSelection() {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return nullptr;
		}
		return &editor->selection;
	}

	static void setClipboard(const std::string& text) {
		if (wxTheClipboard->Open()) {
			wxTheClipboard->SetData(new wxTextDataObject(text));
			wxTheClipboard->Close();
		}
	}

	// Transaction function with undo/redo support
	static void transaction(const std::string& name, sol::function func) {
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			throw sol::error("No map open");
		}

		LuaTransaction& trans = LuaTransaction::getInstance();

		try {
			trans.begin(editor, name);
			func();
			if (g_gui.GetCurrentEditor() == editor) {
				trans.commit();
			} else {
				trans.rollback();
				throw sol::error("Editor changed during transaction");
			}
			g_gui.RefreshView();
		} catch (const sol::error&) {
			trans.rollback();
			throw; // Re-throw to show error to user
		} catch (const std::exception& e) {
			trans.rollback();
			throw sol::error(std::string("Transaction failed: ") + e.what());
		} catch (...) {
			trans.rollback();
			throw sol::error("Transaction failed with unknown error");
		}
	}

	static sol::object getBorders(sol::this_state ts) {
		sol::state_view lua(ts);

		sol::table bordersTable = lua.create_table();

		for (auto& pair : g_brushes.getBorders()) {
			AutoBorder* border = pair.second.get();
			if (!border) {
				continue;
			}

			sol::table b = lua.create_table();
			b["id"] = border->id;
			b["group"] = border->group;
			// b["ground"] = border->ground; // Check if ground exists in AutoBorder

			sol::table tiles = lua.create_table();
			for (int i = 0; i < 13; ++i) {
				tiles[i + 1] = border->tiles[i];
			}
			b["tiles"] = tiles;

			bordersTable[border->id] = b;
		}

		return bordersTable;
	}

	static std::string getDataDirectory() {
		return FileSystem::GetDataDirectory().string();
	}

	static sol::table storageForScript(sol::this_state ts, const std::string& name) {
		sol::state_view lua(ts);
		sol::table storage = lua.create_table();

		namespace fs = std::filesystem;
		
		// Determine potential base directories
		fs::path scriptsPath = fs::weakly_canonical(LuaScriptManager::getInstance().getScriptsDirectory());
		fs::path dataPath = fs::weakly_canonical(FileSystem::GetDataDirectory());

		std::string scriptDir = lua["SCRIPT_DIR"].get_or(std::string(""));

		// Ensure scriptDir is safe and canonical
		fs::path canonicalScriptDir = fs::weakly_canonical(scriptDir);
		auto scriptDirRel = canonicalScriptDir.lexically_relative(scriptsPath);
		if (scriptDirRel.empty() || scriptDirRel.string().find("..") != std::string::npos) {
			scriptDir = scriptsPath.string(); 
		} else {
			scriptDir = canonicalScriptDir.string();
		}

		fs::path fullPath;
		bool allowed = false;

		try {
			fs::path p(name);
			if (p.is_absolute()) {
				fullPath = fs::weakly_canonical(p);
				// Check if absolute path is within allowed roots
				std::vector<fs::path> allowedRoots = { scriptsPath, dataPath };
				for (const auto& root : allowedRoots) {
					auto relative = fullPath.lexically_relative(root);
					if (!relative.empty() && relative.string().find("..") == std::string::npos) {
						allowed = true;
						break;
					}
				}
			} else {
				// For relative paths, try anchoring to each root (scriptDir first)
				std::vector<fs::path> roots = { fs::path(scriptDir), scriptsPath, dataPath };
				for (const auto& root : roots) {
					fs::path candidate = fs::weakly_canonical(root / p);
					auto relative = candidate.lexically_relative(root);
					if (!relative.empty() && relative.string().find("..") == std::string::npos) {
						fullPath = candidate;
						allowed = true;
						break;
					}
				}
			}
		} catch (...) {
			printf("[Lua Security] Failed to canonicalize path in app.storage: %s\n", name.c_str());
			return lua.create_table();
		}

		if (!allowed) {
			printf("[Lua Security] Blocked unsafe path in app.storage: %s\n", name.c_str());
			return lua.create_table();
		}

		std::string path = fullPath.string();
		storage["path"] = path;

		storage["load"] = [path](sol::this_state ts2, sol::object) -> sol::object {
			sol::state_view lua(ts2);
			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file.is_open()) {
				return sol::make_object(lua, sol::nil);
			}

			const std::streamsize fileSize = file.tellg();
			if (fileSize <= 0 || fileSize > static_cast<std::streamsize>(4 * 1024 * 1024)) {
				return sol::make_object(lua, sol::nil);
			}

			std::string content(static_cast<size_t>(fileSize), '\0');
			file.seekg(0, std::ios::beg);
			if (!file.read(content.data(), fileSize)) {
				return sol::make_object(lua, sol::nil);
			}

			sol::table json = lua["json"];
			if (!json.valid() || !json["decode"].valid()) {
				return sol::make_object(lua, sol::nil);
			}

			try {
				sol::function decode = json["decode"];
				sol::protected_function_result result = decode(content);
				if (!result.valid()) {
					return sol::make_object(lua, sol::nil);
				}
				sol::object decoded = result;
				return sol::make_object(lua, decoded);
			} catch (const sol::error&) {
				return sol::make_object(lua, sol::nil);
			}
		};

		storage["save"] = [path](sol::this_state ts2, sol::object first, sol::object second) -> bool {
			sol::state_view lua(ts2);
			std::string content;

			sol::object data = (second.valid() && !second.is<sol::nil_t>()) ? second : first;
			if (!data.valid() || data.is<sol::nil_t>()) {
				return false;
			}

			if (data.is<std::string>()) {
				content = data.as<std::string>();
			} else {
				sol::table json = lua["json"];
				if (!json.valid() || !json["encode_pretty"].valid()) {
					return false;
				}
				try {
					sol::function encode = json["encode_pretty"];
					sol::protected_function_result result = encode(data);
					if (!result.valid()) {
						return false;
					}
					content = result.get<std::string>();
				} catch (const sol::error&) {
					return false;
				}
			}

			std::ofstream file(path, std::ios::trunc);
			if (!file.is_open()) {
				return false;
			}
			file << content;
			file.close();
			return true;
		};

		storage["clear"] = [path](sol::object) -> bool {
			return std::remove(path.c_str()) == 0;
		};

		return storage;
	}

	// ============================================================================
	// Register App API
	// ============================================================================

	void registerApp(sol::state& lua) {
		// Create the 'app' table
		sol::table app = lua.create_named_table("app");

		// Version info
		app["version"] = __RME_VERSION__;
		app["apiVersion"] = 1;

		// Functions
		app["alert"] = showAlert;
		app["hasMap"] = hasMap;
		app["refresh"] = refresh;
		app["setBrush"] = [](const std::string& name) {
			Brush* b = g_brushes.getBrush(name);
			if (b) {
				g_gui.SelectBrush(b);
			}
		};
		app["transaction"] = transaction;
		app["setClipboard"] = setClipboard;
		app["getDataDirectory"] = getDataDirectory;
		app["addContextMenu"] = [](sol::this_state ts, const std::string& label, sol::function callback) {
			g_luaScripts.registerContextMenuItem(label, callback, ts);
		};
		app["selectRaw"] = [](int itemId) {
			const auto view = g_item_definitions.get(static_cast<ServerItemId>(itemId));
			if (view && view.editorData().raw_brush) {
				g_gui.SelectBrush(view.editorData().raw_brush);
			}
		};

		app["setCameraPosition"] = [](int x, int y, int z) {
			g_gui.SetScreenCenterPosition(Position(x, y, z));
		};
		app["getCreatureSpriteMaxId"] = []() -> int {
			return static_cast<int>(g_gui.gfx.getCreatureSpriteMaxID());
		};
		app["storage"] = storageForScript;

		// Yield to process pending UI events (prevents UI freeze during long operations)
		app["yield"] = []() {
			if (wxTheApp && !LuaTransaction::getInstance().isActive()) {
				wxTheApp->Yield(true);
			}
		};

		// Sleep for a given number of milliseconds (use sparingly, blocks the UI)
		app["sleep"] = [](int milliseconds) {
			if (milliseconds > 0 && milliseconds <= 10000) { // Max 10 seconds
				wxMilliSleep(static_cast<unsigned long>(milliseconds));
			}
		};

		// Get elapsed time in milliseconds since application start (high precision timer)
		app["getTime"] = []() -> long {
			return g_gui.gfx.getElapsedTime();
		};

		// Event system: app.events:on("eventName", callback) / app.events:off(id)
		sol::table events = lua.create_table();
		events["on"] = [](sol::this_state ts, sol::table self, const std::string& eventName, sol::function callback) -> int {
			return g_luaScripts.addEventListener(eventName, callback, ts);
		};
		events["off"] = [](sol::this_state ts, sol::table self, int listenerId) -> bool {
			return g_luaScripts.removeEventListener(listenerId);
		};
		app["events"] = events;

		// Properties via metatable (for dynamic properties like 'map' and 'selection')
		sol::table mt = lua.create_table();
		mt[sol::meta_function::index] = [](sol::this_state ts, sol::table self, std::string key) -> sol::object {
			sol::state_view lua(ts);

			if (key == "map") {
				Map* map = getMap();
				if (map) {
					return sol::make_object(lua, map);
				}
				return sol::nil;
			} else if (key == "selection") {
				Selection* sel = getSelection();
				if (sel) {
					return sol::make_object(lua, sel);
				}
				return sol::nil;
			} else if (key == "borders") {
				return getBorders(ts);
			} else if (key == "editor") {
				Editor* editor = g_gui.GetCurrentEditor();
				if (editor) {
					return sol::make_object(lua, editor);
				}
				return sol::nil;
			} else if (key == "brush") {
				Brush* b = g_gui.GetCurrentBrush();
				if (b) {
					return sol::make_object(lua, b);
				}
				return sol::nil;
			} else if (key == "brushSize") {
				return sol::make_object(lua, g_gui.GetBrushSize());
			} else if (key == "brushShape") {
				return sol::make_object(lua, g_gui.GetBrushShape() == BRUSHSHAPE_CIRCLE ? "circle" : "square");
			} else if (key == "brushVariation") {
				return sol::make_object(lua, g_gui.GetBrushVariation());
			} else if (key == "spawnTime") {
				return sol::make_object(lua, g_gui.GetSpawnTime());
			}
			return sol::nil;
		};

		mt[sol::meta_function::new_index] = [](sol::this_state ts, sol::table self, std::string key, sol::object value) {
			if (key == "brushSize") {
				if (value.is<int>()) {
					g_gui.SetBrushSize(value.as<int>());
				}
			} else if (key == "brushShape") {
				if (value.is<std::string>()) {
					std::string s = value.as<std::string>();
					if (s == "circle") {
						g_gui.SetBrushShape(BRUSHSHAPE_CIRCLE);
					} else if (s == "square") {
						g_gui.SetBrushShape(BRUSHSHAPE_SQUARE);
					}
				}
			} else if (key == "brushVariation") {
				if (value.is<int>()) {
					g_gui.SetBrushVariation(value.as<int>());
				}
			} else if (key == "spawnTime") {
				if (value.is<int>()) {
					g_gui.SetSpawnTime(value.as<int>());
				}
			}
		};

		// Keyboard modification state
		sol::table keyboard = lua.create_table();
		keyboard["isCtrlDown"] = []() -> bool {
			return wxGetKeyState(WXK_CONTROL);
		};
		keyboard["isShiftDown"] = []() -> bool {
			return wxGetKeyState(WXK_SHIFT);
		};
		keyboard["isAltDown"] = []() -> bool {
			return wxGetKeyState(WXK_ALT);
		};
		app["keyboard"] = keyboard;

		// Clipboard / Edit operations
		app["copy"] = []() { g_gui.DoCopy(); };
		app["cut"] = []() { g_gui.DoCut(); };
		app["paste"] = []() { g_gui.DoPaste(); };

		// Map overlay system
		sol::table mapView = lua.create_table();
		mapView["addOverlay"] = [](sol::this_state ts, sol::variadic_args va) -> bool {
			if (va.size() == 2 && va[0].is<std::string>() && va[1].is<sol::table>()) {
				return g_luaScripts.addMapOverlay(va[0].as<std::string>(), va[1].as<sol::table>(), ts);
			}
			if (va.size() == 3 && va[1].is<std::string>() && va[2].is<sol::table>()) {
				return g_luaScripts.addMapOverlay(va[1].as<std::string>(), va[2].as<sol::table>(), ts);
			}
			return false;
		};
		mapView["removeOverlay"] = [](sol::this_state ts, sol::variadic_args va) -> bool {
			if (va.size() == 1 && va[0].is<std::string>()) {
				return g_luaScripts.removeMapOverlay(va[0].as<std::string>());
			}
			if (va.size() == 2 && va[1].is<std::string>()) {
				return g_luaScripts.removeMapOverlay(va[1].as<std::string>());
			}
			return false;
		};
		mapView["setEnabled"] = [](sol::this_state ts, sol::variadic_args va) -> bool {
			if (va.size() == 2 && va[0].is<std::string>() && va[1].is<bool>()) {
				return g_luaScripts.setMapOverlayEnabled(va[0].as<std::string>(), va[1].as<bool>());
			}
			if (va.size() == 3 && va[1].is<std::string>() && va[2].is<bool>()) {
				return g_luaScripts.setMapOverlayEnabled(va[1].as<std::string>(), va[2].as<bool>());
			}
			return false;
		};
		mapView["registerShow"] = [](sol::this_state ts, sol::variadic_args va) -> bool {
			std::string label;
			std::string overlayId;
			bool enabled = true;
			sol::function ontoggle;

			if (va.size() == 2 && va[0].is<std::string>() && va[1].is<std::string>()) {
				label = va[0].as<std::string>();
				overlayId = va[1].as<std::string>();
			} else if (va.size() >= 3) {
				if (va[0].is<sol::table>()) {
					if (va[1].is<std::string>()) {
						label = va[1].as<std::string>();
					}
					if (va[2].is<std::string>()) {
						overlayId = va[2].as<std::string>();
					}
				} else if (va[0].is<std::string>() && va[1].is<std::string>()) {
					label = va[0].as<std::string>();
					overlayId = va[1].as<std::string>();
				}
			}

			if (va.size() >= 3 && va[va.size() - 1].is<sol::table>()) {
				sol::table opts = va[va.size() - 1].as<sol::table>();
				enabled = opts.get_or(std::string("enabled"), enabled);
				if (opts["ontoggle"].valid()) {
					ontoggle = opts["ontoggle"];
				}
			} else if (va.size() >= 3 && va[va.size() - 1].is<bool>()) {
				enabled = va[va.size() - 1].as<bool>();
			}

			if (label.empty() || overlayId.empty()) {
				return false;
			}

			return g_luaScripts.registerMapOverlayShow(label, overlayId, enabled, ontoggle, ts);
		};
		app["mapView"] = mapView;

		app[sol::metatable_key] = mt;

		// Register Editor usertype for undo/redo functionality
		lua.new_usertype<Editor>(
			"Editor",
			sol::no_constructor,

			// Undo/Redo functions
			"undo", [](Editor* editor) {
			if (editor && editor->actionQueue && editor->actionQueue->canUndo()) {
				editor->actionQueue->undo();
				g_gui.RefreshView();
			} },
			"redo", [](Editor* editor) {
			if (editor && editor->actionQueue && editor->actionQueue->canRedo()) {
				editor->actionQueue->redo();
				g_gui.RefreshView();
			} },
			"canUndo", [](Editor* editor) -> bool { return editor && editor->actionQueue && editor->actionQueue->canUndo(); },
			"canRedo", [](Editor* editor) -> bool { return editor && editor->actionQueue && editor->actionQueue->canRedo(); },

			// History info
			"historyIndex", sol::property([](Editor* editor) -> int {
				if (editor && editor->actionQueue) {
					return (int)editor->actionQueue->getCurrentIndex();
				}
				return 0;
			}),
			"historySize", sol::property([](Editor* editor) -> int {
				if (editor && editor->actionQueue) {
					return (int)editor->actionQueue->getSize();
				}
				return 0;
			}),

			// Get history as a table
			"getHistory", [](Editor* editor, sol::this_state ts) -> sol::table {
			sol::state_view lua(ts);
			sol::table history = lua.create_table();

			if (editor && editor->actionQueue) {
				size_t size = editor->actionQueue->getSize();
				for (size_t i = 0; i < size; ++i) {
					sol::table input = lua.create_table();
					input["index"] = (int)(i + 1); // 1-based for Lua
					input["name"] = editor->actionQueue->getActionName(i);
					history[i + 1] = input;
				}
			}
			return history; },

			// Navigate to specific history index
			"goToHistory", [](Editor* editor, int targetIndex) {
			if (!editor || !editor->actionQueue) return;

			int current = (int)editor->actionQueue->getCurrentIndex();
			int target = targetIndex; // Already 1-based from Lua

			if (target < 0) target = 0;
			if (target > (int)editor->actionQueue->getSize()) {
				target = (int)editor->actionQueue->getSize();
			}

			int diff = target - current;

			if (diff > 0) {
				for (int i = 0; i < diff; ++i) {
					if (editor->actionQueue->canRedo()) {
						editor->actionQueue->redo();
					}
				}
			} else if (diff < 0) {
				for (int i = 0; i < -diff; ++i) {
					if (editor->actionQueue->canUndo()) {
						editor->actionQueue->undo();
					}
				}
			}
			g_gui.RefreshView(); }
		);
	}

} // namespace LuaAPI
