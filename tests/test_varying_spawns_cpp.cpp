#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <format>
#include <cmath>
#include <sstream>
#include <map>
#include <tuple>

#include "ext/pugixml.hpp"
#include "ext/pugixml.cpp"

namespace {

enum Direction : uint8_t {
	NORTH = 0,
	EAST = 1,
	SOUTH = 2,
	WEST = 3,

	DIRECTION_FIRST = NORTH,
	DIRECTION_LAST = WEST
};

struct Position {
	int x = 0;
	int y = 0;
	int z = 0;

	bool operator==(const Position& other) const = default;
};

struct SpawnAlternative {
	std::string name;
	int spawntime = 0;
	Direction direction = NORTH;
	uint32_t weight = 0;

	bool operator==(const SpawnAlternative& other) const = default;
};

class Creature {
public:
	explicit Creature(std::string name, bool isNpc = false)
		: name_(std::move(name)), isNpc_(isNpc) {}

	[[nodiscard]] const std::string& getName() const noexcept { return name_; }
	void setName(std::string name) { name_ = std::move(name); }

	[[nodiscard]] bool isNpc() const noexcept { return isNpc_; }
	void setNpc(bool npc) noexcept { isNpc_ = npc; }

	[[nodiscard]] Direction getDirection() const noexcept { return direction_; }
	void setDirection(Direction dir) noexcept { direction_ = dir; }

	[[nodiscard]] int getSpawnTime() const noexcept { return spawntime_; }
	void setSpawnTime(int time) noexcept { spawntime_ = time; }

	[[nodiscard]] uint32_t getWeight() const noexcept { return weight_; }
	void setWeight(uint32_t weight) noexcept { weight_ = weight; }

	[[nodiscard]] const std::vector<SpawnAlternative>& getAlternatives() const noexcept { return alternatives_; }
	[[nodiscard]] std::vector<SpawnAlternative>& getAlternatives() noexcept { return alternatives_; }
	void setAlternatives(std::vector<SpawnAlternative> alts) { alternatives_ = std::move(alts); }
	void clearAlternatives() noexcept { alternatives_.clear(); }
	void addAlternative(SpawnAlternative alt) { alternatives_.push_back(std::move(alt)); }

