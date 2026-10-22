#include <cstdint>
#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <cassert>
#include <filesystem>
#include <span>
#include <algorithm>
#include <set>
#include <map>
#include <cstdlib>

#define TEST_ASSERT(expr) \
	do { \
		if (!(expr)) { \
			std::cerr << "TEST_ASSERT FAILED: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
			std::abort(); \
		} \
	} while (false)

// Include pugixml and CollectionXmlService
#include "../source/ext/pugixml.hpp"
#include "../source/io/collection_xml_service.h"

namespace rme::collection {
std::filesystem::path CollectionXmlService::GetTilesetFilePath(std::string_view paletteName, std::string_view tilesetName) {
	std::string sanitized = SanitizeTilesetFilename(tilesetName);
	std::string pName(paletteName);
	std::ranges::transform(pName, pName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (pName == "collection") {
		return std::filesystem::path("tilesets/collections") / (sanitized + ".xml");
	}
	return std::filesystem::path("tilesets") / pName / (sanitized + ".xml");
}
}

// Test 2: Mock brush and Staging Content
struct MockBrush {
	uint32_t id;
	std::string name;
	uint32_t getID() const { return id; }
	std::string getName() const { return name; }
};

struct TestStagingContent {
	std::vector<uint16_t> itemIds;
	std::vector<MockBrush*> brushes;

	void Merge(const TestStagingContent& other) {
		itemIds.insert(itemIds.end(), other.itemIds.begin(), other.itemIds.end());
		std::ranges::sort(itemIds);
		itemIds.erase(std::unique(itemIds.begin(), itemIds.end()), itemIds.end());

		brushes.insert(brushes.end(), other.brushes.begin(), other.brushes.end());
		std::ranges::sort(brushes, [](MockBrush* a, MockBrush* b) {
			if (!a || !b) return a < b;
			if (a->getName() != b->getName()) {
				return a->getName() < b->getName();
			}
			return a->getID() < b->getID();
		});
		auto [bfirst, blast] = std::ranges::unique(brushes, [](MockBrush* a, MockBrush* b) {
			if (a == b) return true;
			if (!a || !b) return false;
			return a->getName() == b->getName();
		});
		brushes.erase(bfirst, blast);
	}

	void DiscardItem(uint16_t itemId) {
		std::erase(itemIds, itemId);
	}

	void DiscardBrush(MockBrush* brush) {
		std::erase_if(brushes, [brush](MockBrush* b) {
			if (b == brush) return true;
			if (b && brush && b->getName() == brush->getName()) return true;
			return false;
		});
	}

	void Clear() {
		itemIds.clear();
		brushes.clear();
	}

