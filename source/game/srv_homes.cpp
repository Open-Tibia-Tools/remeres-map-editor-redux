//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "game/srv_homes.h"

#include "game/spawn.h"
#include "map/map.h"
#include "map/tile.h"

#include <cstdlib>

namespace rme::srv {

	int homeSpawnSize(const Map& map, const Position& position) {
		const Tile* tile = map.getTile(position);
		if (!tile) {
			return 1;
		}
		if (tile->spawn) {
			return tile->spawn->getSize();
		}
		const TileLocation* location = tile->getLocation();
		if (!location || location->getSpawnCount() == 0) {
			return 1;
		}

		// Squares of growing size around the tile, as Map::getSpawnList searches them.
		const auto covering = [&](int x, int y) -> const Spawn* {
			const Tile* other = map.getTile(x, y, position.z);
			if (other && other->spawn && std::abs(position.x - x) <= other->spawn->getSize() && std::abs(position.y - y) <= other->spawn->getSize()) {
				return other->spawn.get();
			}
			return nullptr;
		};
		const int max_ring = std::max(map.getWidth(), map.getHeight());
		for (int ring = 1; ring <= max_ring; ++ring) {
			const int left = position.x - ring;
			const int right = position.x + ring;
			const int top = position.y - ring;
			const int bottom = position.y + ring;
			for (int x = left; x <= right; ++x) {
				for (const int y : { top, bottom }) {
					if (const Spawn* spawn = covering(x, y)) {
						return spawn->getSize();
					}
				}
			}
			for (int y = top + 1; y < bottom; ++y) {
				for (const int x : { left, right }) {
					if (const Spawn* spawn = covering(x, y)) {
						return spawn->getSize();
					}
				}
			}
		}
		return 1;
	}

} // namespace rme::srv