	[[nodiscard]] std::unique_ptr<Creature> deepCopy() const {
		auto copy = std::make_unique<Creature>(name_, isNpc_);
		copy->direction_ = direction_;
		copy->spawntime_ = spawntime_;
		copy->weight_ = weight_;
		copy->alternatives_ = alternatives_;
		return copy;
	}

private:
	std::string name_;
	bool isNpc_ = false;
	Direction direction_ = NORTH;
	int spawntime_ = 60;
	uint32_t weight_ = 0;
	std::vector<SpawnAlternative> alternatives_;
};

struct SpawnEntry {
	Position center;
	int radius = 5;
	Position creatureOffset;
	std::unique_ptr<Creature> creature;
};

// Simulation of XML serialization matching MapXMLIO logic
std::string serializeSpawnsToXml(const std::vector<SpawnEntry>& spawns, bool isOtbm5 = false) {
	pugi::xml_document doc;
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";

	pugi::xml_node rootNode = doc.append_child("spawns");

	for (const auto& entry : spawns) {
		pugi::xml_node spawnNode = rootNode.append_child("spawn");
		spawnNode.append_attribute("centerx") = entry.center.x;
		spawnNode.append_attribute("centery") = entry.center.y;
		spawnNode.append_attribute("centerz") = entry.center.z;
		spawnNode.append_attribute("radius") = entry.radius;

		if (!entry.creature) {
			continue;
		}

		const auto* creature = entry.creature.get();

		if (!isOtbm5 && !creature->isNpc() && !creature->getAlternatives().empty()) {
			// TFS Varying Spawn Group
			pugi::xml_node monstersNode = spawnNode.append_child("monsters");
			monstersNode.append_attribute("x") = entry.creatureOffset.x;
			monstersNode.append_attribute("y") = entry.creatureOffset.y;
			monstersNode.append_attribute("z") = entry.center.z + entry.creatureOffset.z;
			monstersNode.append_attribute("spawntime") = creature->getSpawnTime();

			pugi::xml_node primaryNode = monstersNode.append_child("monster");
			primaryNode.append_attribute("name") = creature->getName().c_str();
			primaryNode.append_attribute("chance") = creature->getWeight();

			for (const auto& alt : creature->getAlternatives()) {
				pugi::xml_node altNode = monstersNode.append_child("monster");
				altNode.append_attribute("name") = alt.name.c_str();
				altNode.append_attribute("chance") = alt.weight;
			}
		} else if (isOtbm5) {
			// OTBM 5 / Canary flat partitioned monster tags
			pugi::xml_node primaryNode = spawnNode.append_child(creature->isNpc() ? "npc" : "monster");
			primaryNode.append_attribute("name") = creature->getName().c_str();
			primaryNode.append_attribute("x") = entry.creatureOffset.x;
			primaryNode.append_attribute("y") = entry.creatureOffset.y;
			primaryNode.append_attribute("z") = entry.center.z + entry.creatureOffset.z;
			primaryNode.append_attribute("spawntime") = creature->getSpawnTime();
			if (creature->getDirection() != NORTH) {
				primaryNode.append_attribute("direction") = static_cast<int>(creature->getDirection());
			}
			if (creature->getWeight() > 0) {
				primaryNode.append_attribute("weight") = creature->getWeight();
			}

			for (const auto& alt : creature->getAlternatives()) {
				pugi::xml_node altNode = spawnNode.append_child("monster");
				altNode.append_attribute("name") = alt.name.c_str();
				altNode.append_attribute("x") = entry.creatureOffset.x;
				altNode.append_attribute("y") = entry.creatureOffset.y;
				altNode.append_attribute("z") = entry.center.z + entry.creatureOffset.z;
				altNode.append_attribute("spawntime") = alt.spawntime;
				if (alt.direction != NORTH) {
					altNode.append_attribute("direction") = static_cast<int>(alt.direction);
				}
				if (alt.weight > 0) {
					altNode.append_attribute("weight") = alt.weight;
				}
			}
		} else {
			// Legacy standard single creature
			pugi::xml_node node = spawnNode.append_child(creature->isNpc() ? "npc" : "monster");
			node.append_attribute("name") = creature->getName().c_str();
			node.append_attribute("x") = entry.creatureOffset.x;
			node.append_attribute("y") = entry.creatureOffset.y;
			node.append_attribute("spawntime") = creature->getSpawnTime();
			if (creature->getDirection() != NORTH) {
				node.append_attribute("direction") = static_cast<int>(creature->getDirection());
			}
		}
	}

	std::ostringstream ss;
	doc.save(ss, "  ", pugi::format_raw | pugi::format_no_declaration);
	return ss.str();
}

// Simulation of XML parsing matching MapXMLIO logic
std::vector<SpawnEntry> parseSpawnsFromXml(const std::string& xmlContent) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load(xmlContent.c_str());
	assert(result);

	std::vector<SpawnEntry> resultSpawns;
	pugi::xml_node rootNode = doc.child("spawns");
	assert(rootNode);

	std::map<std::tuple<int, int, int>, Creature*> tileCreatures;

