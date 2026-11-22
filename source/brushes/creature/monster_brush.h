//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_MONSTER_BRUSH_H
#define RME_MONSTER_BRUSH_H

#include "brushes/creature/creature_brush.h"

// MonsterBrush, specialized placement for Monsters (OTBM 5)
class MonsterBrush : public CreatureBrush {
public:
	explicit MonsterBrush(CreatureType* type);
	~MonsterBrush() override = default;

	void draw_creature(BaseMap* map, Tile* tile) override;

protected:
	bool placesNpc() const override {
		return false;
	}
};

#endif
