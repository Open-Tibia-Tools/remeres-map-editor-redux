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

#include "app/definitions.h"

#include "game/waypoints.h"
#include "map/map.h"

void Waypoints::addWaypoint(std::unique_ptr<Waypoint> wp, bool replace) {
	if (getWaypoint(wp->name) && !replace) {
		return;
	}
	removeWaypoint(wp->name);
	if (wp->pos != Position()) {
		Tile* t = map.getTile(wp->pos);
		if (!t) {
			t = map.createTile(wp->pos.x, wp->pos.y, wp->pos.z);
		}
		t->getLocation()->increaseWaypointCount();
	}
	waypoints.insert(std::make_pair(as_lower_str(wp->name), std::move(wp)));
}

Waypoint* Waypoints::getWaypoint(std::string name) {
	to_lower_str(name);
	WaypointMap::iterator iter = waypoints.find(name);
	if (iter == waypoints.end()) {
		return nullptr;
	}
	return iter->second.get();
}

Waypoint* Waypoints::getWaypoint(TileLocation* location) {
	if (!location) {
		return nullptr;
	}
	return const_cast<Waypoint*>(std::as_const(*this).getWaypoint(location));
}

const Waypoint* Waypoints::getWaypoint(const TileLocation* location) const {
	if (!location) {
		return nullptr;
	}

	const auto matches_location = [location](const auto& entry) {
		const Waypoint* waypoint = entry.second.get();
		return waypoint && waypoint->pos == location->position;
	};

	const auto it = std::ranges::find_if(waypoints, matches_location);
	return it != waypoints.end() ? it->second.get() : nullptr;
}

void Waypoints::removeWaypoint(std::string name) {
	to_lower_str(name);
	WaypointMap::iterator iter = waypoints.find(name);
	if (iter == waypoints.end()) {
		return;
	}
	waypoints.erase(iter);
}
