//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_GAME_SRV_HOMES_H_
#define RME_GAME_SRV_HOMES_H_

// Monster and NPC homes of sector maps. A sector map has no spawns: each
// monster on it is a home of its own (one dat/monster.db line), and each NPC
// has its home in its npc/*.npc file. The creature on the home tile carries
// the home's radius (Creature::getHomeRadius), amount and respawn time.

namespace rme::srv {

	// The radius a monster placed in the editor starts with: what 9,266 of
	// the 9,615 homes of the original monster.db use.
	inline constexpr int kDefaultMonsterHomeRadius = 50;

} // namespace rme::srv

#endif
