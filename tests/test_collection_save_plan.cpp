#include "editor/operations/collection_save_plan.h"
#include "io/collection_tileset_xml.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using rme::collection::AppendMissingTilesetEntries;
using rme::collection::BuildCollectionSavePlan;
using rme::collection::CollectionBrushCategory;
using rme::collection::CollectionBrushInfo;
using rme::collection::CollectionBrushLookup;
using rme::collection::CollectionInclusionMode;
using rme::collection::ComputeCollectionSaveCounts;
using rme::collection::ResolveCollectionInclusionMode;
using rme::collection::SortTilesetEntries;

namespace {

	struct TestBrush {
		std::string name;
		uint32_t id = 0;
		CollectionBrushCategory category = CollectionBrushCategory::Other;
		std::vector<uint16_t> covered;
	};

	std::vector<CollectionBrushInfo> Infos(const std::vector<TestBrush>& brushes) {
		std::vector<CollectionBrushInfo> infos;
		for (const TestBrush& brush : brushes) {
			infos.push_back({ .name = brush.name, .id = brush.id, .category = brush.category, .coveredItemIds = brush.covered });
		}
		return infos;
	}

	std::vector<std::string> PlannedBrushNames(const std::vector<TestBrush>& brushes, const rme::collection::CollectionSavePlan& plan) {
		std::vector<std::string> names;
		for (const size_t index : plan.brushIndices) {
			names.push_back(brushes[index].name);
		}
		return names;
	}

	// Resolver over a fixed brush table, standing in for g_brushes.
	rme::collection::CollectionBrushResolver Resolver(const std::vector<TestBrush>& brushes) {
		std::map<std::string, CollectionBrushLookup, std::less<>> table;
		for (const TestBrush& brush : brushes) {
			table[brush.name] = { brush.category, brush.id };
		}
		return [table](std::string_view name) {
			if (auto it = table.find(name); it != table.end()) {
				return it->second;
			}
			return CollectionBrushLookup {};
		};
	}

	struct XmlEntry {
		std::string tag;
		std::string value;

		bool operator==(const XmlEntry&) const = default;
	};

	std::vector<XmlEntry> Entries(pugi::xml_node root) {
		std::vector<XmlEntry> entries;
		for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
			if (child.type() == pugi::node_comment) {
				entries.push_back({ "#comment", child.value() });
			} else if (child.attribute("name")) {
				entries.push_back({ child.name(), child.attribute("name").as_string() });
			} else if (child.attribute("id")) {
				entries.push_back({ child.name(), child.attribute("id").as_string() });
			} else if (child.attribute("fromid")) {
				const std::string range = std::string(child.attribute("fromid").as_string()) + "-" + child.attribute("toid").as_string();
				entries.push_back({ child.name(), range });
			} else {
				entries.push_back({ child.name(), "" });
			}
		}
		return entries;
	}

	std::string Serialize(const pugi::xml_document& doc) {
		std::ostringstream out;
		doc.save(out, "    ");
		return out.str();
	}

	// Writes a plan the way CollectionXmlService::SaveTilesetXml does and returns the parsed result.
	std::string SaveTileset(
		pugi::xml_document& doc,
		const std::vector<TestBrush>& brushes,
		const rme::collection::CollectionSavePlan& plan
	) {
		pugi::xml_node root = doc.child("tileset");
		if (!root) {
			root = doc.append_child("tileset");
			root.append_attribute("name").set_value("Test");
		}
		const std::vector<std::string> names = PlannedBrushNames(brushes, plan);
		AppendMissingTilesetEntries(root, names, plan.itemIds);
		REQUIRE(SortTilesetEntries(root, Resolver(brushes)));
		return Serialize(doc);
	}

	const std::vector<TestBrush> kCoverBrushes = {
		{ "grass", 1, CollectionBrushCategory::Ground, { 10, 30 } },
		{ "stone wall", 2, CollectionBrushCategory::Wall, { 30, 99 } },
	};

} // namespace

