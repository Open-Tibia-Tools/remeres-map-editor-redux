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
#include "lua_api.h"
#include "map/map.h"
#include "brushes/brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/waypoint/waypoint_brush.h"

namespace LuaAPI {

	static std::string getBrushType(Brush* brush) {
		if (!brush) {
			return "none";
		}

		if (brush->is<RAWBrush>()) {
			return "raw";
		}
		if (brush->is<DoodadBrush>()) {
			return "doodad";
		}
		if (brush->is<GroundBrush>()) {
			return "ground";
		}
		if (brush->is<WallBrush>()) {
			return "wall";
		}
		if (brush->is<TableBrush>()) {
			return "table";
		}
		if (brush->is<CarpetBrush>()) {
			return "carpet";
		}
		if (brush->is<DoorBrush>()) {
			return "door";
		}
		if (brush->is<CreatureBrush>()) {
			return "creature";
		}
		if (brush->is<SpawnBrush>()) {
			return "spawn";
		}
		if (brush->is<HouseBrush>()) {
			return "house";
		}
		if (brush->is<HouseExitBrush>()) {
			return "house_exit";
		}
		if (brush->is<WaypointBrush>()) {
			return "waypoint";
		}
		if (brush->is<EraserBrush>()) {
			return "eraser";
		}
		if (brush->is<TerrainBrush>()) {
			return "terrain";
		}

		return "unknown";
	}

	void registerBrush(sol::state& lua) {
		// Register Brush usertype (read-only)
		lua.new_usertype<Brush>("Brush", sol::no_constructor,

								// Properties (read-only)
								"id", sol::property([](Brush* b) -> uint32_t {
									return b ? b->getID() : 0;
								}),
								"name", sol::property([](Brush* b) -> std::string {
									return b ? b->getName() : "";
								}),
								"lookId", sol::property([](Brush* b) -> int {
									return b ? b->getLookID() : 0;
								}),
								"type", sol::property([](Brush* b) -> std::string {
									return getBrushType(b);
								}),

								// Methods
								"canDraw", [](Brush* b, Map* map, int x, int y, int z) -> bool {
									return b && map && b->canDraw(static_cast<BaseMap*>(map), Position(x, y, z));
								},
								"needBorders", [](Brush* b) -> bool {
									return b && b->needBorders();
								},
								"canDrag", [](Brush* b) -> bool {
									return b && b->canDrag();
								},
								"canSmear", [](Brush* b) -> bool {
									return b && b->canSmear();
								}
		);

		// Global brush table
		sol::table brushes = lua.create_table();

		brushes["get"] = [](sol::object nameObj) -> Brush* {
			if (!nameObj.valid() || nameObj.is<sol::nil_t>() || !nameObj.is<std::string>()) {
				return nullptr;
			}
			std::string name = nameObj.as<std::string>();
			if (name.empty()) {
				return nullptr;
			}
			return g_brushes.getBrush(name);
		};

		brushes["getNames"] = [](sol::this_state ts) -> sol::table {
			sol::state_view lua(ts);
			sol::table list = lua.create_table();
			int index = 1;
			for (const auto& pair : g_brushes.getMap()) {
				list[index++] = pair.first;
			}
			return list;
		};

		brushes["count"] = []() -> size_t {
			return g_brushes.getMap().size();
		};

		lua["Brushes"] = brushes;
	}

} // namespace LuaAPI