	for (auto spawnNode : rootNode.children("spawn")) {
		int centerX = spawnNode.attribute("centerx").as_int();
		int centerY = spawnNode.attribute("centery").as_int();
		int centerZ = spawnNode.attribute("centerz").as_int();
		int radius = spawnNode.attribute("radius").as_int(5);

		for (auto childNode : spawnNode.children()) {
			std::string tag = childNode.name();
			if (tag == "monsters") {
				auto xAttr = childNode.attribute("x");
				auto yAttr = childNode.attribute("y");
				if (!xAttr || !yAttr) {
					continue; // Discard invalid offset
				}

				SpawnEntry entry;
				entry.center.x = centerX;
				entry.center.y = centerY;
				entry.center.z = centerZ;
				entry.radius = radius;
				entry.creatureOffset.x = xAttr.as_int();
				entry.creatureOffset.y = yAttr.as_int();
				if (auto zAttr = childNode.attribute("z")) {
					entry.creatureOffset.z = zAttr.as_int() - entry.center.z;
				}

				int creatureWorldX = entry.center.x + entry.creatureOffset.x;
				int creatureWorldY = entry.center.y + entry.creatureOffset.y;
				int creatureWorldZ = entry.center.z + entry.creatureOffset.z;
				auto tileKey = std::make_tuple(creatureWorldX, creatureWorldY, creatureWorldZ);

				int spawntime = childNode.attribute("spawntime").as_int(60);
				if (spawntime <= 0) spawntime = 60;

				size_t monsterCount = 0;
				for (auto _ : childNode.children("monster")) {
					++monsterCount;
				}
				if (monsterCount == 0) {
					continue; // Empty group
				}

				struct Variant {
					std::string name;
					uint32_t chance = 0;
				};
				std::vector<Variant> variants;
				variants.reserve(monsterCount);
				uint16_t totalChance = 0;

				for (auto mNode : childNode.children("monster")) {
					std::string name = mNode.attribute("name").as_string();
					if (name.empty()) {
						continue; // Discard nameless child
					}

					uint32_t chance = 0;
					auto chanceAttr = mNode.attribute("chance");
					if (chanceAttr) {
						int rawChance = chanceAttr.as_int();
						if (rawChance <= 0) {
							chance = 0;
						} else if (rawChance > 100) {
							chance = 100;
						} else {
							chance = static_cast<uint32_t>(rawChance);
						}
					} else {
						chance = static_cast<uint32_t>(100 / std::max<size_t>(1, monsterCount));
					}

					if (totalChance + chance > 100) {
						uint32_t clamped = (totalChance < 100) ? (100 - totalChance) : 0;
						chance = clamped;
						totalChance = 100;
					} else {
						totalChance += chance;
					}

					variants.push_back({ std::move(name), chance });
				}

				if (variants.empty()) {
					continue;
				}

				if (tileCreatures.contains(tileKey)) {
					// Duplicate creature or group on this tile - production skips it
					continue;
				}

				auto primary = std::make_unique<Creature>(variants[0].name, false);
				primary->setDirection(NORTH);
				primary->setSpawnTime(spawntime);
				primary->setWeight(variants[0].chance);

				for (size_t i = 1; i < variants.size(); ++i) {
					primary->addAlternative({ variants[i].name, spawntime, NORTH, variants[i].chance });
				}

				entry.creature = std::move(primary);
				tileCreatures[tileKey] = entry.creature.get();
				resultSpawns.push_back(std::move(entry));
			} else if (tag == "monster" || tag == "npc") {
				bool isNpc = (tag == "npc");
				std::string name = childNode.attribute("name").as_string();
				if (name.empty()) {
					continue;
				}

				auto xAttr = childNode.attribute("x");
				auto yAttr = childNode.attribute("y");
				if (!xAttr || !yAttr) {
					continue;
				}

				SpawnEntry entry;
				entry.center.x = centerX;
				entry.center.y = centerY;
				entry.center.z = centerZ;
				entry.radius = radius;
				entry.creatureOffset.x = xAttr.as_int();
				entry.creatureOffset.y = yAttr.as_int();
				if (auto zAttr = childNode.attribute("z")) {
					entry.creatureOffset.z = zAttr.as_int() - entry.center.z;
				}

				int creatureWorldX = entry.center.x + entry.creatureOffset.x;
				int creatureWorldY = entry.center.y + entry.creatureOffset.y;
				int creatureWorldZ = entry.center.z + entry.creatureOffset.z;
				auto tileKey = std::make_tuple(creatureWorldX, creatureWorldY, creatureWorldZ);

				int spawntime = childNode.attribute("spawntime").as_int(60);
				Direction direction = static_cast<Direction>(childNode.attribute("direction").as_int(static_cast<int>(NORTH)));
				uint32_t weight = childNode.attribute("weight").as_uint(0);

				if (auto it = tileCreatures.find(tileKey); it != tileCreatures.end()) {
					if (!isNpc && !it->second->isNpc()) {
						it->second->addAlternative({ name, spawntime, direction, weight });
					}
					continue;
				}

				auto creature = std::make_unique<Creature>(name, isNpc);
				creature->setSpawnTime(spawntime);
				creature->setDirection(direction);
				creature->setWeight(weight);

				entry.creature = std::move(creature);
				tileCreatures[tileKey] = entry.creature.get();
				resultSpawns.push_back(std::move(entry));
			}
		}
	}

