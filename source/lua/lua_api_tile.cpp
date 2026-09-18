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
#include "lua_api_tile.h"
#include "lua_api.h"
#include <algorithm>
#include <iterator>
#include <limits>
#include "map/tile.h"
#include "map/tile_operations.h"
#include "game/item.h"
#include "game/creature.h"
#include "game/spawn.h"
#include "brushes/brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/door/door_brush.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "map/map.h"

namespace LuaAPI {

	static sol::table makePositionTable(sol::this_state ts, const Position& pos) {
		sol::state_view lua(ts);
		sol::table tbl = lua.create_table();
		tbl["x"] = pos.x;
		tbl["y"] = pos.y;
		tbl["z"] = pos.z;
		return tbl;
	}

	// Helper to get items as a Lua table
	static sol::table getTileItems(Tile* tile, sol::this_state ts) {
		sol::state_view lua(ts);
		sol::table items = lua.create_table();

		if (!tile) {
			return items;
		}

		int idx = 1;
		for (const auto& item : tile->items) {
			if (item) {
				items[idx++] = item.get();
			}
		}
		return items;
	}

	// Add item to tile
	static Item* addItemToTile(Tile* tile, int itemId, sol::optional<int> countOpt) {
		if (!tile) {
			throw sol::error("Invalid tile");
		}

		if (itemId < 0) {
			throw sol::error("addItemToTile: itemId must be non-negative");
		}
		if (countOpt && (*countOpt < 0 || *countOpt > 65535)) {
			throw sol::error("addItemToTile: count must be between 0 and 65535");
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		std::unique_ptr<Item> item = Item::Create(static_cast<ServerItemId>(itemId));
		if (!item) {
			throw sol::error("Failed to create item with id " + std::to_string(itemId));
		}

		if (countOpt) {
			item->setSubtype(static_cast<uint16_t>(*countOpt));
		}

		Item* ptr = item.get();
		tile->addItem(std::move(item));
		tile->modify();

		return ptr;
	}

	// Remove item from tile
	static bool removeItemFromTile(Tile* tile, Item* itemToRemove) {
		if (!tile || !itemToRemove) {
			return false;
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		// Find and remove the item
		for (auto it = tile->items.begin(); it != tile->items.end(); ++it) {
			if (it->get() == itemToRemove) {
				tile->items.erase(it);
				tile->modify();
				return true;
			}
		}

		// Check if it's the ground
		if (tile->ground.get() == itemToRemove) {
			tile->ground.reset();
			tile->modify();
			return true;
		}

		return false;
	}

	// Set creature on tile
	static Creature* setTileCreature(Tile* tile, const std::string& creatureName, sol::optional<int> spawnTimeOpt, sol::optional<int> directionOpt) {
		if (!tile) {
			throw sol::error("Invalid tile");
		}

		// Check if creature type exists
		CreatureType* type = g_creatures[creatureName];
		if (!type) {
			throw sol::error("Unknown creature type: " + creatureName);
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		// Create new creature
		tile->creature = std::make_unique<Creature>(creatureName);

		// Set spawn time (default to global setting or 60s)
		int spawnTime = spawnTimeOpt.value_or(g_gui.GetSpawnTime());
		tile->creature->setSpawnTime(spawnTime);

		// Set direction (default SOUTH)
		Direction dir = static_cast<Direction>(directionOpt.value_or(SOUTH));
		if (dir >= DIRECTION_FIRST && dir <= DIRECTION_LAST) {
			tile->creature->setDirection(dir);
		}

		tile->modify();
		return tile->creature.get();
	}

	// Remove creature from tile
	static bool removeTileCreature(Tile* tile) {
		if (!tile || !tile->creature) {
			return false;
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		tile->creature.reset();
		tile->modify();
		return true;
	}

	// Set spawn on tile
	static Spawn* setTileSpawn(Tile* tile, sol::optional<int> sizeOpt) {
		if (!tile) {
			throw sol::error("Invalid tile");
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		// Get map instance
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			throw sol::error("No active editor");
		}
		Map& map = editor->map;

		// Remove existing spawn from map metadata
		if (tile->spawn) {
			map.removeSpawn(tile);
		}

		// Create new spawn with given size (default 3)
		int size = sizeOpt.value_or(3);
		if (size < 1) {
			size = 1;
		}
		if (size > 50) {
			size = 50;
		}

		tile->spawn = std::make_unique<Spawn>(size);

		// Register new spawn with map
		map.addSpawn(tile);

		tile->modify();
		return tile->spawn.get();
	}

	// Remove spawn from tile
	static bool removeTileSpawn(Tile* tile) {
		if (!tile || !tile->spawn) {
			return false;
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		// Get map instance
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			throw sol::error("No active editor");
		}
		Map& map = editor->map;

		// Remove spawn from map metadata
		map.removeSpawn(tile);

		tile->spawn.reset();
		tile->modify();
		return true;
	}

	// Set ground item
	static void setTileGround(Tile* tile, sol::object groundObj) {
		if (!tile) {
			throw sol::error("Invalid tile");
		}

		if (groundObj.is<int>()) {
			int groundId = groundObj.as<int>();
			if (groundId < 1 || groundId > std::numeric_limits<uint16_t>::max()) {
				throw sol::error("setTileGround: ground item id must be between 1 and 65535");
			}
			std::unique_ptr<Item> ground = Item::Create(static_cast<uint16_t>(groundId));
			if (!ground) {
				throw sol::error("setTileGround: failed to create ground item " + std::to_string(groundId));
			}
			markTileForUndo(tile);
			tile->ground = std::move(ground);
			tile->modify();
			return;
		} else if (groundObj.is<Item*>()) {
			Item* item = groundObj.as<Item*>();
			if (!item) {
				throw sol::error("setTileGround: invalid ground item");
			}
			if (!item->getGroundBrush()) {
				throw sol::error("setTileGround: item is not a ground item");
			}
			markTileForUndo(tile);
			tile->ground = item->deepCopy();
			tile->modify();
			return;
		} else if (groundObj.is<sol::nil_t>()) {
			markTileForUndo(tile);
			tile->ground.reset();
			tile->modify();
			return;
		}

		throw sol::error("setTileGround: expected item id, ground item, or nil");
	}

	// Set house ID
	static void setTileHouseId(Tile* tile, uint32_t houseId) {
		if (!tile) {
			return;
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		tile->setHouseID(houseId);
		tile->modify();
	}

	// Apply a brush to a tile (with optional auto-bordering)
	static bool applyBrushToTile(Tile* tile, const std::string& brushName, sol::optional<bool> autoBorder) {
		if (!tile) {
			return false;
		}

		Brush* brush = g_brushes.getBrush(brushName);
		if (!brush) {
			return false;
		}

		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return false;
		}

		markTileForUndo(tile);

		bool success = false;

		// Apply the brush based on its type
		if (brush->is<GroundBrush>()) {
			GroundBrush* groundBrush = brush->as<GroundBrush>();
			if (groundBrush) {
				groundBrush->draw(&editor->map, tile, nullptr);
				success = true;
			}
		} else if (brush->is<DoodadBrush>()) {
			DoodadBrush* doodadBrush = brush->as<DoodadBrush>();
			if (doodadBrush) {
				doodadBrush->draw(&editor->map, tile, nullptr);
				success = true;
			}
		} else if (brush->is<WallBrush>()) {
			WallBrush* wallBrush = brush->as<WallBrush>();
			if (wallBrush) {
				wallBrush->draw(&editor->map, tile, nullptr);
				success = true;
			}
		} else if (brush->is<DoorBrush>()) {
			DoorBrush* doorBrush = brush->as<DoorBrush>();
			if (doorBrush) {
				doorBrush->draw(&editor->map, tile, nullptr);
				success = true;
			}
		}

		if (success) {
			// Apply auto-bordering if requested (default: true for ground brushes)
			bool doBorder = autoBorder.value_or(brush->is<GroundBrush>());
			if (doBorder) {
				TileOperations::borderize(tile, &editor->map);
			}
			tile->modify();
		}

		return success;
	}

	void registerTile(sol::state& lua) {
		// Register Tile usertype
		lua.new_usertype<Tile>(
			"Tile",
			// No public constructor - tiles are obtained from the map
			sol::no_constructor,

			// Position properties (read-only)
			"position", sol::property([](Tile* tile, sol::this_state ts) -> sol::table {
				return makePositionTable(ts, tile ? tile->getPosition() : Position());
			}),
			"x", sol::property([](Tile* tile) { return tile ? tile->getX() : 0; }),
			"y", sol::property([](Tile* tile) { return tile ? tile->getY() : 0; }),
			"z", sol::property([](Tile* tile) { return tile ? tile->getZ() : 0; }),

			// Ground (read/write)
			"ground", sol::property([](Tile* tile) -> Item* { return tile ? tile->ground.get() : nullptr; }, setTileGround),
			"hasGround", sol::property([](Tile* tile) { return tile && tile->hasGround(); }),

			// Items collection (read-only - use addItem/removeItem to modify)
			"items", sol::property(getTileItems),
			"itemCount", sol::property([](Tile* tile) { return tile ? static_cast<int>(tile->items.size()) : 0; }),

			// House
			"houseId", sol::property([](Tile* tile) -> uint32_t { return tile ? tile->getHouseID() : 0; }, setTileHouseId),
			"isHouseTile", sol::property([](Tile* tile) { return tile && tile->isHouseTile(); }),
			"isHouseExit", sol::property([](Tile* tile) { return tile && tile->isHouseExit(); }),

			// Flags (read-only for now, some could be made writable)
			"isPZ", sol::property([](Tile* tile) { return tile && tile->isPZ(); }),
			"isBlocking", sol::property([](Tile* tile) { return tile && tile->isBlocking(); }),
			"hasBorders", sol::property([](Tile* tile) { return tile && tile->hasBorders(); }),
			"hasWall", sol::property([](Tile* tile) { return tile && tile->hasWall(); }),
			"hasTable", sol::property([](Tile* tile) { return tile && tile->hasTable(); }),
			"hasCarpet", sol::property([](Tile* tile) { return tile && tile->hasCarpet(); }),
			"groundZOrder", sol::property([](Tile* tile) -> int {
				if (!tile) {
					return 0;
				}
				GroundBrush* brush = tile->getGroundBrush();
				return brush ? brush->getZ() : 0;
			}),
			"isNoPvp", sol::property([](Tile* tile) { return tile && (tile->getMapFlags() & TILESTATE_NOPVP); }),
			"isNoLogout", sol::property([](Tile* tile) { return tile && (tile->getMapFlags() & TILESTATE_NOLOGOUT); }),
			"isPvpZone", sol::property([](Tile* tile) { return tile && (tile->getMapFlags() & TILESTATE_PVPZONE); }),

			// Map flags
			"mapFlags", sol::property([](Tile* tile) -> uint32_t { return tile ? tile->getMapFlags() : 0; }, [](Tile* tile, uint32_t flags) {
					if (tile) {
						markTileForUndo(tile);
						tile->setMapFlags(flags);
						tile->modify();
					} }),

			// Selection
			"isSelected", sol::property([](Tile* tile) { return tile && tile->isSelected(); }),
			"select", [](Tile* tile) { if (tile) tile->select(); },
			"deselect", [](Tile* tile) { if (tile) tile->deselect(); },

			// Creature and Spawn (read-only access, use methods to modify)
			"creature", sol::property([](Tile* tile) -> Creature* { return tile ? tile->creature.get() : nullptr; }),
			"spawn", sol::property([](Tile* tile) -> Spawn* { return tile ? tile->spawn.get() : nullptr; }),
			"hasCreature", sol::property([](Tile* tile) { return tile && tile->creature != nullptr; }),
			"hasSpawn", sol::property([](Tile* tile) { return tile && tile->spawn != nullptr; }),

			// Creature methods
			"setCreature", sol::overload([](Tile* tile, const std::string& name) -> Creature* { return setTileCreature(tile, name, sol::nullopt, sol::nullopt); }, [](Tile* tile, const std::string& name, int spawnTime) -> Creature* { return setTileCreature(tile, name, spawnTime, sol::nullopt); }, [](Tile* tile, const std::string& name, int spawnTime, int direction) -> Creature* { return setTileCreature(tile, name, spawnTime, direction); }),
			"removeCreature", removeTileCreature,

			// Spawn methods
			"setSpawn", sol::overload([](Tile* tile) -> Spawn* { return setTileSpawn(tile, sol::nullopt); }, [](Tile* tile, int size) -> Spawn* { return setTileSpawn(tile, size); }),
			"removeSpawn", removeTileSpawn,

			// Methods
			"addItem", sol::overload([](Tile* tile, int itemId) -> Item* { return addItemToTile(tile, itemId, sol::nullopt); }, [](Tile* tile, int itemId, int count) -> Item* { return addItemToTile(tile, itemId, count); }),
			"removeItem", removeItemFromTile,
			"applyBrush", applyBrushToTile,
			"borderize", [](Tile* tile) {
				if (!tile) return;
				Editor* editor = g_gui.GetCurrentEditor();
				if (!editor) return;
				markTileForUndo(tile);
				TileOperations::borderize(tile, &editor->map);
				tile->modify(); },
			"wallize", [](Tile* tile) {
				if (!tile) return;
				Editor* editor = g_gui.GetCurrentEditor();
				if (!editor) return;
				markTileForUndo(tile);
				TileOperations::wallize(tile, &editor->map);
				tile->modify(); },
			"moveItem", sol::overload(
							// Index-based move within same tile
							// Semantics: Move item at fromIdx so it ends up at toIdx position
							[](Tile* tile, int fromIdx, int toIdx) {
								if (!tile) {
									return;
								}

								int from = fromIdx - 1;
								int to = toIdx - 1;
								int size = (int)tile->items.size();

								if (from < 0 || from >= size || to < 0 || to >= size || from == to) {
									return;
								}

								markTileForUndo(tile);

								std::unique_ptr<Item> item = std::move(tile->items[from]);
								tile->items.erase(tile->items.begin() + from);

								int insertPos = std::clamp(to, 0, (int)tile->items.size());

								tile->items.insert(tile->items.begin() + insertPos, std::move(item));
								tile->modify();
							},
							// Move specific item object to new index in same tile
							[](Tile* tile, Item* item, int toIdx) {
								if (!tile || !item) {
									return;
								}
								auto it = std::find_if(tile->items.begin(), tile->items.end(), [item](const std::unique_ptr<Item>& i) { return i.get() == item; });
								if (it == tile->items.end()) {
									return;
								}

								int from = (int)std::distance(tile->items.begin(), it);
								int to = std::clamp(toIdx - 1, 0, (int)tile->items.size() - 1);
								if (from == to) {
									return;
								}

								markTileForUndo(tile);
								std::unique_ptr<Item> movedItem = std::move(*it);
								tile->items.erase(it);

								int insertPos = std::clamp(to, 0, (int)tile->items.size());
								tile->items.insert(tile->items.begin() + insertPos, std::move(movedItem));
								tile->modify();
							},
							// Move item to DIFFERENT tile
							[](Tile* sourceTile, Item* item, Tile* destTile, sol::optional<int> toIdx) {
								if (!sourceTile || !item || !destTile) {
									return;
								}
								auto it = std::find_if(sourceTile->items.begin(), sourceTile->items.end(), [item](const std::unique_ptr<Item>& i) { return i.get() == item; });
								if (it == sourceTile->items.end()) {
									return;
								}

								markTileForUndo(sourceTile);
								markTileForUndo(destTile);

								std::unique_ptr<Item> movedItem = std::move(*it);
								sourceTile->items.erase(it);
								int to = toIdx ? std::clamp(*toIdx - 1, 0, (int)destTile->items.size()) : (int)destTile->items.size();
								destTile->items.insert(destTile->items.begin() + to, std::move(movedItem));

								sourceTile->modify();
								destTile->modify();
							}
						),

			"getPosition", [](Tile* tile, sol::this_state ts) -> sol::table {
				return makePositionTable(ts, tile ? tile->getPosition() : Position());
			},

			"getItemAt", [](Tile* tile, int index) -> Item* {
				if (!tile) return nullptr;
				// Lua uses 1-based indexing
				return tile->getItemAt(index - 1); },

			"getTopItem", [](Tile* tile) -> Item* { return tile ? tile->getTopItem() : nullptr; },

			"getWall", [](Tile* tile) -> Item* { return tile ? tile->getWall() : nullptr; },

			"getTable", [](Tile* tile) -> Item* { return tile ? tile->getTable() : nullptr; },

			"getCarpet", [](Tile* tile) -> Item* { return tile ? tile->getCarpet() : nullptr; },

			// String representation
			sol::meta_function::to_string, [](Tile* tile) {
				if (!tile) return std::string("Tile(invalid)");
				Position pos = tile->getPosition();
				return "Tile(" + std::to_string(pos.x) + ", " +
					std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")"; }
		);
	}
} // namespace LuaAPI
