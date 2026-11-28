// Standalone tests for the moveuse.dat rule reader.
//
// Build and run (from the repository root):
//   g++ -std=c++23 -O2 -Isource tests/test_srv_moveuse_cpp.cpp -o test_srv_moveuse
//   ./test_srv_moveuse [optional path to a server's dat/moveuse.dat]
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>

#include "io/sec/srv_moveuse.cpp"

using namespace rme::sec;

namespace {

	std::string readText(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	}

	void testRules() {
		const auto rules = readMoveUseRules(
			"# Tibia - graphical Multi-User-Dungeon\n"
			"BEGIN \"Use\"\n"
			"BEGIN \"Levers\"\n"
			"Use, IsType (Obj1,2772), IsPosition (Obj1,[32625,31698,11]) -> Change(Obj1,2773,0), MoveTopOnMap([32627,31699,10],1771,[32626,31699,10]) \"Bridge OFF\"\n"
			"END\n"
			"Collision, IsType (Obj1,431), HasQuestValue (Obj2,3,= ,0), HasQuestValue (Obj2,7,<>,1), !IsObjectThere ([32864,32556,11],1563) -> SetQuestValue(Obj2,3,1), MoveTopRel(Obj1,[0,-1,0])\n"
			"END\n");
		if (!rules) {
			std::cerr << rules.error().line << ": " << rules.error().message << "\n";
		}
		assert(rules.has_value() && rules->size() == 2);

		const MoveUseRule& lever = (*rules)[0];
		assert(lever.line == 4 && lever.section == "Use / Levers" && lever.event == "Use" && lever.name == "Bridge OFF");
		assert(lever.conditions.size() == 2 && lever.actions.size() == 2);
		assert(lever.conditions[1].name == "IsPosition" && lever.conditions[1].arguments.size() == 2);
		const ScriptValue& trigger = lever.conditions[1].arguments[1];
		assert(trigger.kind == ScriptValue::Kind::Coordinate && trigger.position == (AbsolutePosition { .x = 32625, .y = 31698, .z = 11 }));
		assert(lever.conditions[0].arguments[0].kind == ScriptValue::Kind::Identifier && lever.conditions[0].arguments[0].text == "Obj1");
		assert(lever.actions[1].arguments.size() == 3 && lever.actions[1].arguments[2].position.x == 32626);

		const MoveUseRule& quest = (*rules)[1];
		assert(quest.section == "Use" && quest.name.empty());
		assert(quest.conditions[1].arguments[2].kind == ScriptValue::Kind::Operator && quest.conditions[1].arguments[2].text == "=");
		assert(quest.conditions[2].arguments[2].text == "<>" && quest.conditions[2].arguments[3].number == 1);
		assert(quest.actions[1].arguments[1].position.y == -1);
		assert(quest.conditions.size() == 4 && quest.conditions[3].negated && quest.conditions[3].name == "IsObjectThere" && !quest.conditions[2].negated);

		assert(!readMoveUseRules("BEGIN \"Use\"\nUse, IsType (Obj1,1) Change(Obj1,2,0)\nEND\n").has_value());
		assert(!readMoveUseRules("BEGIN \"Use\"\n").has_value());
		const auto stray_end = readMoveUseRules("Use -> Effect(Obj1,3)\nEND\n");
		assert(!stray_end.has_value() && stray_end.error().line == 2);
	}

	void readFile(const std::filesystem::path& path) {
		const auto rules = readMoveUseRules(readText(path));
		if (!rules) {
			std::cerr << path.string() << ":" << rules.error().line << ": " << rules.error().message << "\n";
		}
		assert(rules.has_value());
		std::map<std::string, size_t> events;
		size_t coordinates = 0;
		for (const MoveUseRule& rule : *rules) {
			++events[rule.event];
			for (const auto* calls : { &rule.conditions, &rule.actions }) {
				for (const ScriptCall& call : *calls) {
					for (const ScriptValue& value : call.arguments) {
						coordinates += value.kind == ScriptValue::Kind::Coordinate ? 1 : 0;
					}
				}
			}
		}
		std::string by_event;
		for (const auto& [event, count] : events) {
			by_event += std::format("{}{} {}", by_event.empty() ? "" : ", ", count, event);
		}
		std::cout << std::format("{}: {} rules ({}), {} coordinates\n", path.string(), rules->size(), by_event, coordinates);
	}

} // namespace

int main(int argc, char** argv) {
	testRules();
	std::cout << "moveuse.dat reader tests passed\n";
	if (argc > 1) {
		readFile(argv[1]);
	}
	return 0;
}