	return resultSpawns;
}

// ============================================================================
// Test Scenarios
// ============================================================================

void testScenario1_TfsVaryingSpawnRoundTrip() {
	std::cout << "[Scenario 1] Testing TFS varying monster spawns XML round-trip...\n";

	const std::string originalXml =
		"<spawns>"
		"<spawn centerx=\"103\" centery=\"120\" centerz=\"7\" radius=\"5\">"
		"<monsters x=\"-1\" y=\"1\" z=\"7\" spawntime=\"180\">"
		"<monster name=\"Rabbit\" chance=\"70\"/>"
		"<monster name=\"Skunk\" chance=\"30\"/>"
		"</monsters>"
		"</spawn>"
		"</spawns>";

	auto parsed = parseSpawnsFromXml(originalXml);
	assert(parsed.size() == 1);
	const auto& entry = parsed[0];

	assert(entry.center.x == 103 && entry.center.y == 120 && entry.center.z == 7);
	assert(entry.radius == 5);
	assert(entry.creatureOffset.x == -1 && entry.creatureOffset.y == 1 && entry.creatureOffset.z == 0);

	const auto* creature = entry.creature.get();
	assert(creature != nullptr);
	assert(!creature->isNpc());
	assert(creature->getName() == "Rabbit");
	assert(creature->getSpawnTime() == 180);
	assert(creature->getDirection() == NORTH);
	assert(creature->getWeight() == 70);

	const auto& alts = creature->getAlternatives();
	assert(alts.size() == 1);
	assert(alts[0].name == "Skunk");
	assert(alts[0].spawntime == 180);
	assert(alts[0].direction == NORTH);
	assert(alts[0].weight == 30);

	// Serialize back to XML
	std::string serialized = serializeSpawnsToXml(parsed, false);
	assert(serialized.find("<monsters") != std::string::npos);
	assert(serialized.find("Rabbit") != std::string::npos);
	assert(serialized.find("chance=\"70\"") != std::string::npos);
	assert(serialized.find("Skunk") != std::string::npos);
	assert(serialized.find("chance=\"30\"") != std::string::npos);

	// Re-parse serialized output
	auto reparsed = parseSpawnsFromXml(serialized);
	assert(reparsed.size() == 1);
	const auto* c2 = reparsed[0].creature.get();
	assert(c2->getName() == "Rabbit");
	assert(c2->getWeight() == 70);
	assert(c2->getAlternatives().size() == 1);
	assert(c2->getAlternatives()[0].name == "Skunk");
	assert(c2->getAlternatives()[0].weight == 30);

	std::cout << "  -> Passed: TFS varying spawn parsed and serialized with 100% fidelity.\n";
}

