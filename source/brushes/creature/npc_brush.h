//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_NPC_BRUSH_H
#define RME_NPC_BRUSH_H

#include "brushes/creature/creature_brush.h"

// NpcBrush, specialized placement for NPCs (OTBM 5)
class NpcBrush : public CreatureBrush {
public:
	explicit NpcBrush(CreatureType* type);
	~NpcBrush() override = default;

	void draw_creature(BaseMap* map, Tile* tile) override;

protected:
	bool placesNpc() const override {
		return true;
	}
};

#endif