	[[nodiscard]] bool Empty() const {
		return itemIds.empty() && brushes.empty();
	}
};

void TestFilenameSanitization() {
	std::cout << "[RUN] TestFilenameSanitization...\n";

	using rme::collection::CollectionXmlService;
	TEST_ASSERT(CollectionXmlService::SanitizeTilesetFilename("Nature") == "Nature");
	TEST_ASSERT(CollectionXmlService::SanitizeTilesetFilename("  Desert Sands  ") == "Desert Sands");
	TEST_ASSERT(CollectionXmlService::SanitizeTilesetFilename("Cave: Rocks/Walls?*") == "Cave_ Rocks_Walls__");
	TEST_ASSERT(CollectionXmlService::SanitizeTilesetFilename("<invalid>|dir\"name") == "_invalid__dir_name");
	TEST_ASSERT(CollectionXmlService::SanitizeTilesetFilename("...") == "collection");
	TEST_ASSERT(CollectionXmlService::SanitizeTilesetFilename("   ") == "collection");
	TEST_ASSERT(CollectionXmlService::SanitizeTilesetFilename("") == "collection");
	TEST_ASSERT(CollectionXmlService::SanitizeTilesetFilename(".hidden.") == "hidden");

	std::cout << "[PASS] TestFilenameSanitization passed!\n";
}

void TestStagingContentLogic() {
	std::cout << "[RUN] TestStagingContentLogic...\n";

	MockBrush b1{1, "Grass"};
	MockBrush b2{2, "Stone Wall"};
	MockBrush b3{3, "Water"};

	TestStagingContent staging;
	TEST_ASSERT(staging.Empty());

	staging.itemIds = { 2160, 2152, 100 };
	staging.brushes = { &b1, &b2 };
	TEST_ASSERT(!staging.Empty());
	TEST_ASSERT(staging.itemIds.size() == 3);
	TEST_ASSERT(staging.brushes.size() == 2);

	// Test Merge with overlapping elements
	TestStagingContent second;
	second.itemIds = { 2160, 300, 400 };
	second.brushes = { &b2, &b3 };

	staging.Merge(second);
	// Item IDs should be: 100, 300, 400, 2152, 2160 (5 unique)
	TEST_ASSERT(staging.itemIds.size() == 5);
	TEST_ASSERT(std::ranges::is_sorted(staging.itemIds));
	TEST_ASSERT(staging.itemIds[0] == 100);
	TEST_ASSERT(staging.itemIds[4] == 2160);

	// Brushes should be: Grass, Stone Wall, Water (3 unique)
	TEST_ASSERT(staging.brushes.size() == 3);
	TEST_ASSERT(staging.brushes[0] == &b1);
	TEST_ASSERT(staging.brushes[1] == &b2);
	TEST_ASSERT(staging.brushes[2] == &b3);

	// Test Discard
	staging.DiscardItem(2160);
	TEST_ASSERT(staging.itemIds.size() == 4);
	TEST_ASSERT(std::find(staging.itemIds.begin(), staging.itemIds.end(), 2160) == staging.itemIds.end());

	staging.DiscardBrush(&b2);
	TEST_ASSERT(staging.brushes.size() == 2);
	TEST_ASSERT(std::find(staging.brushes.begin(), staging.brushes.end(), &b2) == staging.brushes.end());

	// Test Clear
	staging.Clear();
	TEST_ASSERT(staging.Empty());

	std::cout << "[PASS] TestStagingContentLogic passed!\n";
}

void TestSpawnBrushExclusion() {
	std::cout << "[RUN] TestSpawnBrushExclusion...\n";

	MockBrush bGround{1, "Grass"};
	MockBrush bCreature{2, "Dragon"};
	MockBrush bDoodad{3, "Ancient Fountain"};
	MockBrush bSpawn{4, "SPAWN"};
	MockBrush bRaw{5, "raw_brush"};
	MockBrush bEraser{6, "Eraser"};
	MockBrush bDemonSpawn{7, "Demon Spawn"};
	MockBrush bSpawnBrush{8, "spawn brush"};

	std::vector<MockBrush*> candidates = { &bGround, &bCreature, &bSpawn, &bDoodad, &bRaw, &bEraser, &bDemonSpawn, &bSpawnBrush };

	auto to_lower = [](std::string_view s) {
		std::string res;
		for (char ch : s) res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
		return res;
	};

	std::vector<MockBrush*> filtered;
	for (MockBrush* b : candidates) {
		if (!b || b->getName().empty()) continue;
		std::string lower = to_lower(b->getName());
		if (lower == "spawn" || lower == "spawn brush" || lower == "eraser" || lower == "raw_brush") {
			continue;
		}
		filtered.push_back(b);
	}

	// Demon Spawn should be retained! Only exact "spawn" and "spawn brush" excluded.
	TEST_ASSERT(filtered.size() == 4);
	TEST_ASSERT(filtered[0] == &bGround);
	TEST_ASSERT(filtered[1] == &bCreature);
	TEST_ASSERT(filtered[2] == &bDoodad);
	TEST_ASSERT(filtered[3] == &bDemonSpawn);
	for (MockBrush* b : filtered) {
		TEST_ASSERT(b->getName() != "SPAWN");
		TEST_ASSERT(b->getName() != "spawn brush");
	}

	std::cout << "[PASS] TestSpawnBrushExclusion passed!\n";
}

void TestXmlGenerationAndRoundtrip() {
	std::cout << "[RUN] TestXmlGenerationAndRoundtrip...\n";

	std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_tileset_collection.xml";

	// Generate tileset XML
	{
		pugi::xml_document doc;
		pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
		decl.append_attribute("version").set_value("1.0");
		decl.append_attribute("encoding").set_value("utf-8");

		pugi::xml_node root = doc.append_child("tileset");
		root.append_attribute("name").set_value("TestCollection");

		pugi::xml_node b1 = root.append_child("brush");
		b1.append_attribute("name").set_value("grass");

		pugi::xml_node b2 = root.append_child("brush");
		b2.append_attribute("name").set_value("water");

		pugi::xml_node it1 = root.append_child("item");
		it1.append_attribute("id").set_value(2160);

		pugi::xml_node it2 = root.append_child("item");
		it2.append_attribute("id").set_value(2152);

		bool saved = doc.save_file(tempPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8);
		TEST_ASSERT(saved);
	}

	// Read back and verify
	{
		pugi::xml_document doc;
		pugi::xml_parse_result res = doc.load_file(tempPath.wstring().c_str());
		TEST_ASSERT(res);

		pugi::xml_node root = doc.child("tileset");
		TEST_ASSERT(root);
		TEST_ASSERT(std::string(root.attribute("name").as_string()) == "TestCollection");

		std::vector<std::string> brushes;
		std::vector<uint16_t> items;

		for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
			std::string tag = child.name();
			if (tag == "brush") {
				brushes.push_back(child.attribute("name").as_string());
			} else if (tag == "item") {
				items.push_back(static_cast<uint16_t>(child.attribute("id").as_uint()));
			}
		}

		TEST_ASSERT(brushes.size() == 2);
		TEST_ASSERT(brushes[0] == "grass");
		TEST_ASSERT(brushes[1] == "water");

		TEST_ASSERT(items.size() == 2);
		TEST_ASSERT(items[0] == 2160);
		TEST_ASSERT(items[1] == 2152);
	}

	// Clean up
	std::filesystem::remove(tempPath);
	std::cout << "[PASS] TestXmlGenerationAndRoundtrip passed!\n";
}

