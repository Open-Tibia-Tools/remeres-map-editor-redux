// Standalone tests for the server creature file readers (.mon, .npc, .evt, monster.db).
//
// Build and run (from the repository root):
//   g++ -std=c++23 -O2 -Isource tests/test_srv_creature_files_cpp.cpp -o test_srv_creature_files
//   ./test_srv_creature_files [optional server data folder holding mon/, npc/ and dat/monster.db]
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>

#include "io/sec/sec_sector_parser.cpp"
#include "io/sec/srv_creature_files.cpp"

using namespace rme::sec;

namespace {

	std::string readText(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	}

	void testMonsterRace() {
		const auto race = readMonsterRace(
			"# Tibia - graphical Multi-User-Dungeon\n"
			"RaceNumber    = 77\n"
			"Name          = \"amazon\"\n"
			"Outfit        = (137, 113-120-95-115)\n"
			"Experience    = 60\n");
		assert(race.has_value());
		assert(race->race == 77 && race->name == "amazon");
		assert(race->outfit.look_type == 137 && race->outfit.head == 113 && race->outfit.feet == 115 && race->outfit.look_item == 0);

		const auto object_look = readMonsterRace("RaceNumber = 5\nName = \"x\"\nOutfit = (0,2051)\n");
		assert(object_look.has_value() && object_look->outfit.look_type == 0 && object_look->outfit.look_item == 2051);

		assert(!readMonsterRace("Name = \"no race\"\nOutfit = (1, 0-0-0-0)\n").has_value());
		const auto bad_outfit = readMonsterRace("RaceNumber = 1\nName = \"a\"\nOutfit = (1, 2-3)\n");
		assert(!bad_outfit.has_value() && bad_outfit.error().line == 3);
	}

	void testNpc() {
		const auto npc = readNpc(
			"# GIMUD - Graphical Interface Multi User Dungeon\n"
			"Name = \"Adrenius\"\n"
			"Outfit = (9,0-0-0-0)\n"
			"Home = [32660,32112,8]\n"
			"Radius = 2\n"
			"\n"
			"Behaviour = {\n"
			"ADDRESS,\"hello$\",!       -> \"Hello, %N!\"\n"
			"Topic=1,\"yes\"             -> \"Fine.\"\n"
			"}\n");
		assert(npc.has_value());
		assert(npc->name == "Adrenius" && npc->outfit.look_type == 9);
		assert(npc->home.x == 32660 && npc->home.y == 32112 && npc->home.z == 8 && npc->radius == 2);

		assert(!readNpc("Name = \"Homeless\"\nOutfit = (9,0-0-0-0)\n").has_value());
	}

	void testMonsterHomes() {
		const auto homes = readMonsterHomes(
			"# Race     X     Y  Z Radius Amount Regen.\n"
			"# ====== 0997,0988,09 ====================\n"
			"    50 31927 31622  9     50      3    700\n"
			"     7 31931 31657  9     50      2    700\n"
			"0 # zero for end of file\n");
		assert(homes.has_value() && homes->size() == 2);
		const MonsterHome& first = (*homes)[0];
		assert(first.race == 50 && first.position.x == 31927 && first.position.y == 31622 && first.position.z == 9);
		assert(first.radius == 50 && first.amount == 3 && first.regen == 700);

		assert(!readMonsterHomes("50 31927 31622 9 50 3\n").has_value());

		const std::string written = writeMonsterHomes(*homes);
		assert(written ==
			"# Tibia - graphical Multi-User-Dungeon\n"
			"# MonsterHomes File\n"
			"\n"
			"# Race     X     Y  Z Radius Amount Regen.\n"
			"\n"
			"# ====== 0997,0988,09 ====================\n"
			"    50 31927 31622  9     50      3    700\n"
			"# ====== 0997,0989,09 ====================\n"
			"     7 31931 31657  9     50      2    700\n"
			"0 # zero for end of file\n");
	}

	void testRaid() {
		const auto raid = readRaid(
			"# Ablauf: eine Angriffswelle\n"
			"Type      = SmallRaid\n"
			"Interval  = 1209600\n"
			"Delay     = 1\n"
			"Position  = [32681,31598,7]\n"
			"Spread    = 6\n"
			"Race      = 105\n"
			"Count     = (9,18)\n"
			"# Message   = \"commented out\"\n"
			"Delay     = 60\n"
			"Position  = [32635,31724,7]\n"
			"Race      = 27\n"
			"Count     = (0,6)\n"
			"Message   = \"Beasts are roaming!\"\n");
		assert(raid.has_value());
		assert(raid->type == "SmallRaid" && raid->interval == 1209600 && raid->waves.size() == 2);
		const RaidWave& first = raid->waves[0];
		assert(first.delay == 1 && first.position.x == 32681 && first.spread == 6 && first.race == 105 && first.min_count == 9 && first.max_count == 18 && first.message.empty());
		assert(raid->waves[1].delay == 60 && raid->waves[1].min_count == 0 && raid->waves[1].message == "Beasts are roaming!");

		const auto no_delay = readRaid("Type = BigRaid\nPosition = [1,2,3]\n");
		assert(!no_delay.has_value() && no_delay.error().line == 2);
		const auto no_race = readRaid("Delay = 0\nPosition = [1,2,3]\nDelay = 5\n");
		assert(!no_race.has_value() && no_race.error().line == 1);
	}

