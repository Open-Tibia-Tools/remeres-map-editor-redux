//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_GAME_SRV_HOMES_H_
#define RME_GAME_SRV_HOMES_H_

// Radii of sector map monster and NPC homes. The server's radius can be 0 or
// far above what an editor spawn covers (some homes use 5000), so a home
// keeps it on its creature while the spawn on the tile shows it capped.

#include "game/creature.h"

#include <algorithm>

namespace rme::srv {

	// The spawn size that shows a home radius.
	[[nodiscard]] inline int spawnSizeFor(int home_radius, int max_spawn_radius) {
		return std::clamp(home_radius, 1, std::max(1, max_spawn_radius));
	}

	// The home's radius: the server's value while the spawn still shows it,
	// the spawn's size once that was changed.
	[[nodiscard]] inline int homeRadius(const Creature& creature, int spawn_size, int max_spawn_radius) {
		const auto home = creature.getHomeRadius();
		if (home && spawn_size == spawnSizeFor(*home, max_spawn_radius)) {
			return *home;
		}
		return spawn_size;
	}

} // namespace rme::srv

#endif
