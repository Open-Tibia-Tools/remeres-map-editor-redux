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

bool Waypoints::renameWaypoint(const std::string& old_name, const std::string& new_name) {
	if (new_name.empty()) {
		return false;
	}

	const std::string old_key = as_lower_str(old_name);
	const std::string new_key = as_lower_str(new_name);

	if (old_key == new_key) {
		auto it = waypoints.find(old_key);
		if (it != waypoints.end()) {
			if (it->second->name == new_name) {
				return true;
			}
			it->second->name = new_name;
			map.doChange();
			return true;
		}
		return false;
	}

	if (waypoints.contains(new_key)) {
		return false;
	}

	auto it = waypoints.find(old_key);
	if (it == waypoints.end()) {
		return false;
	}

	auto node = waypoints.extract(it);
	node.key() = new_key;
	node.mapped()->name = new_name;
	waypoints.insert(std::move(node));

	map.doChange();
	return true;
}