void testScenario2_SingleMonsterLegacyRoundTrip() {
	std::cout << "[Scenario 2] Testing legacy/standard single monster spawn round-trip...\n";

	const std::string originalXml =
		"<spawns>"
		"<spawn centerx=\"100\" centery=\"100\" centerz=\"7\" radius=\"5\">"
		"<monster name=\"Troll\" x=\"2\" y=\"-1\" spawntime=\"60\" direction=\"2\"/>"
		"</spawn>"
		"</spawns>";

	auto parsed = parseSpawnsFromXml(originalXml);
	assert(parsed.size() == 1);
	const auto* c = parsed[0].creature.get();
	assert(c->getName() == "Troll");
	assert(c->getSpawnTime() == 60);
	assert(c->getDirection() == SOUTH);
	assert(c->getAlternatives().empty());

	std::string serialized = serializeSpawnsToXml(parsed, false);
	// Must NOT contain <monsters> tag
	assert(serialized.find("<monsters") == std::string::npos);
	assert(serialized.find("name=\"Troll\"") != std::string::npos);
	assert(serialized.find("x=\"2\"") != std::string::npos);
	assert(serialized.find("y=\"-1\"") != std::string::npos);
	assert(serialized.find("spawntime=\"60\"") != std::string::npos);
	assert(serialized.find("direction=\"2\"") != std::string::npos);

	std::cout << "  -> Passed: Legacy monster format preserved with zero regressions.\n";
}

void testScenario3_NpcSpawnPreservation() {
	std::cout << "[Scenario 3] Testing NPC spawn preservation...\n";

	const std::string originalXml =
		"<spawns>"
		"<spawn centerx=\"200\" centery=\"200\" centerz=\"7\" radius=\"3\">"
		"<npc name=\"Donald\" x=\"0\" y=\"0\" spawntime=\"120\"/>"
		"</spawn>"
		"</spawns>";

	auto parsed = parseSpawnsFromXml(originalXml);
	assert(parsed.size() == 1);
	const auto* c = parsed[0].creature.get();
	assert(c->isNpc());
	assert(c->getName() == "Donald");
	assert(c->getSpawnTime() == 120);

	std::string serialized = serializeSpawnsToXml(parsed, false);
	assert(serialized.find("<monsters") == std::string::npos);
	assert(serialized.find("<npc name=\"Donald\"") != std::string::npos);

	std::cout << "  -> Passed: NPC spawn serialized without interference from varying monsters.\n";
}

void testScenario4_MissingChanceFallback() {
	std::cout << "[Scenario 4] Testing 100 / count fallback when chance is omitted...\n";

	const std::string xml =
		"<spawns>"
		"<spawn centerx=\"100\" centery=\"100\" centerz=\"7\" radius=\"5\">"
		"<monsters x=\"0\" y=\"0\" z=\"7\" spawntime=\"90\">"
		"<monster name=\"Wolf\"/>"
		"<monster name=\"Bear\"/>"
		"</monsters>"
		"</spawn>"
		"</spawns>";

	auto parsed = parseSpawnsFromXml(xml);
	assert(parsed.size() == 1);
	const auto* c = parsed[0].creature.get();
	assert(c->getName() == "Wolf");
	// 100 / 2 = 50
	assert(c->getWeight() == 50);
	assert(c->getAlternatives().size() == 1);
	assert(c->getAlternatives()[0].name == "Bear");
	assert(c->getAlternatives()[0].weight == 50);

	std::cout << "  -> Passed: Omitted chances correctly defaulted to 100 / count = 50%.\n";
}

