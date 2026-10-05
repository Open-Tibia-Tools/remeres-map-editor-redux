//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "brushes/zone/zone_brush.h"

#include "map/basemap.h"
#include "map/tile.h"

bool ZoneBrush::canDraw(BaseMap* map, const Position& position) const {
	if (zone_id == 0) {
		return false;
	}
	const Tile* tile = map->getTile(position);
	return tile && tile->hasGround();
}

void ZoneBrush::draw(BaseMap* /*map*/, Tile* tile, void* /*parameter*/) {
	if (tile->hasGround()) {
		tile->addZone(zone_id);
	}
}

void ZoneBrush::undraw(BaseMap* /*map*/, Tile* tile) {
	tile->removeZone(zone_id);
}
