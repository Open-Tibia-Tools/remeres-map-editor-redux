// Standalone tests for the sector (.sec) parser.
//
// Build and run (from the repository root):
//   g++ -std=c++23 -O2 -Isource tests/test_sec_sector_parser_cpp.cpp -o test_sec_sector_parser
//   ./test_sec_sector_parser [optional sector folder to parse completely]
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "io/sec/sec_sector_parser.cpp"

using namespace rme::sec;

namespace {

	void testFileNames() {
		const auto coords = parseSectorFileName("1005-0997-06.sec");
		assert(coords && coords->x == 1005 && coords->y == 997 && coords->z == 6);
		assert(parseSectorFileName("0996-0984-07.SEC").has_value());
		assert(!parseSectorFileName("1005-0997.sec").has_value());
		assert(!parseSectorFileName("1005-0997-06.sec~").has_value());
		assert(!parseSectorFileName("map.otbm").has_value());
		assert(!parseSectorFileName("a-b-c.sec").has_value());
	}

	void testTilesAndFlags() {
		const auto sector = parseSector(
			"# Tibia - graphical Multi-User-Dungeon\n"
			"# Data for sector 1005/997/6\n"
			"\n"
			"0-18: Refresh, Content={4518, 1100, 1283, 3655, 3654}\n"
			"31-5: Refresh, ProtectionZone, NoLogout\n"
			"07-09: Content={}\n");
		assert(sector.has_value());
		assert(sector->tiles.size() == 3);

		const SectorTile& first = sector->tiles[0];
		assert(first.offset_x == 0 && first.offset_y == 18);
		assert(first.hasFlag(TileFlag::Refresh) && !first.hasFlag(TileFlag::ProtectionZone));
		assert(first.objects.size() == 5);
		assert(first.objects[0].type_id == 4518 && first.objects[4].type_id == 3654);

		const SectorTile& second = sector->tiles[1];
		assert(second.offset_x == 31 && second.offset_y == 5);
		assert(second.hasFlag(TileFlag::Refresh) && second.hasFlag(TileFlag::ProtectionZone) && second.hasFlag(TileFlag::NoLogout));
		assert(second.objects.empty());

		assert(sector->tiles[2].offset_x == 7 && sector->tiles[2].offset_y == 9 && sector->tiles[2].objects.empty());
	}

	void testAttributesAndContainers() {
		const auto sector = parseSector(
			"7-30: Refresh, Content={409, 2435 Content={2821 String=\"Dancing \\\"made\\\" easy\", 2816 String=\"a\\nb\\\\c\"}, 1648 Level=60}\n"
			"16-24: Content={486, 5022 AbsTeleportDestination=-2010517994, 2006 ContainerLiquidType=5 amount=3}\n"
			"1-1: content={1644 DoorQuestNumber=64 DoorQuestValue=1}\n");
		assert(sector.has_value());
		assert(sector->tiles.size() == 3);

		const auto& objects = sector->tiles[0].objects;
		assert(objects.size() == 3);
		const SectorObject& bookcase = objects[1];
		assert(bookcase.type_id == 2435 && bookcase.attributes.empty());
		assert(bookcase.content.size() == 2);
		assert(bookcase.content[0].attributes.size() == 1);
		assert(bookcase.content[0].attributes[0].key == InstanceAttribute::String);
		assert(bookcase.content[0].attributes[0].text == "Dancing \"made\" easy");
		assert(bookcase.content[1].attributes[0].text == "a\nb\\c");
		assert(objects[2].attributes[0].key == InstanceAttribute::Level && objects[2].attributes[0].number == 60);

		const auto& teleport = sector->tiles[1].objects[1];
		assert(teleport.attributes[0].key == InstanceAttribute::AbsTeleportDestination);
		const AbsolutePosition destination = unpackAbsoluteCoordinate(teleport.attributes[0].number);
		assert(destination.x >= 24576 && destination.x < 40960);
		assert(destination.y >= 24576 && destination.y < 40960);
		assert(destination.z >= 0 && destination.z <= 15);

		const auto& potion = sector->tiles[1].objects[2];
		assert(potion.attributes.size() == 2);
		assert(potion.attributes[0].key == InstanceAttribute::ContainerLiquidType && potion.attributes[0].number == 5);
		assert(potion.attributes[1].key == InstanceAttribute::Amount && potion.attributes[1].number == 3);

		const auto& door = sector->tiles[2].objects[0];
		assert(door.attributes.size() == 2);
		assert(door.attributes[1].key == InstanceAttribute::DoorQuestValue && door.attributes[1].number == 1);
	}

	void testCoordinateRoundTrip() {
		// Same packing as the server's PackAbsoluteCoordinate.
		const auto pack = [](int x, int y, int z) {
			return static_cast<int32_t>((((x - 24576) & 0x3FFF) << 18) | (((y - 24576) & 0x3FFF) << 4) | (z & 0xF));
		};
		for (const auto& [x, y, z] : { std::tuple { 32369, 32241, 7 }, std::tuple { 33000, 31000, 15 }, std::tuple { 24576, 24576, 0 } }) {
			const AbsolutePosition position = unpackAbsoluteCoordinate(pack(x, y, z));
			assert(position.x == x && position.y == y && position.z == z);
		}
	}