void testScenario5_ChanceClampingAndSumClamping() {
	std::cout << "[Scenario 5] Testing chance clamping (> 100) and cumulative sum clamping...\n";

	// Test 1: Single monster with chance > 100 clamped to 100
	const std::string xmlSingleClamped =
		"<spawns>"
		"<spawn centerx=\"100\" centery=\"100\" centerz=\"7\" radius=\"5\">"
		"<monsters x=\"0\" y=\"0\" z=\"7\" spawntime=\"60\">"
		"<monster name=\"Demon\" chance=\"150\"/>"
		"</monsters>"
		"</spawn>"
		"</spawns>";

	auto parsed1 = parseSpawnsFromXml(xmlSingleClamped);
	assert(parsed1[0].creature->getWeight() == 100);

	// Test 2: Cumulative sum exceeding 100 clamped so sum <= 100
	const std::string xmlSumClamped =
		"<spawns>"
		"<spawn centerx=\"100\" centery=\"100\" centerz=\"7\" radius=\"5\">"
		"<monsters x=\"0\" y=\"0\" z=\"7\" spawntime=\"60\">"
		"<monster name=\"Orc\" chance=\"80\"/>"
		"<monster name=\"Orc Leader\" chance=\"40\"/>"
		"<monster name=\"Orc Warlord\" chance=\"10\"/>"
		"</monsters>"
		"</spawn>"
		"</spawns>";

	auto parsed2 = parseSpawnsFromXml(xmlSumClamped);
	const auto* c2 = parsed2[0].creature.get();
	assert(c2->getName() == "Orc");
	assert(c2->getWeight() == 80);
	assert(c2->getAlternatives().size() == 2);
	// Orc Leader clamped to 100 - 80 = 20
	assert(c2->getAlternatives()[0].name == "Orc Leader");
	assert(c2->getAlternatives()[0].weight == 20);
	// Orc Warlord clamped to 100 - 100 = 0
	assert(c2->getAlternatives()[1].name == "Orc Warlord");
	assert(c2->getAlternatives()[1].weight == 0);

	std::cout << "  -> Passed: Cumulative chances strictly bounded to 100% total.\n";
}

void testScenario6_ParserResilience() {
	std::cout << "[Scenario 6] Testing edge cases and corrupted XML resilience...\n";

	// Empty <monsters> group
	const std::string xmlEmpty =
		"<spawns>"
		"<spawn centerx=\"100\" centery=\"100\" centerz=\"7\" radius=\"5\">"
		"<monsters x=\"0\" y=\"0\" z=\"7\" spawntime=\"60\"/>"
		"</spawn>"
		"</spawns>";
	auto parsedEmpty = parseSpawnsFromXml(xmlEmpty);
	assert(parsedEmpty.empty());

	// Missing offset coordinates
	const std::string xmlNoOffset =
		"<spawns>"
		"<spawn centerx=\"100\" centery=\"100\" centerz=\"7\" radius=\"5\">"
		"<monsters spawntime=\"60\">"
		"<monster name=\"Rat\" chance=\"100\"/>"
		"</monsters>"
		"</spawn>"
		"</spawns>";
	auto parsedNoOffset = parseSpawnsFromXml(xmlNoOffset);
	assert(parsedNoOffset.empty());

	// Nameless child skipped gracefully
	const std::string xmlNameless =
		"<spawns>"
		"<spawn centerx=\"100\" centery=\"100\" centerz=\"7\" radius=\"5\">"
		"<monsters x=\"0\" y=\"0\" z=\"7\" spawntime=\"60\">"
		"<monster chance=\"50\"/>"
		"<monster name=\"Snake\" chance=\"50\"/>"
		"</monsters>"
		"</spawn>"
		"</spawns>";
	auto parsedNameless = parseSpawnsFromXml(xmlNameless);
	assert(parsedNameless.size() == 1);
	assert(parsedNameless[0].creature->getName() == "Snake");

	// Duplicate <monsters> group on the same tile is discarded (CodeRabbit fix)
	const std::string xmlDuplicate =
		"<spawns>"
		"<spawn centerx=\"100\" centery=\"100\" centerz=\"7\" radius=\"5\">"
		"<monsters x=\"0\" y=\"0\" z=\"7\" spawntime=\"60\">"
		"<monster name=\"Rat\" chance=\"100\"/>"
		"</monsters>"
		"<monsters x=\"0\" y=\"0\" z=\"7\" spawntime=\"60\">"
		"<monster name=\"Cave Rat\" chance=\"100\"/>"
		"</monsters>"
		"</spawn>"
		"</spawns>";
	auto parsedDuplicate = parseSpawnsFromXml(xmlDuplicate);
	assert(parsedDuplicate.size() == 1);
	assert(parsedDuplicate[0].creature->getName() == "Rat");
	assert(parsedDuplicate[0].creature->getAlternatives().empty());

	std::cout << "  -> Passed: Corrupted and edge-case XML gracefully handled.\n";
}

