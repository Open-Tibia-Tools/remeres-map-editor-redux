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

#ifndef RME_WAYPOINTS_H_
#define RME_WAYPOINTS_H_

#include "map/position.h"

class Waypoint {
public:
	Waypoint() = default;
	Waypoint(std::string name, Position pos) : name(std::move(name)), pos(pos) { }

	std::string name;
	Position pos;
};

using WaypointMap = std::map<std::string, std::unique_ptr<Waypoint>>;

class Waypoints {
	Map& map;

public:
	Waypoints(Map& map) :
		map(map) { }
	~Waypoints() = default;

	void addWaypoint(std::unique_ptr<Waypoint> wp, bool replace = true);
	Waypoint* getWaypoint(std::string name);
	Waypoint* getWaypoint(TileLocation* location);
	const Waypoint* getWaypoint(const TileLocation* location) const;
	void removeWaypoint(std::string name);
	bool renameWaypoint(const std::string& old_name, const std::string& new_name);

	WaypointMap waypoints;

	WaypointMap::iterator begin() {
		return waypoints.begin();
	}
	WaypointMap::const_iterator begin() const {
		return waypoints.begin();
	}
	WaypointMap::iterator end() {
		return waypoints.end();
	}
	WaypointMap::const_iterator end() const {
		return waypoints.end();
	}

	size_t size() const {
		return waypoints.size();
	}

	bool empty() const {
		return waypoints.empty();
	}
};

#endif