	void testErrors() {
		const auto missing_offset = parseSector("Content={100}\n");
		assert(!missing_offset.has_value() && missing_offset.error().line == 1);

		const auto bad_attribute = parseSector("0-0: Content={100}\n1-1: Content={100 Colour=3}\n");
		assert(!bad_attribute.has_value() && bad_attribute.error().line == 2);
		assert(bad_attribute.error().message.find("Colour") != std::string::npos);

		assert(!parseSector("32-0: Content={100}").has_value());
		assert(!parseSector("0-0: Content={100").has_value());
		assert(!parseSector("0-0: Content={100 String=\"open").has_value());
		assert(!parseSector("0-0: Content={100 200}").has_value());
		assert(!parseSector("0-0: Content={70000}").has_value());
		assert(!parseSector("0-0: Teleport").has_value());
	}

	void testWriter() {
		SectorTile flags_only { .offset_x = 3, .offset_y = 10, .flags = static_cast<uint8_t>(static_cast<uint8_t>(TileFlag::ProtectionZone) | static_cast<uint8_t>(TileFlag::Refresh)) };
		SectorTile tile { .offset_x = 3, .offset_y = 2 };
		SectorObject book { .type_id = 2821 };
		// Attributes out of order and a zero value: written in the server's order, zero left out.
		book.attributes.push_back(ObjectAttribute { .key = InstanceAttribute::Editor, .text = "Sir \"Q\"" });
		book.attributes.push_back(ObjectAttribute { .key = InstanceAttribute::String, .text = "a\nb\\c" });
		book.attributes.push_back(ObjectAttribute { .key = InstanceAttribute::Amount, .number = 0 });
		SectorObject chest { .type_id = 2546 };
		chest.attributes.push_back(ObjectAttribute { .key = InstanceAttribute::ChestQuestNumber, .number = 18 });
		chest.content.push_back(book);
		tile.objects.push_back(SectorObject { .type_id = 1771 });
		tile.objects.push_back(chest);
		SectorTile empty { .offset_x = 0, .offset_y = 0 };
		Sector sector;
		sector.tiles = { flags_only, tile, empty };

		const std::string text = writeSector(SectorCoordinates { .x = 1005, .y = 997, .z = 6 }, sector);
		const std::string expected =
			"# Tibia - graphical Multi-User-Dungeon\n"
			"# Data for sector 1005/997/6\n"
			"\n"
			"3-2: Content={1771, 2546 ChestQuestNumber=18 Content={2821 String=\"a\\nb\\\\c\" Editor=\"Sir \\\"Q\\\"\"}}\n"
			"3-10: Refresh, ProtectionZone\n";
		if (text != expected) {
			std::cerr << text;
		}
		assert(text == expected);

		// And it reads back to the same objects.
		const auto reread = parseSector(text);
		assert(reread.has_value() && reread->tiles.size() == 2);
		assert(reread->tiles[0].objects[1].content[0].attributes[0].text == "a\nb\\c");
		assert(reread->tiles[0].objects[1].content[0].attributes[1].text == "Sir \"Q\"");

		assert(sectorFileName(SectorCoordinates { .x = 996, .y = 984, .z = 7 }) == "0996-0984-07.sec");
		const AbsolutePosition destination { .x = 32369, .y = 32241, .z = 7 };
		const AbsolutePosition back = unpackAbsoluteCoordinate(packAbsoluteCoordinate(destination));
		assert(back.x == destination.x && back.y == destination.y && back.z == destination.z);
		assert(packAbsoluteCoordinate(unpackAbsoluteCoordinate(-2010517994)) == -2010517994);
	}

	void parseFolder(const std::filesystem::path& folder) {
		const auto started = std::chrono::steady_clock::now();
		size_t files = 0;
		size_t tiles = 0;
		size_t objects = 0;
		size_t failures = 0;
		size_t rewritten_differently = 0;

		const auto count = [](const auto& self, const std::vector<SectorObject>& list) -> size_t {
			size_t total = list.size();
			for (const SectorObject& object : list) {
				total += self(self, object.content);
			}
			return total;
		};

		for (const auto& entry : std::filesystem::directory_iterator(folder)) {
			if (!entry.is_regular_file() || !parseSectorFileName(entry.path().filename().string())) {
				continue;
			}
			std::ifstream stream(entry.path(), std::ios::binary);
			const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
			const auto sector = parseSector(text);
			++files;
			if (!sector) {
				++failures;
				std::cerr << entry.path().filename().string() << ":" << sector.error().line << ": " << sector.error().message << "\n";
				continue;
			}
			tiles += sector->tiles.size();
			for (const SectorTile& tile : sector->tiles) {
				objects += count(count, tile.objects);
			}

			// Files the server wrote come back byte for byte.
			const auto coordinates = parseSectorFileName(entry.path().filename().string());
			if (writeSector(*coordinates, *sector) != text) {
				if (rewritten_differently++ < 3) {
					std::cerr << entry.path().filename().string() << ": written back differently\n";
				}
			}
		}

		const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
		std::cout << std::format("{}: {} files, {} tiles, {} objects, {} failures, {} written back differently, {:.2f} s\n", folder.string(), files, tiles, objects, failures, rewritten_differently, seconds);
		assert(failures == 0);
	}

} // namespace

int main(int argc, char** argv) {
	testFileNames();
	testTilesAndFlags();
	testAttributesAndContainers();
	testCoordinateRoundTrip();
	testErrors();
	testWriter();
	std::cout << "sec sector parser tests passed\n";

	if (argc > 1) {
		parseFolder(argv[1]);
	}
	return 0;
}