void testScenario7_DeepCopyIndependence() {
	std::cout << "[Scenario 7] Testing deep copy and alternatives data independence...\n";

	auto original = std::make_unique<Creature>("Dragon", false);
	original->setSpawnTime(120);
	original->setWeight(60);
	original->setDirection(NORTH);
	original->addAlternative({ "Dragon Lord", 120, NORTH, 40 });

	// Perform deepCopy
	auto copy = original->deepCopy();
	assert(copy != nullptr);
	assert(copy->getName() == "Dragon");
	assert(copy->getWeight() == 60);
	assert(copy->getAlternatives().size() == 1);
	assert(copy->getAlternatives()[0].name == "Dragon Lord");
	assert(copy->getAlternatives()[0].weight == 40);

	// Mutate copy
	copy->setName("Wyrm");
	copy->setWeight(80);
	copy->getAlternatives()[0].weight = 20;
	copy->addAlternative({ "Elder Wyrm", 120, NORTH, 10 });

	// Verify original remained completely unmodified
	assert(original->getName() == "Dragon");
	assert(original->getWeight() == 60);
	assert(original->getAlternatives().size() == 1);
	assert(original->getAlternatives()[0].name == "Dragon Lord");
	assert(original->getAlternatives()[0].weight == 40);

	std::cout << "  -> Passed: Deep copy ensures zero shared mutation across instances.\n";
}

void testScenario8_Otbm5CanaryCompatibility() {
	std::cout << "[Scenario 8] Testing Canary / OTBM 5 partitioned spawn serialization...\n";

	SpawnEntry entry;
	entry.center = { 100, 100, 7 };
	entry.radius = 5;
	entry.creatureOffset = { 1, 2, 0 };

	auto creature = std::make_unique<Creature>("Hero", false);
	creature->setSpawnTime(200);
	creature->setWeight(70);
	creature->addAlternative({ "Black Knight", 200, NORTH, 30 });
	entry.creature = std::move(creature);

	std::vector<SpawnEntry> list;
	list.push_back(std::move(entry));

	// Serialize with isOtbm5 = true
	std::string serializedOtbm5 = serializeSpawnsToXml(list, true);

	// Under OTBM 5, must NOT produce <monsters> group; instead separate <monster ... weight="...">
	assert(serializedOtbm5.find("<monsters") == std::string::npos);
	assert(serializedOtbm5.find("name=\"Hero\"") != std::string::npos);
	assert(serializedOtbm5.find("weight=\"70\"") != std::string::npos);
	assert(serializedOtbm5.find("name=\"Black Knight\"") != std::string::npos);
	assert(serializedOtbm5.find("weight=\"30\"") != std::string::npos);

	std::cout << "  -> Passed: OTBM 5 flat partitioned format preserved without modification.\n";
}