TEST_CASE("inclusion checkboxes resolve to a mode", "[collection]") {
	REQUIRE(ResolveCollectionInclusionMode(true, true, true) == CollectionInclusionMode::BrushesAndUncoveredItems);
	REQUIRE(ResolveCollectionInclusionMode(true, true, false) == CollectionInclusionMode::ItemsAndBrushes);
	REQUIRE(ResolveCollectionInclusionMode(true, false, true) == CollectionInclusionMode::ItemsOnly);
	REQUIRE(ResolveCollectionInclusionMode(true, false, false) == CollectionInclusionMode::ItemsOnly);
	REQUIRE(ResolveCollectionInclusionMode(false, true, true) == CollectionInclusionMode::BrushesOnly);
	REQUIRE(ResolveCollectionInclusionMode(false, true, false) == CollectionInclusionMode::BrushesOnly);
	REQUIRE_FALSE(ResolveCollectionInclusionMode(false, false, true).has_value());
	REQUIRE_FALSE(ResolveCollectionInclusionMode(false, false, false).has_value());
}

TEST_CASE("brushes and uncovered items drops items drawn by staged brushes", "[collection]") {
	const std::vector<uint16_t> items = { 10, 20, 30, 40 };
	const auto infos = Infos(kCoverBrushes);
	const auto plan = BuildCollectionSavePlan(items, infos, CollectionInclusionMode::BrushesAndUncoveredItems);

	REQUIRE(plan.itemIds == std::vector<uint16_t> { 20, 40 });
	REQUIRE(plan.brushIndices.size() == 2);
	REQUIRE(plan.stagedItemCount == 4);
	// 30 is covered by both brushes and counted once; 99 is not staged.
	REQUIRE(plan.coveredItemCount == 2);
	REQUIRE(items == std::vector<uint16_t> { 10, 20, 30, 40 });
}

TEST_CASE("without brushes every item is uncovered", "[collection]") {
	const std::vector<uint16_t> items = { 40, 10, 20 };
	const auto plan = BuildCollectionSavePlan(items, {}, CollectionInclusionMode::BrushesAndUncoveredItems);

	REQUIRE(plan.itemIds == std::vector<uint16_t> { 10, 20, 40 });
	REQUIRE(plan.brushIndices.empty());
	REQUIRE(plan.coveredItemCount == 0);
}

TEST_CASE("all items covered keeps brushes and is not an empty save", "[collection]") {
	const std::vector<uint16_t> items = { 10, 30 };
	const auto infos = Infos(kCoverBrushes);
	const auto plan = BuildCollectionSavePlan(items, infos, CollectionInclusionMode::BrushesAndUncoveredItems);

	REQUIRE(plan.itemIds.empty());
	REQUIRE(plan.brushIndices.size() == 2);
	const auto counts = ComputeCollectionSaveCounts(
		CollectionInclusionMode::BrushesAndUncoveredItems,
		plan.stagedItemCount,
		plan.coveredItemCount,
		infos.size()
	);
	REQUIRE(counts.items == 0);
	REQUIRE(counts.brushes == 2);
	REQUIRE_FALSE(counts.Empty());
}

TEST_CASE("items only, brushes only and items and brushes do not cross-filter", "[collection]") {
	const std::vector<uint16_t> items = { 30, 10, 20, 40, 20, 0 };
	const auto infos = Infos(kCoverBrushes);

	const auto itemsOnly = BuildCollectionSavePlan(items, infos, CollectionInclusionMode::ItemsOnly);
	REQUIRE(itemsOnly.itemIds == std::vector<uint16_t> { 10, 20, 30, 40 });
	REQUIRE(itemsOnly.brushIndices.empty());

	const auto brushesOnly = BuildCollectionSavePlan(items, infos, CollectionInclusionMode::BrushesOnly);
	REQUIRE(brushesOnly.itemIds.empty());
	REQUIRE(brushesOnly.brushIndices.size() == 2);

	const auto both = BuildCollectionSavePlan(items, infos, CollectionInclusionMode::ItemsAndBrushes);
	REQUIRE(both.itemIds == std::vector<uint16_t> { 10, 20, 30, 40 });
	REQUIRE(both.brushIndices.size() == 2);
}

