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

#include "brushes/town/town_temple_brush.h"
#include "game/town.h"
#include "map/basemap.h"
#include "map/tile.h"

TownTempleBrush::TownTempleBrush() :
	Brush(),
	draw_town(0) {
}

TownTempleBrush::~TownTempleBrush() = default;

void TownTempleBrush::setTown(Town* town) {
	draw_town = town ? town->getID() : 0;
}

uint32_t TownTempleBrush::getTownID() const {
	return draw_town;
}

bool TownTempleBrush::canDraw(BaseMap* map, const Position& position) const {
	if (!map || !position.isValid()) {
		return false;
	}
	if (!hasPlacementCallback() && draw_town == 0) {
		return false;
	}
	Tile* tile = map->getTile(position);
	return tile != nullptr && tile->hasGround();
}

void TownTempleBrush::undraw(BaseMap* map, Tile* tile) {
	ASSERT(false);
}

void TownTempleBrush::draw(BaseMap* map, Tile* tile, void* parameter) {
	ASSERT(false);
}