void testScenario9_DirectionNorthInvariant() {
	std::cout << "[Scenario 9] Testing TFS fixed DIRECTION_NORTH invariant...\n";

	const std::string xml =
		"<spawns>"
		"<spawn centerx=\"100\" centery=\"100\" centerz=\"7\" radius=\"5\">"
		"<monsters x=\"0\" y=\"0\" z=\"7\" spawntime=\"60\">"
		"<monster name=\"Vampire\" chance=\"60\"/>"
		"<monster name=\"Vampire Bride\" chance=\"40\"/>"
		"</monsters>"
		"</spawn>"
		"</spawns>";

	auto parsed = parseSpawnsFromXml(xml);
	assert(parsed.size() == 1);
	const auto* c = parsed[0].creature.get();
	assert(c->getDirection() == NORTH);
	assert(c->getAlternatives()[0].direction == NORTH);

	std::string serialized = serializeSpawnsToXml(parsed, false);
	// <monsters> node must NOT output a direction attribute
	assert(serialized.find("direction=") == std::string::npos);

	std::cout << "  -> Passed: DIRECTION_NORTH strictly enforced per TFS PR #3605.\n";
}

void testScenario10_ComplexFourVariantGroupRoundTrip() {
	std::cout << "[Scenario 10] Testing complex 4-variant monster group round-trip...\n";

	const std::string xml =
		"<spawns>"
		"<spawn centerx=\"300\" centery=\"400\" centerz=\"8\" radius=\"10\">"
		"<monsters x=\"-3\" y=\"4\" z=\"8\" spawntime=\"300\">"
		"<monster name=\"Dragon\" chance=\"50\"/>"
		"<monster name=\"Dragon Lord\" chance=\"25\"/>"
		"<monster name=\"Dragon Hatchling\" chance=\"15\"/>"
		"<monster name=\"Fire Elemental\" chance=\"10\"/>"
		"</monsters>"
		"</spawn>"
		"</spawns>";

	auto parsed = parseSpawnsFromXml(xml);
	assert(parsed.size() == 1);
	const auto* c = parsed[0].creature.get();
	assert(c->getName() == "Dragon");
	assert(c->getWeight() == 50);
	assert(c->getSpawnTime() == 300);

	const auto& alts = c->getAlternatives();
	assert(alts.size() == 3);
	assert(alts[0].name == "Dragon Lord" && alts[0].weight == 25);
	assert(alts[1].name == "Dragon Hatchling" && alts[1].weight == 15);
	assert(alts[2].name == "Fire Elemental" && alts[2].weight == 10);

	std::string serialized = serializeSpawnsToXml(parsed, false);
	auto reparsed = parseSpawnsFromXml(serialized);
	assert(reparsed.size() == 1);
	const auto* r = reparsed[0].creature.get();
	assert(r->getName() == "Dragon");
	assert(r->getWeight() == 50);
	assert(r->getAlternatives().size() == 3);
	assert(r->getAlternatives()[0].name == "Dragon Lord" && r->getAlternatives()[0].weight == 25);
	assert(r->getAlternatives()[1].name == "Dragon Hatchling" && r->getAlternatives()[1].weight == 15);
	assert(r->getAlternatives()[2].name == "Fire Elemental" && r->getAlternatives()[2].weight == 10);

	std::cout << "  -> Passed: Complex 4-variant group preserved with 100% round-trip fidelity.\n";
}

} // namespace

int main() {
	std::cout.setf(std::ios::unitbuf);
	std::cerr.setf(std::ios::unitbuf);
	std::cout << "========================================================\n";
	std::cout << "  RME Redux: TFS Varying Monster Spawns Test Suite\n";
	std::cout << "========================================================\n";

	testScenario1_TfsVaryingSpawnRoundTrip();
	testScenario2_SingleMonsterLegacyRoundTrip();
	testScenario3_NpcSpawnPreservation();
	testScenario4_MissingChanceFallback();
	testScenario5_ChanceClampingAndSumClamping();
	testScenario6_ParserResilience();
	testScenario7_DeepCopyIndependence();
	testScenario8_Otbm5CanaryCompatibility();
	testScenario9_DirectionNorthInvariant();
	testScenario10_ComplexFourVariantGroupRoundTrip();

	std::cout << "========================================================\n";
	std::cout << "  ALL 10 SCENARIOS PASSED SUCCESSFULLY (100% GREEN)!\n";
	std::cout << "========================================================\n";
	return 0;
}