	void testRaceNames() {
		const auto race = [](int number, std::string name) { return MonsterRace { .race = number, .name = std::move(name) }; };
		const auto named = nameMonsterRaces({
			{ "butterflyblue", race(227, "butterfly") },
			{ "rat", race(21, "rat") },
			{ "butterflypurple", race(213, "butterfly") },
			{ "Butterflyred", race(228, "Butterfly") },
		});
		assert(named.size() == 4);
		assert(named[0].race.race == 21 && named[0].editor_name == "rat");
		assert(named[1].race.race == 213 && named[1].editor_name == "butterfly");
		assert(named[2].race.race == 227 && named[2].editor_name == "butterfly (butterflyblue)");
		assert(named[3].race.race == 228 && named[3].editor_name == "Butterfly (Butterflyred)");
	}

	void testNpcHomeUpdate() {
		const std::string npc =
			"Name = \"Adrenius\"\r\n"
			"Home   = [32660,32112,8]\r\n"
			"Radius = 2\r\n"
			"\r\n"
			"Behaviour = {\r\n"
			"Home = \"not a field\"\r\n"
			"}\r\n";
		const std::string updated = updateNpcHome(npc, AbsolutePosition { .x = 32661, .y = 32113, .z = 7 }, 4);
		assert(updated ==
			"Name = \"Adrenius\"\r\n"
			"Home   = [32661,32113,7]\r\n"
			"Radius = 4\r\n"
			"\r\n"
			"Behaviour = {\r\n"
			"Home = \"not a field\"\r\n"
			"}\r\n");
		assert(updateNpcHome(npc, AbsolutePosition { .x = 32660, .y = 32112, .z = 8 }, 2) == npc);
	}

	void readFolder(const std::filesystem::path& data) {
		std::set<int> races;
		size_t monsters = 0;
		for (const auto& entry : std::filesystem::directory_iterator(data / "mon")) {
			if (entry.path().extension() != ".mon") {
				continue;
			}
			const auto race = readMonsterRace(readText(entry.path()));
			if (!race) {
				std::cerr << entry.path().filename().string() << ":" << race.error().line << ": " << race.error().message << "\n";
			}
			assert(race.has_value());
			races.insert(race->race);
			++monsters;
		}

		size_t npcs = 0;
		for (const auto& entry : std::filesystem::directory_iterator(data / "npc")) {
			if (entry.path().extension() != ".npc") {
				continue;
			}
			const auto npc = readNpc(readText(entry.path()));
			if (!npc) {
				std::cerr << entry.path().filename().string() << ":" << npc.error().line << ": " << npc.error().message << "\n";
			}
			assert(npc.has_value());
			++npcs;
		}

		const std::string monster_db = readText(data / "dat" / "monster.db");
		const auto homes = readMonsterHomes(monster_db);
		if (homes && writeMonsterHomes(*homes) != monster_db) {
			std::cerr << "monster.db is written back differently\n";
		}
		if (!homes) {
			std::cerr << "monster.db:" << homes.error().line << ": " << homes.error().message << "\n";
		}
		assert(homes.has_value());
		size_t unknown_races = 0;
		size_t monsters_placed = 0;
		for (const MonsterHome& home : *homes) {
			unknown_races += races.contains(home.race) ? 0 : 1;
			monsters_placed += static_cast<size_t>(home.amount);
		}
		size_t raids = 0;
		size_t waves = 0;
		size_t unknown_raid_races = 0;
		for (const auto& entry : std::filesystem::directory_iterator(data / "mon")) {
			if (entry.path().extension() != ".evt") {
				continue;
			}
			const auto raid = readRaid(readText(entry.path()));
			if (!raid) {
				std::cerr << entry.path().filename().string() << ":" << raid.error().line << ": " << raid.error().message << "\n";
			}
			assert(raid.has_value());
			++raids;
			waves += raid->waves.size();
			for (const RaidWave& wave : raid->waves) {
				unknown_raid_races += races.contains(wave.race) ? 0 : 1;
			}
		}
		std::cout << std::format("{}: {} monster races, {} NPCs, {} monster homes ({} monsters), {} homes with unknown races, {} raids ({} waves, {} with unknown races)\n", data.string(), monsters, npcs, homes->size(), monsters_placed, unknown_races, raids, waves, unknown_raid_races);
	}

} // namespace

int main(int argc, char** argv) {
	testMonsterRace();
	testNpc();
	testMonsterHomes();
	testNpcHomeUpdate();
	testRaceNames();
	testRaid();
	std::cout << "server creature file tests passed\n";
	if (argc > 1) {
		readFolder(argv[1]);
	}
	return 0;
}