TEST_CASE("only staged brushes contribute covered items", "[collection]") {
	// A brush the extractor filtered out (e.g. a RAW brush for item 20) is not in the staged list.
	const std::vector<uint16_t> items = { 10, 20 };
	const std::vector<TestBrush> staged = { { "grass", 1, CollectionBrushCategory::Ground, { 10 } } };
	const auto infos = Infos(staged);
	const auto plan = BuildCollectionSavePlan(items, infos, CollectionInclusionMode::BrushesAndUncoveredItems);

	REQUIRE(plan.itemIds == std::vector<uint16_t> { 20 });
}

TEST_CASE("brushes without related items cover nothing", "[collection]") {
	const std::vector<uint16_t> items = { 10, 20 };
	const std::vector<TestBrush> staged = { { "Rat", 7, CollectionBrushCategory::Creature, {} } };
	const auto infos = Infos(staged);
	const auto plan = BuildCollectionSavePlan(items, infos, CollectionInclusionMode::BrushesAndUncoveredItems);

	REQUIRE(plan.itemIds == std::vector<uint16_t> { 10, 20 });
	REQUIRE(plan.coveredItemCount == 0);
	REQUIRE(plan.brushIndices.size() == 1);
}

TEST_CASE("save counts match the plan in every mode", "[collection]") {
	const std::vector<uint16_t> items = { 10, 20, 30, 40, 50 };
	const auto infos = Infos(kCoverBrushes);
	const auto modes = {
		CollectionInclusionMode::ItemsOnly,
		CollectionInclusionMode::BrushesOnly,
		CollectionInclusionMode::ItemsAndBrushes,
		CollectionInclusionMode::BrushesAndUncoveredItems,
	};
	for (const auto mode : modes) {
		const auto plan = BuildCollectionSavePlan(items, infos, mode);
		const auto full = BuildCollectionSavePlan(items, infos, CollectionInclusionMode::BrushesAndUncoveredItems);
		const auto counts = ComputeCollectionSaveCounts(mode, full.stagedItemCount, full.coveredItemCount, infos.size());
		REQUIRE(counts.items == plan.itemIds.size());
		REQUIRE(counts.brushes == plan.brushIndices.size());
	}
}

TEST_CASE("brushes are ordered by category, then ID, then name", "[collection]") {
	std::vector<TestBrush> brushes = {
		{ "zebra rug", 1, CollectionBrushCategory::Carpet, {} },
		{ "wooden table", 2, CollectionBrushCategory::Table, {} },
		{ "Rat", 3, CollectionBrushCategory::Creature, {} },
		{ "house", 4, CollectionBrushCategory::House, {} },
		{ "archway", 5, CollectionBrushCategory::Door, {} },
		{ "Bush", 6, CollectionBrushCategory::Doodad, {} },
		{ "stone wall", 7, CollectionBrushCategory::Wall, {} },
		{ "mystery", 8, CollectionBrushCategory::Other, {} },
		{ "sand", 9, CollectionBrushCategory::Ground, {} },
		{ "Grass", 10, CollectionBrushCategory::Ground, {} },
		{ "dirt", 11, CollectionBrushCategory::Ground, {} },
		{ "grass", 12, CollectionBrushCategory::Ground, {} },
		{ "bench", 13, CollectionBrushCategory::Doodad, {} },
	};
	const std::vector<std::string> expected = {
		"sand", "Grass", "dirt", "grass", "stone wall", "Bush", "bench", "zebra rug", "wooden table", "archway", "Rat", "house", "mystery"
	};

	std::ranges::sort(brushes, [](const TestBrush& a, const TestBrush& b) { return a.id < b.id; });
	do {
		const auto infos = Infos(brushes);
		const auto plan = BuildCollectionSavePlan({}, infos, CollectionInclusionMode::BrushesOnly);
		REQUIRE(PlannedBrushNames(brushes, plan) == expected);
		std::ranges::rotate(brushes, brushes.begin() + 5);
	} while (brushes.front().id != 1);
}

