//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_SRV_CREATURE_FILES_H_
#define RME_IO_SEC_SRV_CREATURE_FILES_H_

// Readers for the creature data of the original game server:
//   mon/*.mon       monster races   (RaceNumber, Name, Outfit, ...)
//   npc/*.npc       NPCs            (Name, Outfit, Home, Radius, Behaviour, ...)
//   dat/monster.db  monster homes   (one line per home: Race X Y Z Radius Amount Regen)
//   mon/*.evt       raids           (waves of monsters the server spawns at set times)
// Only the fields the editor shows are read; NPC behaviour scripts are skipped.

#include "io/sec/sec_sector_parser.h"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace rme::sec {

	// `(lookType, head-body-legs-feet)`, or `(0, typeId)` for an object look.
	struct CreatureOutfit {
		int look_type = 0;
		int look_item = 0;
		int head = 0;
		int body = 0;
		int legs = 0;
		int feet = 0;
	};

	struct MonsterRace {
		int race = 0;
		std::string name;
		CreatureOutfit outfit;
	};

	struct NpcDefinition {
		std::string name;
		CreatureOutfit outfit;
		AbsolutePosition home;
		int radius = 0;
	};

	// The server keeps `amount` monsters of `race` alive within `radius` of
	// the home and respawns them after `regen` seconds.
	struct MonsterHome {
		int race = 0;
		AbsolutePosition position;
		int radius = 0;
		int amount = 0;
		int regen = 0;
	};

	// One wave of a raid: `Delay` seconds after the raid starts, between
	// `min_count` and `max_count` monsters of `race` appear within `spread`
	// of the position.
	struct RaidWave {
		int delay = 0;
		AbsolutePosition position;
		int spread = 0;
		int race = 0;
		int min_count = 0;
		int max_count = 0;
		std::string message;
	};

	// A raid file (mon/*.evt). `interval` is the average time between two
	// runs in seconds; a raid with a fixed `Date` instead has interval 0.
	struct Raid {
		std::string type; // SmallRaid or BigRaid
		int64_t interval = 0;
		std::vector<RaidWave> waves;
	};

	[[nodiscard]] std::expected<MonsterRace, ParseError> readMonsterRace(std::string_view text);
	[[nodiscard]] std::expected<Raid, ParseError> readRaid(std::string_view text);

	struct NamedMonsterRace {
		std::string editor_name;
		MonsterRace race;
	};

	// The editor knows creatures by name, but a few races share one (four
	// butterflies, two slimes). The lowest race number keeps the plain name,
	// the others get their file name added, e.g. "butterfly (butterflyblue)",
	// so each race keeps its own look and is saved back as itself. Takes
	// (file stem, race) pairs; returns them ordered by race number.
	[[nodiscard]] std::vector<NamedMonsterRace> nameMonsterRaces(std::vector<std::pair<std::string, MonsterRace>> races_by_file);
	[[nodiscard]] std::expected<NpcDefinition, ParseError> readNpc(std::string_view text);
	[[nodiscard]] std::expected<std::vector<MonsterHome>, ParseError> readMonsterHomes(std::string_view text);

	// monster.db as the server keeps it: homes grouped under a comment per
	// sector, sectors in ascending order (the homes of a sector keep their
	// order), LF line endings.
	[[nodiscard]] std::string writeMonsterHomes(std::vector<MonsterHome> homes);

	// The NPC file with its Home and Radius values replaced; every other line,
	// the behaviour script included, is kept as it is.
	[[nodiscard]] std::string updateNpcHome(std::string_view text, const AbsolutePosition& home, int radius);

} // namespace rme::sec

#endif
