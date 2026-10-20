//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "brushes/creature/monster_brush.h"
#include "ui/gui.h"
#include "app/settings.h"
#include "map/tile.h"
#include "game/creature.h"
#include "map/basemap.h"
#include "map/map.h"
#include "game/spawn.h"

MonsterBrush::MonsterBrush(CreatureType* type) :
	CreatureBrush(type) {
}

bool MonsterBrush::canDraw(BaseMap* map, const Position& position) const {
	Tile* tile = map->getTile(position);
	if (creature_type && tile && !tile->isBlocking()) {
		if (tile->getLocation()->getSpawnCount() != 0 || g_settings.getInteger(Config::AUTO_CREATE_SPAWN)) {
			// Monsters cannot be placed in protection zones
			return !tile->isPZ();
		}
	}
	return false;
}

void MonsterBrush::draw_creature(BaseMap* map, Tile* tile) {
	if (!canDraw(map, tile->getPosition())) {
		return;
	}
	undraw(map, tile);
	if (!creature_type) {
		return;
	}

	const bool is_otbm5 = map && map->supportsZones();

	if (tile->spawn == nullptr && tile->getLocation()->getSpawnCount() == 0) {
		tile->spawn = std::make_unique<Spawn>(1);
	}
	if (is_otbm5) {
		if (tile->spawn) {
			tile->spawn->addKind(Spawn::MONSTERS);
		} else if (Map* full_map = dynamic_cast<Map*>(map)) {
			for (Spawn* s : full_map->getSpawnList(tile)) {
				s->addKind(Spawn::MONSTERS);
			}
		}
	}

	tile->creature = std::make_unique<Creature>(creature_type);
	tile->creature->setSpawnTime(g_gui.GetSpawnTime());
	if (is_otbm5) {
		if (const int weight = g_gui.GetDefaultCreatureWeight(); weight > 0) {
			tile->creature->setWeight(static_cast<uint32_t>(weight));
		}
	}
}
