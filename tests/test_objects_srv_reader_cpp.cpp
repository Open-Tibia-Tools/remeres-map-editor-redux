// Standalone tests for the server objects.srv reader.
//
// Build and run (from the repository root):
//   g++ -std=c++23 -O2 -Isource tests/test_objects_srv_reader_cpp.cpp -o test_objects_srv_reader
//   ./test_objects_srv_reader [optional path to objects.srv to read completely]
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "item_definitions/formats/srv/objects_srv_reader.cpp"

using namespace rme::srv;

namespace {

	void testRecords() {
		const auto types = readObjectTypes(
			"# Tibia - graphical Multi-User-Dungeon\n"
			"TypeID      = 0 # map container\n"
			"Name        = \"\"\n"
			"Flags       = {Container}\n"
			"Attributes  = {Capacity=0}\n"
			"\n"
			"TypeID      = 1648\n"
			"Name        = \"a gate of expertise\"\n"
			"Description = \"Only the \\\"worthy\\\" may pass\"\n"
			"Flags       = {Bottom,LevelDoor,Unpass}\n"
			"Attributes  = {Waypoints=0,DisguiseTarget=353,Offset=-4}\n"
			"\n"
			"TypeID      = 4600\n"
			"Name        = \"shallow water\"\n"
			"Flags       = {}\n");
		assert(types.has_value());
		assert(types->size() == 3);

		const ObjectType& gate = (*types)[1];
		assert(gate.type_id == 1648 && gate.name == "a gate of expertise");
		assert(gate.description == "Only the \"worthy\" may pass");
		assert(gate.hasFlag("leveldoor") && gate.hasFlag("Unpass") && !gate.hasFlag("Bank"));
		assert(gate.attribute("DisguiseTarget") == 353);
		assert(gate.attribute("offset") == -4);
		assert(!gate.attribute("Capacity").has_value());

		assert((*types)[2].flags.empty() && (*types)[2].attributes.empty());
	}

	void testErrors() {
		assert(!readObjectTypes("Name = \"orphan\"\n").has_value());
		assert(!readObjectTypes("TypeID = 100\nColour = 3\n").has_value());
		assert(!readObjectTypes("TypeID = 100\nFlags = {Bank Unpass}\n").has_value());
		assert(!readObjectTypes("TypeID = 70000\n").has_value());
		const auto error = readObjectTypes("TypeID = 100\nName = \"a\"\nAttributes = {Weight=}\n");
		assert(!error.has_value() && error.error().line == 3);
	}

	void readFile(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::binary);
		const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		const auto types = readObjectTypes(text);
		if (!types) {
			std::cerr << path.string() << ":" << types.error().line << ": " << types.error().message << "\n";
		}
		assert(types.has_value());
		size_t disguised = 0;
		for (const ObjectType& type : *types) {
			disguised += type.hasFlag("Disguise") ? 1 : 0;
		}
		std::cout << std::format("{}: {} object types, {} disguised\n", path.string(), types->size(), disguised);
	}

} // namespace

int main(int argc, char** argv) {
	testRecords();
	testErrors();
	std::cout << "objects.srv reader tests passed\n";
	if (argc > 1) {
		readFile(argv[1]);
	}
	return 0;
}
