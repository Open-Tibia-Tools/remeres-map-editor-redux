#include "rendering/utilities/tile_describer.h"
#include <format>
#include "map/tile.h"
#include "game/spawn.h"
#include "game/creature.h"
#include "game/item.h"

std::string TileDescriber::GetDescription(Tile* tile, bool showSpawns, bool showCreatures) {
	if (!tile) {
		return "Nothing";
	}

	if (tile->spawn && showSpawns) {
		return std::format("Spawn radius: {}", tile->spawn->getSize());
	}

	if (tile->creature && showCreatures) {
		return std::format("{} \"{}\" spawntime: {}",
			tile->creature->isNpc() ? "NPC" : "Monster",
			tile->creature->getName(),
			tile->creature->getSpawnTime());
	}

	if (Item* item = tile->getTopItem()) {
		std::string ss = std::format("Item \"{}\" id:{} cid:{}",
			item->getName(),
			item->getID(),
			item->getClientID());

		if (item->getUniqueID()) {
			ss += std::format(" uid:{}", item->getUniqueID());
		}
		if (item->getActionID()) {
			ss += std::format(" aid:{}", item->getActionID());
		}
		if (item->hasWeight()) {
			ss += std::format(" weight: {:.2f}", item->getWeight());
		}
		return ss;
	}

	return "Nothing";
}
