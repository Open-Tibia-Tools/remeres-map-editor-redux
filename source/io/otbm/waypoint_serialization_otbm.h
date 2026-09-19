#ifndef RME_WAYPOINT_SERIALIZATION_OTBM_H_
#define RME_WAYPOINT_SERIALIZATION_OTBM_H_

#include "io/otbm/otbm_types.h"

class Map;
class BinaryNode;
class FastOTBMNode;
class NodeFileWriteHandle;
struct MapVersion;

class WaypointSerializationOTBM {
public:
	static void readWaypoints(Map& map, BinaryNode* mapNode);
	static void readWaypointsFast(Map& map, FastOTBMNode& mapNode);
	static OTBMWriteResult writeWaypoints(const Map& map, NodeFileWriteHandle& f, MapVersion mapVersion);
};

#endif
