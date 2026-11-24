#include "palette/quick_tilesets/brush_collection_menu_model.h"
#include "palette/quick_tilesets/brush_collection_policy.h"
#include "palette/quick_tilesets/quick_tileset_drag_payload.h"
#include "palette/quick_tilesets/quick_tileset_json.h"
#include "palette/quick_tilesets/quick_tileset_model.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace QuickTilesets;

namespace {

	Item Raw(int id) {
		return { .key = "r:" + std::to_string(id), .palette = 6 };
	}

	Item Named(const std::string& name) {
		return { .key = "b:" + name, .palette = 0 };
	}

	std::vector<std::string> Keys(const std::vector<Item>& items) {
		std::vector<std::string> keys;
		for (const Item& item : items) {
			keys.push_back(item.key);
		}
		return keys;
	}

	Tab ManyItemsTab() {
		Tab tab { .id = "port", .name = "Port", .items = {} };
		std::vector<Item> many;
		for (int i = 1; i <= 5000; ++i) {
			many.push_back(Raw(i));
			many.push_back(Named("brush-" + std::to_string(i)));
		}
		Insert(tab, many);
		return tab;
	}

} // namespace

TEST_CASE("Recent keeps 20 brushes and 60 RAW items, newest first", "[quick_tilesets][model]") {
	State state;
	for (int i = 1; i <= 90; ++i) {
		Remember(state, Raw(i));
		Remember(state, Named("brush-" + std::to_string(i)));
	}
	REQUIRE(state.recent.size() == 80);
	REQUIRE(std::ranges::count_if(state.recent, [](const Item& item) { return item.isRaw(); }) == 60);
	REQUIRE(std::ranges::count_if(state.recent, [](const Item& item) { return !item.isRaw(); }) == 20);
	REQUIRE(state.recent.front().key == "b:brush-90");

	REQUIRE(Remember(state, Raw(40)));
	REQUIRE(state.recent.front().key == "r:40");
	REQUIRE(state.recent.size() == 80);
}

TEST_CASE("Remember is a no-op for the front entry", "[quick_tilesets][model]") {
	State state;
	REQUIRE(Remember(state, Named("grass")));
	REQUIRE_FALSE(Remember(state, Named("grass")));
	REQUIRE(Remember(state, Named("sand")));
	REQUIRE(Remember(state, Named("grass")));
	REQUIRE(Keys(state.recent) == std::vector<std::string> { "b:grass", "b:sand" });
}

TEST_CASE("Insert copies idempotently and keeps existing positions", "[quick_tilesets][model]") {
	Tab tab = ManyItemsTab();
	REQUIRE(tab.items.size() == 10000);
	Insert(tab, std::vector<Item> { Raw(1), Raw(99999) });
	REQUIRE(tab.items.size() == 10001);
	REQUIRE(tab.items.front().key == "r:1");
	REQUIRE(tab.items.back().key == "r:99999");
}

TEST_CASE("Insert with reorder moves entries before the target", "[quick_tilesets][model]") {
	Tab tab = ManyItemsTab();
	Insert(tab, std::vector<Item> { Raw(4) }, "r:1", true);
	REQUIRE(tab.items.front().key == "r:4");
	REQUIRE(tab.items.size() == 10000);

	const auto before = tab.items;
	Insert(tab, std::vector<Item> { Raw(4) }, "r:4", true);
	REQUIRE(tab.items == before);

	Insert(tab, std::vector<Item> { Raw(5), Raw(6), Raw(5) }, "r:1", true);
	REQUIRE(tab.items[1].key == "r:5");
	REQUIRE(tab.items[2].key == "r:6");
	REQUIRE(tab.items.size() == 10000);
}

TEST_CASE("RemoveFromTab drops the given keys only", "[quick_tilesets][model]") {
	Tab tab { .id = "a", .name = "A", .items = { Raw(1), Named("grass"), Raw(2) } };
	REQUIRE(RemoveFromTab(tab, std::vector<Item> { Raw(1), Raw(3) }));
	REQUIRE(Keys(tab.items) == std::vector<std::string> { "b:grass", "r:2" });
	REQUIRE_FALSE(RemoveFromTab(tab, std::vector<Item> { Raw(3) }));
}

TEST_CASE("favorites add only missing entries and keep positions", "[quick_tilesets][model]") {
	State state;
	state.favorites = { Raw(1), Named("grass") };
	const std::vector<Item> mixed { Named("sand"), Raw(1) };
	REQUIRE_FALSE(AllFavorite(state, mixed));
	REQUIRE(ToggleFavorites(state, mixed));
	REQUIRE(Keys(state.favorites) == std::vector<std::string> { "r:1", "b:grass", "b:sand" });

	REQUIRE(AllFavorite(state, mixed));
	REQUIRE(ToggleFavorites(state, mixed));
	REQUIRE(Keys(state.favorites) == std::vector<std::string> { "b:grass" });
	REQUIRE_FALSE(AllFavorite(state, std::vector<Item> {}));
}

