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

#ifndef RME_RAW_BRUSH_H
#define RME_RAW_BRUSH_H

#include "brushes/brush.h"
#include "item_definitions/core/item_definition_types.h"

//=============================================================================
// RAWBrush, draw items like SimOne's editor

class RAWBrush : public Brush {
public:
	RAWBrush(ServerItemId itemid); // Create a RAWBrush of the specified type
	~RAWBrush() override;


	bool canDraw(BaseMap* map, const Position& position) const override {
		return true;
	}
	void draw(BaseMap* map, Tile* tile, void* parameter) override;
	void undraw(BaseMap* map, Tile* tile) override;

	bool canDrag() const override {
		return true;
	}
	int getLookID() const override;
	std::string getName() const override;
	ServerItemId getItemID() const;

protected:
	ServerItemId item_id = 0;
};

#endif