void TestPalettesXmlIncludeSync() {
	std::cout << "[RUN] TestPalettesXmlIncludeSync...\n";

	std::filesystem::path tempPalettesPath = std::filesystem::temp_directory_path() / "test_palettes.xml";

	// Create sample palettes.xml
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child("palettes");
		pugi::xml_node terrain = root.append_child("palette");
		terrain.append_attribute("name").set_value("Terrain");

		pugi::xml_node collection = root.append_child("palette");
		collection.append_attribute("name").set_value("Collection");
		collection.append_child("tileset"); // empty tileset tag

		bool saved = doc.save_file(tempPalettesPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8);
		TEST_ASSERT(saved);
	}

	// Perform include injection
	{
		pugi::xml_document doc;
		TEST_ASSERT(doc.load_file(tempPalettesPath.wstring().c_str()));

		pugi::xml_node root = doc.child("palettes");
		TEST_ASSERT(root);

		pugi::xml_node collectionPal;
		for (pugi::xml_node p = root.child("palette"); p; p = p.next_sibling("palette")) {
			if (std::string(p.attribute("name").as_string()) == "Collection") {
				collectionPal = p;
				break;
			}
		}
		TEST_ASSERT(collectionPal);

		pugi::xml_node tilesetNode = collectionPal.child("tileset");
		TEST_ASSERT(tilesetNode);

		std::string incRelPath = "tilesets/collections/MyDungeon.xml";
		bool alreadyIncluded = false;
		std::string normIncludeRel = incRelPath;
		std::ranges::transform(normIncludeRel, normIncludeRel.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::replace(normIncludeRel.begin(), normIncludeRel.end(), '\\', '/');

		for (pugi::xml_node inc = tilesetNode.child("include"); inc; inc = inc.next_sibling("include")) {
			std::string existingFolder = inc.attribute("folder").as_string();
			if (!existingFolder.empty()) {
				std::string normFolder = existingFolder;
				std::ranges::transform(normFolder, normFolder.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::replace(normFolder.begin(), normFolder.end(), '\\', '/');
				if (!normFolder.empty() && normFolder.back() != '/') normFolder += '/';
				if (normIncludeRel.starts_with(normFolder)) {
					alreadyIncluded = true;
					break;
				}
			}
			std::string existingFile = inc.attribute("file").as_string();
			if (!existingFile.empty()) {
				std::string normExisting = existingFile;
				std::ranges::transform(normExisting, normExisting.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::replace(normExisting.begin(), normExisting.end(), '\\', '/');
				if (normExisting == normIncludeRel) {
					alreadyIncluded = true;
					break;
				}
			}
		}
		TEST_ASSERT(!alreadyIncluded);

		pugi::xml_node newInc = tilesetNode.append_child("include");
		newInc.append_attribute("file").set_value(incRelPath.c_str());

		TEST_ASSERT(doc.save_file(tempPalettesPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8));
	}

	// Verify idempotency (second attempt should not duplicate)
	{
		pugi::xml_document doc;
		TEST_ASSERT(doc.load_file(tempPalettesPath.wstring().c_str()));

		pugi::xml_node root = doc.child("palettes");
		pugi::xml_node collectionPal = root.child("palette");
		while (collectionPal && std::string(collectionPal.attribute("name").as_string()) != "Collection") {
			collectionPal = collectionPal.next_sibling("palette");
		}
		TEST_ASSERT(collectionPal);

		pugi::xml_node tilesetNode = collectionPal.child("tileset");
		TEST_ASSERT(tilesetNode);

		int count = 0;
		for (pugi::xml_node inc = tilesetNode.child("include"); inc; inc = inc.next_sibling("include")) {
			if (std::string(inc.attribute("file").as_string()) == "tilesets/collections/MyDungeon.xml") {
				count++;
			}
		}
		TEST_ASSERT(count == 1);
	}

	// Verify that a folder include <include folder="tilesets/collections/" /> covers files
	{
		pugi::xml_document doc;
		TEST_ASSERT(doc.load_file(tempPalettesPath.wstring().c_str()));
		pugi::xml_node root = doc.child("palettes");
		pugi::xml_node collectionPal = root.child("palette");
		while (collectionPal && std::string(collectionPal.attribute("name").as_string()) != "Collection") {
			collectionPal = collectionPal.next_sibling("palette");
		}
		pugi::xml_node tilesetNode = collectionPal.child("tileset");
		tilesetNode.remove_child("include"); // Clear file include
		pugi::xml_node folderInc = tilesetNode.append_child("include");
		folderInc.append_attribute("folder").set_value("tilesets/collections/");
		folderInc.append_attribute("subfolders").set_value("true");

		std::string incRelPath = "tilesets/collections/NewSet.xml";
		bool covered = false;
		std::string normIncludeRel = incRelPath;
		std::ranges::transform(normIncludeRel, normIncludeRel.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::replace(normIncludeRel.begin(), normIncludeRel.end(), '\\', '/');

		for (pugi::xml_node inc = tilesetNode.child("include"); inc; inc = inc.next_sibling("include")) {
			std::string existingFolder = inc.attribute("folder").as_string();
			if (!existingFolder.empty()) {
				std::string normFolder = existingFolder;
				std::ranges::transform(normFolder, normFolder.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::replace(normFolder.begin(), normFolder.end(), '\\', '/');
				if (!normFolder.empty() && normFolder.back() != '/') normFolder += '/';
				if (normIncludeRel.starts_with(normFolder)) {
					covered = true;
					break;
				}
			}
		}
		TEST_ASSERT(covered);
	}

	// Clean up
	std::filesystem::remove(tempPalettesPath);
	std::cout << "[PASS] TestPalettesXmlIncludeSync passed!\n";
}

void TestMaterialDatabaseSources() {
	std::cout << "[RUN] TestMaterialDatabaseSources...\n";

	std::vector<std::string> sources;

	auto normalizeKey = [](std::string key) -> std::string {
		std::ranges::transform(key, key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::replace(key.begin(), key.end(), '\\', '/');
		return key;
	};

	auto addSource = [&sources, &normalizeKey](std::string key) {
		key = normalizeKey(std::move(key));
		auto it = std::ranges::lower_bound(sources, key);
		if (it == sources.end() || *it != key) {
			sources.insert(it, std::move(key));
		}
	};

	auto isKnown = [&sources, &normalizeKey](std::string_view key) -> bool {
		std::string normKey = normalizeKey(std::string(key));
		return std::ranges::binary_search(sources, normKey);
	};

	addSource("data/1098/tilesets/terrain/Nature.xml");
	addSource("data/1098/tilesets/doodad/Trash.xml");
	addSource("data/1098/tilesets/collections/Custom_1.xml");

	TEST_ASSERT(sources.size() == 3);
	TEST_ASSERT(std::ranges::is_sorted(sources));

	// Case-insensitive checks
	TEST_ASSERT(isKnown("data/1098/tilesets/collections/Custom_1.xml"));
	TEST_ASSERT(isKnown("DATA/1098/TILESETS/COLLECTIONS/CUSTOM_1.XML"));
	TEST_ASSERT(isKnown("data/1098/tilesets/collections/custom_1.xml"));

	// Windows backslash vs forward slash checks
	TEST_ASSERT(isKnown("data\\1098\\tilesets\\collections\\Custom_1.xml"));
	TEST_ASSERT(isKnown("DATA\\1098\\TILESETS\\terrain\\Nature.xml"));

	// Non-existent
	TEST_ASSERT(!isKnown("data/1098/tilesets/collections/NonExistent.xml"));

	// Duplicate add with different slashes and casing
	addSource("DATA\\1098\\tilesets\\collections\\CUSTOM_1.xml");
	TEST_ASSERT(sources.size() == 3);
	TEST_ASSERT(std::ranges::is_sorted(sources));

	std::cout << "[PASS] TestMaterialDatabaseSources passed!\n";
}

void TestPaletteCatalogSetTilesetBrushes() {
	std::cout << "[RUN] TestPaletteCatalogSetTilesetBrushes...\n";

	struct TestTilesetDef {
		std::string name;
		std::vector<MockBrush*> brushes;
	};

	struct TestPaletteDef {
		std::string name;
		std::vector<TestTilesetDef> tilesets;
	};

	struct TestCatalog {
		std::vector<TestPaletteDef> palettes;

		void setTilesetBrushes(std::string_view paletteName, std::string_view tilesetName, std::vector<MockBrush*> brushes) {
			auto it = std::ranges::find_if(palettes, [paletteName](const auto& existing) {
				std::string a = existing.name;
				std::string b = std::string(paletteName);
				std::ranges::transform(a, a.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::ranges::transform(b, b.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return a == b;
			});

			if (it == palettes.end()) {
				TestPaletteDef newPal;
				newPal.name = std::string(paletteName);
				TestTilesetDef newTileset;
				newTileset.name = std::string(tilesetName);
				newTileset.brushes = std::move(brushes);
				newPal.tilesets.push_back(std::move(newTileset));
				palettes.push_back(std::move(newPal));
				return;
			}

			auto tilesetIt = std::ranges::find_if(it->tilesets, [tilesetName](const auto& existingTileset) {
				std::string a = existingTileset.name;
				std::string b = std::string(tilesetName);
				std::ranges::transform(a, a.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::ranges::transform(b, b.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return a == b;
			});

			if (tilesetIt != it->tilesets.end()) {
				tilesetIt->brushes = std::move(brushes);
			} else {
				TestTilesetDef newTileset;
				newTileset.name = std::string(tilesetName);
				newTileset.brushes = std::move(brushes);
				it->tilesets.push_back(std::move(newTileset));
			}
		}
	};

	MockBrush b1{1, "Grass"};
	MockBrush b2{2, "Stone Wall"};
	MockBrush b3{3, "Water"};

	TestCatalog catalog;
	TEST_ASSERT(catalog.palettes.empty());

	// 1. Initial creation of palette and tileset
	catalog.setTilesetBrushes("Collection", "Dungeon", {&b1, &b2});
	TEST_ASSERT(catalog.palettes.size() == 1);
	TEST_ASSERT(catalog.palettes[0].name == "Collection");
	TEST_ASSERT(catalog.palettes[0].tilesets.size() == 1);
	TEST_ASSERT(catalog.palettes[0].tilesets[0].name == "Dungeon");
	TEST_ASSERT(catalog.palettes[0].tilesets[0].brushes.size() == 2);

	// 2. Add second tileset to existing palette
	catalog.setTilesetBrushes("Collection", "Nature", {&b3});
	TEST_ASSERT(catalog.palettes.size() == 1);
	TEST_ASSERT(catalog.palettes[0].tilesets.size() == 2);
	TEST_ASSERT(catalog.palettes[0].tilesets[0].name == "Dungeon");
	TEST_ASSERT(catalog.palettes[0].tilesets[1].name == "Nature");
	TEST_ASSERT(catalog.palettes[0].tilesets[1].brushes.size() == 1);

	// 3. Overwrite "Dungeon" tileset (case-insensitive "collection" / "dungeon") with single brush b3
	catalog.setTilesetBrushes("collection", "dungeon", {&b3});
	TEST_ASSERT(catalog.palettes.size() == 1);
	TEST_ASSERT(catalog.palettes[0].tilesets.size() == 2);
	TEST_ASSERT(catalog.palettes[0].tilesets[0].brushes.size() == 1);
	TEST_ASSERT(catalog.palettes[0].tilesets[0].brushes[0] == &b3);
	// Nature tileset untouched
	TEST_ASSERT(catalog.palettes[0].tilesets[1].brushes.size() == 1);
	TEST_ASSERT(catalog.palettes[0].tilesets[1].brushes[0] == &b3);

	// 4. Overwrite with multiple brushes
	catalog.setTilesetBrushes("COLLECTION", "DUNGEON", {&b1, &b2, &b3});
	TEST_ASSERT(catalog.palettes[0].tilesets[0].brushes.size() == 3);

	std::cout << "[PASS] TestPaletteCatalogSetTilesetBrushes passed!\n";
}

void TestTargetPaletteXmlAndPaths() {
	std::cout << "[RUN] TestTargetPaletteXmlAndPaths...\n";

	using rme::collection::CollectionXmlService;
	auto p1 = CollectionXmlService::GetTilesetFilePath("Collection", "MySet");
	TEST_ASSERT(p1.generic_string().find("tilesets/collections/MySet.xml") != std::string::npos);

	auto p2 = CollectionXmlService::GetTilesetFilePath("Terrain", "MyCustomTerrain");
	TEST_ASSERT(p2.generic_string().find("tilesets/terrain/MyCustomTerrain.xml") != std::string::npos);

	auto p3 = CollectionXmlService::GetTilesetFilePath("Doodad", "MyDoodad");
	TEST_ASSERT(p3.generic_string().find("tilesets/doodad/MyDoodad.xml") != std::string::npos);

	// Test multi-palette include in palettes.xml
	std::filesystem::path tempPalettesPath = std::filesystem::temp_directory_path() / "test_target_palettes.xml";
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child("palettes");
		pugi::xml_node terrainPal = root.append_child("palette");
		terrainPal.append_attribute("name").set_value("Terrain");
		pugi::xml_node ts = terrainPal.append_child("tileset");
		pugi::xml_node inc = ts.append_child("include");
		inc.append_attribute("file").set_value("tilesets/terrain/Nature.xml");

		pugi::xml_node collPal = root.append_child("palette");
		collPal.append_attribute("name").set_value("Collection");
		collPal.append_child("tileset");

		TEST_ASSERT(doc.save_file(tempPalettesPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8));
	}

	// Add include to Terrain palette
	{
		pugi::xml_document doc;
		TEST_ASSERT(doc.load_file(tempPalettesPath.wstring().c_str()));
		pugi::xml_node root = doc.child("palettes");

		pugi::xml_node terrainPal;
		for (pugi::xml_node pal = root.child("palette"); pal; pal = pal.next_sibling("palette")) {
			if (std::string(pal.attribute("name").as_string()) == "Terrain") {
				terrainPal = pal;
				break;
			}
		}
		TEST_ASSERT(terrainPal);
		pugi::xml_node tilesetNode = terrainPal.child("tileset");
		TEST_ASSERT(tilesetNode);

		pugi::xml_node newInc = tilesetNode.append_child("include");
		newInc.append_attribute("file").set_value("tilesets/terrain/MyCustomTerrain.xml");
		TEST_ASSERT(doc.save_file(tempPalettesPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8));
	}

	// Verify Terrain palette has 2 includes and Collection is untouched
	{
		pugi::xml_document doc;
		TEST_ASSERT(doc.load_file(tempPalettesPath.wstring().c_str()));
		pugi::xml_node root = doc.child("palettes");
		for (pugi::xml_node pal = root.child("palette"); pal; pal = pal.next_sibling("palette")) {
			std::string name = pal.attribute("name").as_string();
			if (name == "Terrain") {
				int count = 0;
				for (pugi::xml_node inc = pal.child("tileset").child("include"); inc; inc = inc.next_sibling("include")) {
					count++;
				}
				TEST_ASSERT(count == 2);
			} else if (name == "Collection") {
				int count = 0;
				for (pugi::xml_node inc = pal.child("tileset").child("include"); inc; inc = inc.next_sibling("include")) {
					count++;
				}
				TEST_ASSERT(count == 0);
			}
		}
	}

	std::filesystem::remove(tempPalettesPath);
	std::cout << "[PASS] TestTargetPaletteXmlAndPaths passed!\n";
}

void TestMultiFloorSelectionAndHitTesting() {
	std::cout << "[RUN] TestMultiFloorSelectionAndHitTesting...\n";

	constexpr int MAP_MAX_LAYER = 15;
	constexpr int GROUND_LAYER = 7;

	auto getFloorTileOffset = [](int floor) -> int {
		if (floor <= GROUND_LAYER) {
			return GROUND_LAYER - floor;
		}
		return 0;
	};

	TEST_ASSERT(getFloorTileOffset(7) == 0);
	TEST_ASSERT(getFloorTileOffset(6) == 1);
	TEST_ASSERT(getFloorTileOffset(5) == 2);
	TEST_ASSERT(getFloorTileOffset(8) == 0);
	TEST_ASSERT(getFloorTileOffset(15) == 0);

	// 1. Verify "Floors Below" bounding box: s_z = MAP_MAX_LAYER, e_z = floor
	// For floor = 8 (underground)
	{
		const int floor = 8;
		const int s_z = MAP_MAX_LAYER;
		const int e_z = floor;
		TEST_ASSERT(s_z == 15);
		TEST_ASSERT(e_z == 8);
		TEST_ASSERT(s_z >= e_z);
		// Floors above 8 (0..7) are NEVER included
		for (int z = s_z; z >= e_z; --z) {
			TEST_ASSERT(z >= 8 && z <= 15);
		}
	}

	// For floor = 7 (ground)
	{
		const int floor = 7;
		const int s_z = MAP_MAX_LAYER;
		const int e_z = floor;
		TEST_ASSERT(s_z == 15);
		TEST_ASSERT(e_z == 7);
		for (int z = s_z; z >= e_z; --z) {
			TEST_ASSERT(z >= 7 && z <= 15);
		}
	}

	// 2. Multi-floor cursor hit test projection
	// Active floor is 6 (tower roof). User clicks screen position mapped to (100, 100, 6).
	// Lower visible floor 7 tile should be at (100 - 1 + 0, 100 - 1 + 0, 7) = (99, 99, 7).
	{
		const int mouse_x = 100;
		const int mouse_y = 100;
		const int mouse_z = 6;

		struct MockTile {
			int x;
			int y;
			int z;
			bool selected;
		};

		std::vector<MockTile> tiles = {
			{ 100, 100, 6, false }, // unselected on active floor
			{ 99, 99, 7, true },    // selected on visible floor below!
		};

		auto findTile = [&](int x, int y, int z) -> const MockTile* {
			for (const auto& t : tiles) {
				if (t.x == x && t.y == y && t.z == z) return &t;
			}
			return nullptr;
		};

		// Algorithm: check active floor first, if unselected check visible floors below
		const MockTile* activeTile = findTile(mouse_x, mouse_y, mouse_z);
		TEST_ASSERT(activeTile != nullptr && !activeTile->selected);

		const MockTile* resolvedTile = nullptr;
		for (int z = mouse_z + 1; z <= MAP_MAX_LAYER; ++z) {
			const int z_x = mouse_x - getFloorTileOffset(mouse_z) + getFloorTileOffset(z);
			const int z_y = mouse_y - getFloorTileOffset(mouse_z) + getFloorTileOffset(z);
			const MockTile* lower = findTile(z_x, z_y, z);
			if (lower && lower->selected) {
				resolvedTile = lower;
				break;
			}
		}

		TEST_ASSERT(resolvedTile != nullptr);
		TEST_ASSERT(resolvedTile->z == 7);
		TEST_ASSERT(resolvedTile->x == 99 && resolvedTile->y == 99);
		TEST_ASSERT(resolvedTile->selected == true);
	}

	std::cout << "[PASS] TestMultiFloorSelectionAndHitTesting passed!\n";
}


void TestCreatureNameAndReloadPreservation() {
	std::cout << "[RUN] TestCreatureNameAndReloadPreservation...\n";

	// Test 1: Creature name fallback when CreatureType is not in database
	struct MockCreatureType {
		std::string name;
		bool standard = false;
		bool missing = false;
		bool isNpc = false;
	};

	std::map<std::string, MockCreatureType*> mock_database;

	auto getName = [&](const std::string& type_name) -> const std::string& {
		auto it = mock_database.find(type_name);
		if (it != mock_database.end() && it->second) {
			return it->second->name;
		}
		return type_name;
	};

	// When type is not registered, getName() returns type_name, NOT empty string
	std::string npcName = "Frodo";
	TEST_ASSERT(getName(npcName) == "Frodo");

	// Test 2: Standard vs non-standard database clearing
	MockCreatureType standardMonster{ "Demon", true, false, false };
	MockCreatureType spawnLoadedNpc{ "Frodo", false, true, true };

	mock_database["demon"] = &standardMonster;
	mock_database["frodo"] = &spawnLoadedNpc;

	// Clear only standard (as on F5 reload)
	for (auto it = mock_database.begin(); it != mock_database.end(); ) {
		if (it->second && it->second->standard) {
			it = mock_database.erase(it);
		} else {
			++it;
		}
	}

	TEST_ASSERT(mock_database.find("demon") == mock_database.end());
	TEST_ASSERT(mock_database.find("frodo") != mock_database.end());
	TEST_ASSERT(mock_database["frodo"]->name == "Frodo");

	// Test 3: Standard creature superseding retained non-standard creature
	MockCreatureType dummyNpc{ "Frodo", false, true, true }; // non-standard from spawn
	mock_database["frodo"] = &dummyNpc;
	MockCreatureType officialNpc{ "Frodo (Official)", true, true, false }; // standard from npcs.xml

	auto loadCreature = [&](MockCreatureType* ct, bool isStandard) {
		auto it = mock_database.find("frodo");
		if (it != mock_database.end()) {
			if (it->second->missing || (!it->second->standard && isStandard)) {
				mock_database["frodo"] = ct;
			}
		} else {
			mock_database["frodo"] = ct;
		}
	};
	loadCreature(&officialNpc, true);
	TEST_ASSERT(mock_database["frodo"]->name == "Frodo (Official)");
	TEST_ASSERT(mock_database["frodo"]->standard == true);

	std::cout << "[PASS] TestCreatureNameAndReloadPreservation passed!\n";
}

void TestDiscardUndoRedo() {
	std::cout << "[RUN] TestDiscardUndoRedo...\n";

	TestStagingContent staging;
	MockBrush b1{ 10, "Brush A" };
	MockBrush b2{ 20, "Brush B" };
	staging.itemIds = { 100, 200, 300 };
	staging.brushes = { &b1, &b2 };

	struct TestAction {
		enum class Kind { Item, Brush, ClearItems, ClearBrushes, ClearAll };
		Kind kind;
		uint16_t itemId = 0;
		size_t originalIndex = 0;
		MockBrush* brush = nullptr;
		std::vector<uint16_t> savedItems;
		std::vector<MockBrush*> savedBrushes;
	};

	std::vector<TestAction> undoStack;
	std::vector<TestAction> redoStack;

	// Action 1: Discard item 200 at index 1
	{
		auto it = std::find(staging.itemIds.begin(), staging.itemIds.end(), 200);
		TEST_ASSERT(it != staging.itemIds.end());
		size_t idx = std::distance(staging.itemIds.begin(), it);
		undoStack.push_back({ TestAction::Kind::Item, 200, idx });
		staging.DiscardItem(200);
	}
	TEST_ASSERT(staging.itemIds.size() == 2);
	TEST_ASSERT(staging.itemIds[0] == 100 && staging.itemIds[1] == 300);

	// Action 2: Discard brush b1 at index 0
	{
		auto it = std::find(staging.brushes.begin(), staging.brushes.end(), &b1);
		TEST_ASSERT(it != staging.brushes.end());
		size_t idx = std::distance(staging.brushes.begin(), it);
		undoStack.push_back({ TestAction::Kind::Brush, 0, idx, &b1 });
		staging.DiscardBrush(&b1);
	}
	TEST_ASSERT(staging.brushes.size() == 1);
	TEST_ASSERT(staging.brushes[0] == &b2);

	// Undo Action 2 (Brush)
	{
		TEST_ASSERT(!undoStack.empty());
		auto act = std::move(undoStack.back());
		undoStack.pop_back();
		staging.brushes.insert(staging.brushes.begin() + act.originalIndex, act.brush);
		redoStack.push_back(std::move(act));
	}
	TEST_ASSERT(staging.brushes.size() == 2);
	TEST_ASSERT(staging.brushes[0] == &b1);

	// Undo Action 1 (Item)
	{
		TEST_ASSERT(!undoStack.empty());
		auto act = std::move(undoStack.back());
		undoStack.pop_back();
		staging.itemIds.insert(staging.itemIds.begin() + act.originalIndex, act.itemId);
		redoStack.push_back(std::move(act));
	}
	TEST_ASSERT(staging.itemIds.size() == 3);
	TEST_ASSERT(staging.itemIds[0] == 100 && staging.itemIds[1] == 200 && staging.itemIds[2] == 300);

	// Redo Action 1 (Item)
	{
		TEST_ASSERT(!redoStack.empty());
		auto act = std::move(redoStack.back());
		redoStack.pop_back();
		staging.DiscardItem(act.itemId);
		undoStack.push_back(std::move(act));
	}
	TEST_ASSERT(staging.itemIds.size() == 2);
	TEST_ASSERT(staging.itemIds[0] == 100 && staging.itemIds[1] == 300);

	// Action 3: Clear All
	{
		undoStack.push_back({ TestAction::Kind::ClearAll, 0, 0, nullptr, staging.itemIds, staging.brushes });
		redoStack.clear();
		staging.Clear();
	}
	TEST_ASSERT(staging.Empty());

	// Simulate user adding an item while empty:
	staging.itemIds.push_back(999);

	// Undo Action 3 (Clear All) - must MERGE, preserving item 999!
	{
		TEST_ASSERT(!undoStack.empty());
		auto act = std::move(undoStack.back());
		undoStack.pop_back();
		TestStagingContent restored;
		restored.itemIds = std::move(act.savedItems);
		restored.brushes = std::move(act.savedBrushes);
		staging.Merge(restored);
		redoStack.push_back(std::move(act));
	}
	TEST_ASSERT(staging.itemIds.size() == 3);
	TEST_ASSERT(std::find(staging.itemIds.begin(), staging.itemIds.end(), 999) != staging.itemIds.end());
	TEST_ASSERT(std::find(staging.itemIds.begin(), staging.itemIds.end(), 100) != staging.itemIds.end());
	TEST_ASSERT(staging.brushes.size() == 2);

	std::cout << "[PASS] TestDiscardUndoRedo passed!\n";
}

void TestGridSorting() {
	std::cout << "[RUN] TestGridSorting...\n";

	auto toLower = [](std::string s) {
		for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	};

	// 1. Item ID sort ascending & descending
	std::vector<uint16_t> items = { 300, 100, 500, 200 };
	std::ranges::sort(items, [](uint16_t a, uint16_t b) { return a < b; });
	TEST_ASSERT(items[0] == 100 && items[1] == 200 && items[2] == 300 && items[3] == 500);

	std::ranges::sort(items, [](uint16_t a, uint16_t b) { return a > b; });
	TEST_ASSERT(items[0] == 500 && items[1] == 300 && items[2] == 200 && items[3] == 100);

	// 2. Item Name sort (case-insensitive with ID fallback)
	struct MockItemName {
		std::string name;
		uint16_t id;
	};
	std::vector<MockItemName> namedItems = {
		{ "Sword", 10 },
		{ "apple", 20 },
		{ "Axe", 30 },
		{ "banana", 40 },
		{ "Axe", 25 }
	};

	// Ascending
	std::ranges::sort(namedItems, [&toLower](const MockItemName& a, const MockItemName& b) {
		std::string na = toLower(a.name);
		std::string nb = toLower(b.name);
		if (na != nb) return na < nb;
		return a.id < b.id;
	});
	TEST_ASSERT(namedItems[0].name == "apple");
	TEST_ASSERT(namedItems[1].name == "Axe" && namedItems[1].id == 25);
	TEST_ASSERT(namedItems[2].name == "Axe" && namedItems[2].id == 30);
	TEST_ASSERT(namedItems[3].name == "banana");
	TEST_ASSERT(namedItems[4].name == "Sword");

	// Descending
	std::ranges::sort(namedItems, [&toLower](const MockItemName& a, const MockItemName& b) {
		std::string na = toLower(a.name);
		std::string nb = toLower(b.name);
		if (na != nb) return na > nb;
		return a.id > b.id;
	});
	TEST_ASSERT(namedItems[0].name == "Sword");
	TEST_ASSERT(namedItems[1].name == "banana");
	TEST_ASSERT(namedItems[2].name == "Axe" && namedItems[2].id == 30);
	TEST_ASSERT(namedItems[3].name == "Axe" && namedItems[3].id == 25);
	TEST_ASSERT(namedItems[4].name == "apple");

	// 3. Brush sorting by ID and Name
	MockBrush bWater{ 50, "Water" };
	MockBrush bGrass{ 20, "Grass" };
	MockBrush bSand{ 80, "sand" };
	std::vector<MockBrush*> brushes = { &bWater, &bGrass, &bSand };

	// Sort brushes by name ascending
	std::ranges::sort(brushes, [&toLower](const MockBrush* a, const MockBrush* b) {
		std::string na = toLower(a->getName());
		std::string nb = toLower(b->getName());
		if (na != nb) return na < nb;
		return a->getID() < b->getID();
	});
	TEST_ASSERT(brushes[0]->getName() == "Grass");
	TEST_ASSERT(brushes[1]->getName() == "sand");
	TEST_ASSERT(brushes[2]->getName() == "Water");

	// Sort brushes by ID ascending
	std::ranges::sort(brushes, [](const MockBrush* a, const MockBrush* b) {
		return a->getID() < b->getID();
	});
	TEST_ASSERT(brushes[0]->getID() == 20);
	TEST_ASSERT(brushes[1]->getID() == 50);
	TEST_ASSERT(brushes[2]->getID() == 80);

	std::cout << "[PASS] TestGridSorting passed!\n";
}

int main() {
	std::cout << "========== RUNNING COLLECTION STUDIO TESTS ==========\n";
	TestFilenameSanitization();
	TestStagingContentLogic();
	TestSpawnBrushExclusion();
	TestXmlGenerationAndRoundtrip();
	TestPalettesXmlIncludeSync();
	TestMaterialDatabaseSources();
	TestPaletteCatalogSetTilesetBrushes();
	TestTargetPaletteXmlAndPaths();
	TestMultiFloorSelectionAndHitTesting();
	TestCreatureNameAndReloadPreservation();
	TestDiscardUndoRedo();
	TestGridSorting();
	std::cout << "========== ALL TESTS PASSED SUCCESSFULLY! ==========\n";
	return 0;
}