TEST_CASE("tab names are trimmed and must not be empty", "[quick_tilesets][model]") {
	REQUIRE(NormalizeTabName("  Port  ") == std::optional<std::string>("Port"));
	REQUIRE_FALSE(NormalizeTabName(" \t ").has_value());

	State state;
	REQUIRE(CreateTab(state, "a", " Port "));
	REQUIRE(state.tabs.front().name == "Port");
	REQUIRE(state.active == "a");
	REQUIRE_FALSE(CreateTab(state, "a", "Again"));
	REQUIRE_FALSE(CreateTab(state, "b", "   "));
	REQUIRE_FALSE(CreateTab(state, "", "Name"));

	REQUIRE(RenameTab(state, "a", " Harbour "));
	REQUIRE(state.tabs.front().name == "Harbour");
	REQUIRE_FALSE(RenameTab(state, "a", "Harbour"));
	REQUIRE_FALSE(RenameTab(state, "a", " "));
	REQUIRE_FALSE(RenameTab(state, "missing", "X"));
}

TEST_CASE("deleting the active tab falls back to Recent", "[quick_tilesets][model]") {
	State state;
	CreateTab(state, "a", "A");
	CreateTab(state, "b", "B");
	REQUIRE(ActiveTab(state) == FindTab(state, "b"));
	REQUIRE(DeleteTab(state, "b"));
	REQUIRE(state.active.empty());
	REQUIRE(ActiveTab(state) == nullptr);
	REQUIRE_FALSE(DeleteTab(state, "b"));

	state.active = "stale";
	REQUIRE(ActiveTab(state) == nullptr);
}

TEST_CASE("tabs move within bounds", "[quick_tilesets][model]") {
	State state;
	CreateTab(state, "a", "A");
	CreateTab(state, "b", "B");
	CreateTab(state, "c", "C");
	REQUIRE_FALSE(MoveTab(state, "a", -1));
	REQUIRE_FALSE(MoveTab(state, "c", 1));
	REQUIRE_FALSE(MoveTab(state, "missing", 1));
	REQUIRE(MoveTab(state, "a", 1));
	REQUIRE(MoveTab(state, "c", -1));
	std::vector<std::string> ids;
	for (const Tab& tab : state.tabs) {
		ids.push_back(tab.id);
	}
	REQUIRE(ids == std::vector<std::string> { "b", "c", "a" });
}

TEST_CASE("RAW keys parse to positive 32-bit server ids", "[quick_tilesets][model]") {
	REQUIRE(ParseRawKey("r:1") == std::optional<uint32_t>(1));
	REQUIRE(ParseRawKey("r:70000") == std::optional<uint32_t>(70000));
	REQUIRE(ParseRawKey("r:4294967295") == std::optional<uint32_t>(4294967295u));
	REQUIRE(RawKey(4526) == "r:4526");
	REQUIRE_FALSE(ParseRawKey("r:0").has_value());
	REQUIRE_FALSE(ParseRawKey("r:4294967296").has_value());
	REQUIRE_FALSE(ParseRawKey("r:-1").has_value());
	REQUIRE_FALSE(ParseRawKey("r:12a").has_value());
	REQUIRE_FALSE(ParseRawKey("r:").has_value());
	REQUIRE_FALSE(ParseRawKey("b:12").has_value());
}

TEST_CASE("encode and decode round trip", "[quick_tilesets][model][json]") {
	State state;
	for (int i = 1; i <= 30; ++i) {
		Remember(state, Raw(i));
	}
	state.tabs.push_back(ManyItemsTab());
	state.active = "port";
	state.split = 0.35;
	state.favorites = { Raw(100), Named("ground"), { .key = "c:rat", .palette = 4 } };

	const State restored = Decode(Encode(state));
	REQUIRE(restored.tabs == state.tabs);
	REQUIRE(restored.recent == state.recent);
	REQUIRE(restored.favorites == state.favorites);
	REQUIRE(restored.active == state.active);
	REQUIRE(restored.split == state.split);
}

TEST_CASE("decode clamps the split and accepts files without favorites", "[quick_tilesets][model][json]") {
	nlohmann::json encoded = Encode(State {});
	encoded["split"] = 0.99;
	encoded.erase("favorites");
	const State state = Decode(encoded);
	REQUIRE(state.split == 0.85);
	REQUIRE(state.favorites.empty());
}

TEST_CASE("decode collapses duplicates and keeps unknown references", "[quick_tilesets][model][json]") {
	State state;
	state.favorites = { Raw(1), Named("temporarily unavailable brush") };
	nlohmann::json encoded = Encode(state);
	encoded["favorites"].push_back(encoded["favorites"][0]);
	const State restored = Decode(encoded);
	REQUIRE(restored.favorites == state.favorites);
}

