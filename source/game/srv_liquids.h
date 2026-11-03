//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_GAME_SRV_LIQUIDS_H_
#define RME_GAME_SRV_LIQUIDS_H_

#include "game/item.h"

#include <array>

namespace rme::srv {

	// The liquids of the original server, indexed by its liquid type
	// number (ContainerLiquidType / PoolLiquidType), as editor liquid subtypes.
	// The client draws a liquid by colour, and subtype % 8 lands on the colour
	// the server sends for it (GetLiquidColor): wine 15 -> purple, mud 19 -> brown.
	inline constexpr std::array<SplashType, 13> kLiquids = {
		LIQUID_NONE,
		LIQUID_WATER,
		LIQUID_WINE,
		LIQUID_BEER,
		LIQUID_MUD,
		LIQUID_BLOOD,
		LIQUID_SLIME,
		LIQUID_OIL,
		LIQUID_URINE,
		LIQUID_MILK,
		LIQUID_MANAFLUID,
		LIQUID_LIFEFLUID,
		LIQUID_LEMONADE,
	};

} // namespace rme::srv

#endif
