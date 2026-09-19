#include "waypoint_serialization_otbm.h"

#include "map/map.h"
#include "io/otbm/fast_otbm_reader.h"
#include <spdlog/spdlog.h>

void WaypointSerializationOTBM::readWaypoints(Map& map, FastOTBMNode& mapNode) {
	spdlog::debug("Reading OTBM_WAYPOINTS...");
	mapNode.forEachChild([&](FastOTBMNode& waypointNode) {
		if (waypointNode.type != OTBM_WAYPOINT) {
			return;
		}

		std::string name;
		if (!waypointNode.stream.getString(name)) {
			spdlog::warn("Failed to read waypoint name");
			return;
		}

		uint16_t x, y;
		uint8_t z;
		if (!waypointNode.stream.getU16(x) || !waypointNode.stream.getU16(y) || !waypointNode.stream.getU8(z)) {
			spdlog::warn("Invalid position for waypoint '{}'", name);
			return;
		}

		Waypoint wp;
		wp.name = std::move(name);
		wp.pos = { x, y, z };

		map.waypoints.addWaypoint(std::make_unique<Waypoint>(std::move(wp)));
	});
}

OTBMWriteResult WaypointSerializationOTBM::writeWaypoints(const Map& map, NodeFileWriteHandle& f, MapVersion mapVersion) {
	if (map.waypoints.begin() == map.waypoints.end()) {
		return OTBMWriteResult::Success;
	}

	OTBMWriteResult WriteResult = OTBMWriteResult::Success;

	if (mapVersion.otbm < MAP_OTBM_2) {
		return OTBMWriteResult::SuccessWithUnsupportedVersion;
	}
	const bool supportWaypoints = mapVersion.otbm >= MAP_OTBM_3;

	if (supportWaypoints) {
		f.addNode(OTBM_WAYPOINTS);
		for (const auto& [name, waypoint_ptr] : map.waypoints) {
			const Waypoint* waypoint = waypoint_ptr.get();
			f.addNode(OTBM_WAYPOINT);
			f.addString(waypoint->name);
			f.addU16(waypoint->pos.x);
			f.addU16(waypoint->pos.y);
			f.addU8(waypoint->pos.z);
			f.endNode();
		}
		f.endNode();
	} else {
		WriteResult = OTBMWriteResult::SuccessWithUnsupportedVersion;
	}

	return WriteResult;
}