TEST_CASE("case-variant names are ordered by ID", "[collection]") {
	const std::vector<TestBrush> brushes = {
		{ "Grass", 20, CollectionBrushCategory::Ground, {} },
		{ "grass", 10, CollectionBrushCategory::Ground, {} },
	};
	const auto infos = Infos(brushes);
	const auto plan = BuildCollectionSavePlan({}, infos, CollectionInclusionMode::BrushesOnly);
	REQUIRE(PlannedBrushNames(brushes, plan) == std::vector<std::string> { "grass", "Grass" });
}

TEST_CASE("ID orders brushes inside a category before their names", "[collection]") {
	const std::vector<TestBrush> brushes = {
		{ "alpha", 30, CollectionBrushCategory::Ground, {} },
		{ "zeta", 10, CollectionBrushCategory::Ground, {} },
	};
	const auto infos = Infos(brushes);
	const auto plan = BuildCollectionSavePlan({}, infos, CollectionInclusionMode::BrushesOnly);
	REQUIRE(PlannedBrushNames(brushes, plan) == std::vector<std::string> { "zeta", "alpha" });
}

TEST_CASE("equal sort IDs fall back to the case-insensitive name", "[collection]") {
	const std::vector<TestBrush> brushes = {
		{ "Zeta", 0, CollectionBrushCategory::Other, {} },
		{ "alpha", 0, CollectionBrushCategory::Other, {} },
	};
	const auto infos = Infos(brushes);
	const auto plan = BuildCollectionSavePlan({}, infos, CollectionInclusionMode::BrushesOnly);
	REQUIRE(PlannedBrushNames(brushes, plan) == std::vector<std::string> { "alpha", "Zeta" });
}

TEST_CASE("equal sort IDs and case-insensitive names fall back to the exact name", "[collection]") {
	const std::vector<TestBrush> brushes = {
		{ "grass", 5, CollectionBrushCategory::Other, {} },
		{ "Grass", 5, CollectionBrushCategory::Other, {} },
	};
	const auto infos = Infos(brushes);
	const auto plan = BuildCollectionSavePlan({}, infos, CollectionInclusionMode::BrushesOnly);
	REQUIRE(PlannedBrushNames(brushes, plan) == std::vector<std::string> { "Grass", "grass" });
}

TEST_CASE("saved tileset has no item covered by an included brush", "[collection][xml]") {
	const std::vector<uint16_t> items = { 40, 30, 20, 10 };
	const auto infos = Infos(kCoverBrushes);
	const auto plan = BuildCollectionSavePlan(items, infos, CollectionInclusionMode::BrushesAndUncoveredItems);

	pugi::xml_document saved;
	const std::string xml = SaveTileset(saved, kCoverBrushes, plan);

	pugi::xml_document parsed;
	REQUIRE(parsed.load(xml.c_str()));
	const pugi::xml_node root = parsed.child("tileset");
	REQUIRE(root);
	for (pugi::xml_node item = root.child("item"); item; item = item.next_sibling("item")) {
		const auto id = static_cast<uint16_t>(item.attribute("id").as_uint());
		for (const TestBrush& brush : kCoverBrushes) {
			REQUIRE(std::ranges::find(brush.covered, id) == brush.covered.end());
		}
	}
	REQUIRE(Entries(root) == std::vector<XmlEntry> {
		{ "brush", "grass" },
		{ "brush", "stone wall" },
		{ "item", "20" },
		{ "item", "40" },
	});
}

TEST_CASE("tileset output does not depend on staging order", "[collection][xml]") {
	std::vector<TestBrush> brushes = {
		{ "stone wall", 2, CollectionBrushCategory::Wall, { 30 } },
		{ "bush", 3, CollectionBrushCategory::Doodad, { 50 } },
		{ "grass", 1, CollectionBrushCategory::Ground, { 10 } },
	};
	std::vector<uint16_t> items = { 70, 10, 60, 30, 50, 20 };

	std::string reference;
	for (int round = 0; round < 6; ++round) {
		const auto infos = Infos(brushes);
		const auto plan = BuildCollectionSavePlan(items, infos, CollectionInclusionMode::BrushesAndUncoveredItems);
		pugi::xml_document doc;
		const std::string xml = SaveTileset(doc, brushes, plan);
		if (round == 0) {
			reference = xml;
			REQUIRE(Entries(doc.child("tileset")) == std::vector<XmlEntry> {
				{ "brush", "grass" },
				{ "brush", "stone wall" },
				{ "brush", "bush" },
				{ "item", "20" },
				{ "item", "60" },
				{ "item", "70" },
			});
		} else {
			REQUIRE(xml == reference);
		}
		std::ranges::rotate(brushes, brushes.begin() + 1);
		std::ranges::rotate(items, items.begin() + 1);
		std::ranges::reverse(items);
	}
}

