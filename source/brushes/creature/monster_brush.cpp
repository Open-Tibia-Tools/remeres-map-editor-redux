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
#include "game/srv_homes.h"
#include "ui/properties/srv_attribute_fields.h"

MonsterBrush::MonsterBrush(CreatureType* type) :
	CreatureBrush(type) {
}

bool MonsterBrush::canDraw(BaseMap* map, const Position& position) const {
	Tile* tile = map->getTile(position);
	if (creature_type && tile && !tile->isBlocking()) {
		// A sector map monster is a home of its own and needs no spawn.
		if (rme::srv::isSrvClient() || tile->getLocation()->getSpawnCount() != 0 || g_settings.getInteger(Config::AUTO_CREATE_SPAWN)) {
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
	const bool sector_map = rme::srv::isSrvClient();

	if (!sector_map && tile->spawn == nullptr && tile->getLocation()->getSpawnCount() == 0) {
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
	if (sector_map) {
		tile->creature->setHomeRadius(rme::srv::kDefaultMonsterHomeRadius);
	}
	if (is_otbm5) {
		if (const int weight = g_gui.GetDefaultCreatureWeight(); weight > 0) {
			tile->creature->setWeight(static_cast<uint32_t>(weight));
		}
	}
}
