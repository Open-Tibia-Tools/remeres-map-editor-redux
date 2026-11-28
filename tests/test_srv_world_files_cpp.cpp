// Standalone tests for the server world file readers (map.dat, houseareas.dat, houses.dat).
//
// Build and run (from the repository root):
//   g++ -std=c++23 -O2 -Isource tests/test_srv_world_files_cpp.cpp -o test_srv_world_files
//   ./test_srv_world_files [optional server dat folder holding map.dat, houseareas.dat and houses.dat]
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>

#include "io/sec/sec_sector_parser.cpp"
#include "io/sec/srv_world_files.cpp"

using namespace rme::sec;

namespace {

	std::string readText(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	}

	void testMapInfo() {
		const auto info = readMapInfo(
			"# GIMUD - Graphical Interface Multi User Dungeon\n"
			"SectorXMin =  996\n"
			"@\"mem.dat\"\n"
			"NewbieStart = [32097,32219,7]\n"
			"VeteranStart = [32369,32241,7]\n"
			"Mark = (\"Thais\",[32369,32215,7])\n"
			"Mark = (\"Ab'Dendriel\",[32661,31687,7])\n"
			"Depot = (0,\"Thais\",1000)\n"
			"Depot = (8,\"Port Hope\",500)\n");
		assert(info.has_value());
		assert(info->marks.size() == 2 && info->marks[1].name == "Ab'Dendriel" && info->marks[1].position.y == 31687);
		assert(info->depots.size() == 2 && info->depots[1].id == 8 && info->depots[1].name == "Port Hope" && info->depots[1].capacity == 500);
		assert(info->newbie_start && info->newbie_start->x == 32097);
		assert(info->veteran_start && info->veteran_start->z == 7);

		// The sector range needs all six bounds.
		assert(!info->sectors.has_value());
		const auto ranged = readMapInfo("SectorXMin = 996\nSectorXMax = 1043\nSectorYMin = 984\nSectorYMax = 1031\nSectorZMin = 0\nSectorZMax = 15\n");
		assert(ranged.has_value() && ranged->sectors.has_value());
		assert(ranged->sectors->min.x == 996 && ranged->sectors->max.y == 1031 && ranged->sectors->max.z == 15);
		assert(ranged->sectors->contains(SectorCoordinates { .x = 1043, .y = 984, .z = 7 }));
		assert(!ranged->sectors->contains(SectorCoordinates { .x = 1044, .y = 984, .z = 7 }));

		const auto bad = readMapInfo("Mark = (\"Thais\",[32369,32215])\n");
		assert(!bad.has_value() && bad.error().line == 1);
	}

	void testHouseAreas() {
		const auto areas = readHouseAreas(
			"# Gebietsnummer, Name, Quadratmeterpreis, Depot\n"
			"Area = (100,\"Wilderness around Thais\",45,0)\n"
			"Area = (200,\"Wilderness around Carlin\",40,1)\n");
		assert(areas.has_value() && areas->size() == 2);
		assert((*areas)[1].id == 200 && (*areas)[1].square_price == 40 && (*areas)[1].depot == 1);
	}

