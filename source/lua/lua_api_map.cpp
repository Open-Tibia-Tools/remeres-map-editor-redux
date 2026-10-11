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
#include "lua_api_map.h"
#include "lua_api.h"
#include "map/map.h"
#include "map/basemap.h"
#include "map/tile.h"
#include "map/position.h"
#include "ui/gui.h"
#include "editor/editor.h"

namespace LuaAPI {

	// Custom iterator for Map tiles to use in Lua for-loops
	// Iterator for Map Tiles
	class LuaMapTileIterator {
	public:
		LuaMapTileIterator(Map* mapPtr) :
			map(mapPtr),
			mapGeneration(mapPtr ? mapPtr->getGeneration() : 0),
			currentIndex(0) {
			Editor* currentEditor = g_gui.GetCurrentEditor();
			if (currentEditor && currentEditor->getMap() == map && map && map->getGeneration() == mapGeneration) {
				for (auto it = map->begin(); it != map->end(); ++it) {
					TileLocation* loc = &(*it);
					if (loc && loc->get()) {
						positions.push_back(loc->get()->getPosition());
					}
				}
			}
		}

		std::tuple<sol::object, sol::object> next(sol::this_state ts) {
			sol::state_view lua(ts);

			Editor* currentEditor = g_gui.GetCurrentEditor();
			if (!currentEditor || currentEditor->getMap() != map || !map || map->getGeneration() != mapGeneration) {
				return std::make_tuple(sol::nil, sol::nil);
			}

			// Find next valid tile
			while (currentIndex < positions.size()) {
				Position pos = positions[currentIndex++];
				Tile* tile = map->getTile(pos);
				if (tile) {
					return std::make_tuple(
						sol::make_object(lua, tile),
						sol::make_object(lua, tile)
					);
				}
			}

			return std::make_tuple(sol::nil, sol::nil);
		}

	private:
		Map* map;
		uint64_t mapGeneration;
		std::vector<Position> positions;
		size_t currentIndex;
	};

	// Iterator for Spawns
	class LuaMapSpawnIterator {
	public:
		LuaMapSpawnIterator(Map* mapPtr) :
			map(mapPtr),
			mapGeneration(mapPtr ? mapPtr->getGeneration() : 0),
			currentIndex(0) {
			Editor* currentEditor = g_gui.GetCurrentEditor();
			if (currentEditor && currentEditor->getMap() == map && map && map->getGeneration() == mapGeneration) {
				for (const auto& pos : map->spawns) {
					positions.push_back(pos);
				}
			}
		}

		Tile* next() {
			Editor* currentEditor = g_gui.GetCurrentEditor();
			if (!currentEditor || currentEditor->getMap() != map || !map || map->getGeneration() != mapGeneration) {
				return nullptr;
			}

			while (currentIndex < positions.size()) {
				Position pos = positions[currentIndex++];
				Tile* tile = map->getTile(pos);
				if (tile) {
					return tile;
				}
			}
			return nullptr;
		}

	private:
		Map* map;
		uint64_t mapGeneration = 0;
		std::vector<Position> positions;
		size_t currentIndex;
	};
	void registerMap(sol::state& lua) {
		// Register the iterator type
		lua.new_usertype<LuaMapTileIterator>("MapTileIterator", sol::no_constructor, "next", &LuaMapTileIterator::next);

		// Register Spawn iterator
		lua.new_usertype<LuaMapSpawnIterator>("MapSpawnIterator", sol::no_constructor, "next", &LuaMapSpawnIterator::next);

		// Register Map usertype
		lua.new_usertype<Map>(
			"Map",
			// No public constructor - maps are obtained from app.map
			sol::no_constructor,

			// Properties (read-only)
			"name", sol::property(&Map::getName),
			"filename", sol::property(&Map::getFilename),
			"description", sol::property(&Map::getMapDescription),
			"width", sol::property(&Map::getWidth),
			"height", sol::property(&Map::getHeight),
			"houseFilename", sol::property(&Map::getHouseFilename),
			"spawnFilename", sol::property(&Map::getSpawnFilename),
			"monsterFilename", sol::property(&Map::getMonsterFilename),
			"npcFilename", sol::property(&Map::getNpcFilename),
			"hasFile", sol::property(&Map::hasFile),
			"hasChanged", sol::property(&Map::hasChanged),
			"tileCount", sol::property([](Map* map) -> uint64_t {
				return map ? map->getTileCount() : 0;
			}),

			// Get tile methods
			"getTile", sol::overload([](Map* map, int x, int y, int z) -> Tile* { return map ? map->getTile(x, y, z) : nullptr; }, [](Map* map, const Position& pos) -> Tile* { return map ? map->getTile(pos) : nullptr; }),

			// Get or create tile (for adding content to empty positions)
			"getOrCreateTile", [](Map* map, sol::variadic_args va) -> Tile* {
				if (!map) {
					return nullptr;
				}

				Position pos;
				if (va.size() == 1 && va[0].is<Position>()) {
					pos = va[0].as<Position>();
				} else if (va.size() == 3) {
					int x = va[0].as<int>();
					int y = va[1].as<int>();
					int z = va[2].as<int>();
					
					// Pre-validate before assigning to potentially 16-bit unsigned fields in Position
					if (x < 0 || x > 65535 || y < 0 || y > 65535 || z < 0 || z > 15) {
						throw sol::error("getOrCreateTile: Invalid coordinates (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
					}
					
					pos.x = x;
					pos.y = y;
					pos.z = z;
				} else {
					throw sol::error("getOrCreateTile expects (x, y, z) or (Position)");
				}

				if (!pos.isValid()) {
					throw sol::error("getOrCreateTile: Invalid coordinates (" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")");
				}

				Tile* existing = map->getTile(pos);
				Tile* tile = map->getOrCreateTile(pos);
				if (!existing) {
					markTileForUndo(tile, false);
				}
				return tile;
			},

			// Tiles iterator - allows: for tile in map.tiles do ... end
			"tiles", sol::property([](Map* map, sol::this_state ts) {
				sol::state_view lua(ts);

				// Return an iterator function
				auto iterator = std::make_shared<LuaMapTileIterator>(map);

				// Return the iterator function that Lua will call repeatedly
				return sol::make_object(lua, [iterator](sol::this_state ts) {
					return iterator->next(ts);
				});
			}),

			// Spawns iterator - allows: for tile in map.spawns do ... end
			"spawns", sol::property([](Map* map, sol::this_state ts) {
				sol::state_view lua(ts);

				auto iterator = std::make_shared<LuaMapSpawnIterator>(map);

				return sol::make_object(lua, [iterator]() -> Tile* {
					return iterator->next();
				});
			}),

			// String representation
			sol::meta_function::to_string, [](Map* map) {
			if (!map){ return std::string("Map(invalid)");
}
			return "Map(\"" + map->getName() + "\", " +
				   std::to_string(map->getWidth()) + "x" +
				   std::to_string(map->getHeight()) + ")"; }
		);
	}

} // namespace LuaAPI
