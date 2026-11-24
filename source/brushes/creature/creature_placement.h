#ifndef RME_BRUSHES_CREATURE_CREATURE_PLACEMENT_H_
#define RME_BRUSHES_CREATURE_CREATURE_PLACEMENT_H_

// Decides whether a creature brush may place its creature on a tile, and why not.
// Pure rules, no map or wx dependency, so the decision is testable on its own.
namespace CreaturePlacement {

	enum class Result {
		Allowed,
		NoCreatureType,
		NoTile,
		TileBlocking,
		// Outside every spawn and "Place spawn with creature" is turned off.
		NoSpawn,
		// Monsters cannot be placed in protection zones; NPCs can.
		MonsterInProtectionZone,
	};

	struct Input {
		bool has_creature_type = false;
		bool has_tile = false;
		bool tile_blocking = false;
		// The tile already lies inside at least one spawn radius.
		bool inside_spawn = false;
		// "Place spawn with creature": a spawn is created on the tile when needed.
		bool auto_create_spawn = false;
		bool protection_zone = false;
		bool is_npc = false;
	};

	constexpr Result Check(const Input& input) {
		if (!input.has_creature_type) {
			return Result::NoCreatureType;
		}
		if (!input.has_tile) {
			return Result::NoTile;
		}
		if (input.tile_blocking) {
			return Result::TileBlocking;
		}
		if (!input.inside_spawn && !input.auto_create_spawn) {
			return Result::NoSpawn;
		}
		if (input.protection_zone && !input.is_npc) {
			return Result::MonsterInProtectionZone;
		}
		return Result::Allowed;
	}

	// Status bar text for a rejected placement; empty for Allowed.
	constexpr const char* Describe(Result result) {
		switch (result) {
			case Result::Allowed:
				return "";
			case Result::NoCreatureType:
				return "No creature selected.";
			case Result::NoTile:
				return "Creatures can only be placed on an existing tile.";
			case Result::TileBlocking:
				return "Creatures cannot be placed on a blocking tile.";
			case Result::NoSpawn:
				return "Outside every spawn. Place a spawn first or enable \"Place spawn with creature\".";
			case Result::MonsterInProtectionZone:
				return "Monsters cannot be placed in a protection zone.";
		}
		return "";
	}

} // namespace CreaturePlacement

#endif