TEST_CASE("decode rejects invalid data", "[quick_tilesets][model][json]") {
	State state;
	state.tabs.push_back({ .id = "a", .name = "A", .items = {} });
	nlohmann::json duplicate = Encode(state);
	duplicate["tabs"].push_back(duplicate["tabs"][0]);
	REQUIRE_THROWS(Decode(duplicate));

	REQUIRE_THROWS(nlohmann::json { { "key", "bad" }, { "palette", 6 } }.get<Item>());
	REQUIRE_THROWS(nlohmann::json { { "key", "r:1" }, { "palette", 9 } }.get<Item>());
	REQUIRE_THROWS(nlohmann::json { { "id", "" }, { "name", "A" }, { "items", nlohmann::json::array() } }.get<Tab>());
}

TEST_CASE("each client version keeps its own section", "[quick_tilesets][model][json]") {
	nlohmann::json document = EmptyDocument();
	REQUIRE_NOTHROW(ValidateDocument(document));

	State modern;
	modern.tabs.push_back(ManyItemsTab());
	MergeVersionSection(document, "1310", modern);
	MergeVersionSection(document, "860", State {});

	REQUIRE(DecodeVersion(document, "860").tabs.empty());
	REQUIRE(DecodeVersion(document, "1310").tabs.front().items.size() == 10000);
	REQUIRE(DecodeVersion(document, "772").tabs.empty());

	State old;
	Remember(old, Raw(7));
	MergeVersionSection(document, "860", old);
	REQUIRE(DecodeVersion(document, "1310").tabs.front().items.size() == 10000);
	REQUIRE(DecodeVersion(document, "860").recent.front().key == "r:7");
}

TEST_CASE("documents of another schema are rejected", "[quick_tilesets][model][json]") {
	REQUIRE_THROWS(ValidateDocument(nlohmann::json::array()));
	REQUIRE_THROWS(ValidateDocument(nlohmann::json { { "schema", 2 }, { "versions", nlohmann::json::object() } }));
	REQUIRE_THROWS(ValidateDocument(nlohmann::json { { "schema", 1 }, { "versions", nlohmann::json::array() } }));
}

TEST_CASE("drag payloads round trip and reject foreign data", "[quick_tilesets][drag]") {
	const DragPayload payload { .version = "1310", .tab = "port", .items = { Raw(5), Named("ground") } };
	const auto parsed = ParseDragPayload(EncodeDragPayload(payload));
	REQUIRE(parsed);
	REQUIRE(parsed->version == payload.version);
	REQUIRE(parsed->tab == payload.tab);
	REQUIRE(parsed->items == payload.items);

	REQUIRE_FALSE(ParseDragPayload("not json"));
	REQUIRE_FALSE(ParseDragPayload(R"({"tab":"port","items":[]})"));
	REQUIRE_FALSE(ParseDragPayload(R"({"version":"1310","items":[{"key":"bad"}]})"));
}

TEST_CASE("only standalone brushes are collected", "[quick_tilesets][policy]") {
	REQUIRE(IsCollectable(BrushKind::Named));
	REQUIRE(IsCollectable(BrushKind::Raw));
	REQUIRE(IsCollectable(BrushKind::Creature));
	REQUIRE_FALSE(IsCollectable(BrushKind::PaletteContext));
	REQUIRE_FALSE(IsCollectable(BrushKind::Tool));
}

TEST_CASE("brush menu offers favorites, sets and removal", "[quick_tilesets][menu]") {
	using namespace QuickTilesets::BrushMenu;
	REQUIRE(Build(Facts {}).empty());

	const auto plain = Build(Facts { .selection_count = 2, .all_favorite = false, .tab_names = {}, .inside_user_tab = false });
	REQUIRE(plain.size() == 2);
	REQUIRE(plain[0].command == Command::AddFavorites);
	REQUIRE(plain[1].command == Command::NewTab);
	REQUIRE(plain[1].in_tab_submenu);
	REQUIRE_FALSE(plain[1].separator_before);

	const auto inside = Build(Facts { .selection_count = 1, .all_favorite = true, .tab_names = { "Port", "Cave" }, .inside_user_tab = true });
	REQUIRE(inside.size() == 5);
	REQUIRE(inside[0].command == Command::RemoveFavorites);
	REQUIRE(inside[1].command == Command::AddToTab);
	REQUIRE(inside[1].label == "Port");
	REQUIRE(inside[2].tab_index == 1);
	REQUIRE(inside[3].command == Command::NewTab);
	REQUIRE(inside[3].separator_before);
	REQUIRE(inside[4].command == Command::RemoveFromTab);
	REQUIRE_FALSE(inside[4].in_tab_submenu);
}