TEST_CASE("tileset brush nodes sort by ID within a category", "[collection][xml]") {
	const std::vector<TestBrush> known = {
		{ "grass", 30, CollectionBrushCategory::Ground, {} },
		{ "Grass", 20, CollectionBrushCategory::Ground, {} },
		{ "Bush", 1, CollectionBrushCategory::Doodad, {} },
		{ "apple tree", 2, CollectionBrushCategory::Doodad, {} },
	};

	pugi::xml_document doc;
	REQUIRE(doc.load(R"(<tileset name="Case">
    <brush name="Bush"/>
    <brush name="grass"/>
    <brush name="apple tree"/>
    <brush name="Grass"/>
</tileset>)"));
	pugi::xml_node root = doc.child("tileset");
	REQUIRE(SortTilesetEntries(root, Resolver(known)));

	REQUIRE(Entries(root) == std::vector<XmlEntry> {
		{ "brush", "Grass" },
		{ "brush", "grass" },
		{ "brush", "Bush" },
		{ "brush", "apple tree" },
	});
}

TEST_CASE("append merges into an unsorted tileset without losing entries", "[collection][xml]") {
	const std::vector<TestBrush> known = {
		{ "grass", 1, CollectionBrushCategory::Ground, {} },
		{ "stone wall", 2, CollectionBrushCategory::Wall, {} },
		{ "bush", 3, CollectionBrushCategory::Doodad, {} },
		{ "rug", 4, CollectionBrushCategory::Carpet, {} },
		{ "Rat", 5, CollectionBrushCategory::Creature, {} },
		{ "dirt", 6, CollectionBrushCategory::Ground, {} },
	};

	pugi::xml_document doc;
	REQUIRE(doc.load(R"(<?xml version="1.0" encoding="utf-8"?>
<tileset name="Existing">
    <item id="900"/>
    <!-- hand-written note -->
    <brush name="bush"/>
    <item fromid="300" toid="305"/>
    <creature name="Rat"/>
    <unknown flag="1"/>
    <brush name="missing brush"/>
    <brush name="stone wall"/>
</tileset>)",
		pugi::parse_default | pugi::parse_comments | pugi::parse_declaration));
	pugi::xml_node root = doc.child("tileset");

	// "Rat" is already listed as <creature>, so it must not be appended again as <brush>.
	const std::vector<std::string> newBrushes = { "rug", "bush", "grass", "dirt", "Rat" };
	const std::vector<uint16_t> newItems = { 302, 900, 100, 0 };
	AppendMissingTilesetEntries(root, newBrushes, newItems);
	REQUIRE(SortTilesetEntries(root, Resolver(known)));

	REQUIRE(Entries(root) == std::vector<XmlEntry> {
		{ "#comment", " hand-written note " },
		{ "unknown", "" },
		{ "brush", "grass" },
		{ "brush", "dirt" },
		{ "brush", "stone wall" },
		{ "brush", "bush" },
		{ "brush", "rug" },
		{ "creature", "Rat" },
		{ "brush", "missing brush" },
		{ "item", "100" },
		{ "item", "300-305" },
		{ "item", "900" },
	});
	REQUIRE(root.child("unknown").attribute("flag").as_int() == 1);

	// Appending the same content again changes nothing.
	const std::string once = Serialize(doc);
	AppendMissingTilesetEntries(root, newBrushes, newItems);
	REQUIRE(SortTilesetEntries(root, Resolver(known)));
	REQUIRE(Serialize(doc) == once);
}