	void testHouses() {
		const auto houses = readHouses(
			"ID = 10001\n"
			"Name = \"Spiritkeep\"\n"
			"Description = \"This guildhall has twenty-three beds.\"\n"
			"RentOffset = 2200\n"
			"Area = 100\n"
			"GuildHouse = true\n"
			"Exit = [32265,32316,7]\n"
			"Center = [32264,32314,7]\n"
			"Fields = {[32258,32309,5],[32259,32309,5]}\n"
			"\n"
			"ID = 10002\n"
			"Name = \"Empty\"\n"
			"Description = \"\"\n"
			"RentOffset = 0\n"
			"Area = 101\n"
			"GuildHouse = false\n"
			"Exit = [1,2,3]\n"
			"Center = [1,2,3]\n"
			"Fields = {}\n");
		assert(houses.has_value() && houses->size() == 2);
		const HouseDefinition& keep = (*houses)[0];
		assert(keep.id == 10001 && keep.name == "Spiritkeep" && keep.rent_offset == 2200 && keep.area == 100 && keep.guild_house);
		assert(keep.exit.x == 32265 && keep.center.y == 32314);
		assert(keep.fields.size() == 2 && keep.fields[1].x == 32259 && keep.fields[1].z == 5);
		assert(!(*houses)[1].guild_house && (*houses)[1].fields.empty());

		assert(!readHouses("Name = \"orphan\"\n").has_value());
		assert(writeHouses({ (*houses)[0] }, "\n") ==
			"ID = 10001\n"
			"Name = \"Spiritkeep\"\n"
			"Description = \"This guildhall has twenty-three beds.\"\n"
			"RentOffset = 2200\n"
			"Area = 100\n"
			"GuildHouse = true\n"
			"Exit = [32265,32316,7]\n"
			"Center = [32264,32314,7]\n"
			"Fields = {[32258,32309,5],[32259,32309,5]}\n");

		const std::string map_dat = "SectorXMin = 996\r\nMark = (\"Thais\",[1,2,3])\r\n# comment\r\nMark = (\"Old\",[4,5,6])\r\nDepot = (0,\"Thais\",1000)\r\n";
		// Marks and depots are written where their first line was; other lines stay.
		const std::vector<MapDepot> depots { MapDepot { .id = 0, .name = "Thais" }, MapDepot { .id = 1, .name = "Carlin", .capacity = 2000 } };
		assert(updateMapDat(map_dat, { MapMark { .name = "Thais", .position = { 7, 8, 9 } }, MapMark { .name = "New \"Mark\"", .position = { 1, 1, 1 } } }, depots) ==
			"SectorXMin = 996\r\nMark = (\"Thais\",[7,8,9])\r\nMark = (\"New \\\"Mark\\\"\",[1,1,1])\r\n# comment\r\nDepot = (0,\"Thais\",1000)\r\nDepot = (1,\"Carlin\",2000)\r\n");
		// Marks and depots that are gone lose their lines; new ones without a place go at the end.
		assert(updateMapDat(map_dat, {}, {}) == "SectorXMin = 996\r\n# comment\r\n");
		assert(updateMapDat("SectorXMin = 996", { MapMark { .name = "A", .position = { 1, 2, 3 } } }, depots) ==
			"SectorXMin = 996\nMark = (\"A\",[1,2,3])\nDepot = (0,\"Thais\",1000)\nDepot = (1,\"Carlin\",2000)\n");
		// Keys that only start with Mark or Depot are other keys.
		assert(updateMapDat("Mark2 = 1\nMark_old = 2\nDepots = 3\nMark = (\"Old\",[4,5,6])\n", {}, {}) == "Mark2 = 1\nMark_old = 2\nDepots = 3\n");

		// A new map.dat reads back as written.
		MapInfo fresh;
		fresh.sectors = SectorRange { .min = { .x = 1000, .y = 1000, .z = 7 }, .max = { .x = 1001, .y = 1002, .z = 7 } };
		fresh.veteran_start = AbsolutePosition { .x = 32000, .y = 32001, .z = 7 };
		fresh.depots = depots;
		fresh.marks = { MapMark { .name = "Temple", .position = { 32000, 32000, 7 } } };
		const auto reread = readMapInfo(writeMapDat(fresh));
		assert(reread.has_value() && reread->sectors && reread->sectors->max.y == 1002 && reread->sectors->min.z == 7);
		assert(!reread->newbie_start && reread->veteran_start && *reread->veteran_start == *fresh.veteran_start);
		assert(reread->depots == fresh.depots && reread->marks.size() == 1 && reread->marks[0].name == "Temple");

		// New house areas go at the end, in the file's line endings.
		const std::vector<HouseArea> new_areas { HouseArea { .id = 200, .name = "Carlin", .square_price = 0, .depot = 1 } };
		assert(appendHouseAreas("# areas\nArea = (100,\"A\",45,0)", new_areas) == "# areas\nArea = (100,\"A\",45,0)\nArea = (200,\"Carlin\",0,1)\n");
		const auto areas = readHouseAreas(appendHouseAreas("", new_areas));
		assert(areas.has_value() && areas->size() == 1 && (*areas)[0].id == 200 && (*areas)[0].depot == 1);

		// Like the server's loader, commas between fields are optional.
		const auto no_commas = readHouses("ID = 1\nFields = {[1,2,3] [4,5,6]}\n");
		assert(no_commas.has_value() && (*no_commas)[0].fields.size() == 2);
		assert(!readHouses("ID = 1\nFields = {7}\n").has_value());
	}

	void readFolder(const std::filesystem::path& dat) {
		const std::string map_dat = readText(dat / "map.dat");
		const std::string houses_dat = readText(dat / "houses.dat");
		const auto info = readMapInfo(map_dat);
		const auto areas = readHouseAreas(readText(dat / "houseareas.dat"));
		const auto houses = readHouses(houses_dat);
		// Unchanged data reads back the same. (houses.dat mixes two writers'
		// styles, some pad the floor as "05", so the bytes can differ.)
		if (houses) {
			const std::string_view newline = houses_dat.find("\r\n") != std::string::npos ? "\r\n" : "\n";
			const auto reread = readHouses(writeHouses(*houses, newline));
			assert(reread.has_value() && *reread == *houses);
		}
		if (info) {
			assert(updateMapDat(map_dat, info->marks, info->depots) == map_dat);
		}
		for (const auto* error : { info ? nullptr : &info.error(), areas ? nullptr : &areas.error(), houses ? nullptr : &houses.error() }) {
			if (error) {
				std::cerr << error->line << ": " << error->message << "\n";
			}
		}
		assert(info && areas && houses);

		std::map<int, int> area_depot;
		for (const HouseArea& area : *areas) {
			area_depot[area.id] = area.depot;
		}
		size_t fields = 0;
		size_t unknown_areas = 0;
		for (const HouseDefinition& house : *houses) {
			fields += house.fields.size();
			unknown_areas += area_depot.contains(house.area) ? 0 : 1;
		}
		const std::string range = info->sectors ? std::format("{}-{} x {}-{} x {}-{}", info->sectors->min.x, info->sectors->max.x, info->sectors->min.y, info->sectors->max.y, info->sectors->min.z, info->sectors->max.z) : "none";
		std::cout << std::format("{}: {} marks, {} depots, {} house areas, {} houses ({} fields), {} houses with unknown areas, sector range {}\n", dat.string(), info->marks.size(), info->depots.size(), areas->size(), houses->size(), fields, unknown_areas, range);
		assert(unknown_areas == 0);
	}

} // namespace

int main(int argc, char** argv) {
	testMapInfo();
	testHouseAreas();
	testHouses();
	std::cout << "server world file tests passed\n";
	if (argc > 1) {
		readFolder(argv[1]);
	}
	return 0;
}
