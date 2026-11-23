#include "app/application.h"
#include "app/definitions.h"
#include "app/main.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/wall/wall_brush.h"
#include "editor/operations/magic_wand_plan.h"
#include "game/creature.h"
#include "game/item.h"
#include "game/items.h"
#include "item_definitions/core/item_definition_fragments.h"
#include "item_definitions/core/item_definition_store.h"
#include "item_definitions/core/item_definition_types.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "ui/replace_tool/map_item_collector.h"
#include "ui/replace_tool/replacement_engine.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// These two symbols live in application.cpp, which is intentionally excluded
// from the test link so its main() does not conflict with Catch2's.
Application& wxGetApp() {
	static Application* application = nullptr;
	return *application;
}

void Application::Unload() {
}

namespace {

	constexpr int kFloor = 7;

	// Minimal map for the wand plan: items are created from injected definitions, so the plan can
	// be exercised without loading a client version.
	class World {
	public:
		World() {
			g_item_definitions.clear();
		}
		~World() {
			tiles_.clear();
			g_item_definitions.clear();
		}

		void Define(
			ServerItemId id,
			std::string name,
			ItemGroup_t group = ITEM_GROUP_NONE,
			uint64_t flags = 0,
			Brush* brush = nullptr,
			Brush* doodad_brush = nullptr,
			RAWBrush* raw_brush = nullptr
		) {
			ResolvedItemDefinitionRow row;
			row.server_id = id;
			row.client_id = id;
			row.group = group;
			row.type = ITEM_TYPE_NONE;
			row.flags = flags;
			row.name = std::move(name);
			g_item_definitions.append(std::move(row));

			if (brush || doodad_brush || raw_brush) {
				ItemEditorData editor_data;
				editor_data.brush = brush;
				editor_data.doodad_brush = doodad_brush;
				editor_data.raw_brush = raw_brush;
				g_item_definitions.setEditorData(id, editor_data);
			}
		}

		Tile& At(int x, int y) {
			const Position position(x, y, kFloor);
			auto it = tiles_.find(position);
			if (it == tiles_.end()) {
				it = tiles_.emplace(position, std::make_unique<Tile>(x, y, kFloor)).first;
			}
			return *it->second;
		}

		void Put(int x, int y, ServerItemId id) {
			At(x, y).addItem(std::make_unique<Item>(id, 0));
		}

		MagicWand::TileLookup Lookup() {
			return [this](const Position& position) -> const Tile* {
				const auto it = tiles_.find(position);
				return it == tiles_.end() ? nullptr : it->second.get();
			};
		}

		MagicWand::WandHit HitAt(
			int x,
			int y,
			MagicWand::HitKind kind = MagicWand::HitKind::None,
			Item* item = nullptr,
			Creature* creature = nullptr
		) {
			return MagicWand::WandHit { kind, &At(x, y), item, creature };
		}

	private:
		std::map<Position, std::unique_ptr<Tile>> tiles_;
	};

	PositionRect Rect(int min_x, int min_y, int max_x, int max_y) {
		return PositionRect { min_x, min_y, max_x, max_y };
	}

	bool HasEntry(const MagicWand::Plan& plan, int x, int y) {
		for (const MagicWand::Entry& entry : plan.entries) {
			if (entry.position.x == x && entry.position.y == y) {
				return true;
			}
		}
		return false;
	}

} // namespace

TEST_CASE("position rect clamps, expands and counts tiles", "[magic_wand][plan]") {
	const PositionRect rect = PositionRect::fromCorners(Position(10, 4, 7), Position(2, 8, 7));
	REQUIRE(rect.min_x == 2);
	REQUIRE(rect.min_y == 4);
	REQUIRE(rect.max_x == 10);
	REQUIRE(rect.max_y == 8);
	REQUIRE(rect.tileCount() == 9 * 5);
	REQUIRE(rect.contains(Position(2, 4, 7)));
	REQUIRE_FALSE(rect.contains(Position(11, 4, 7)));

	const PositionRect grown = rect.expanded(2);
	REQUIRE(grown.min_x == 0);
	REQUIRE(grown.min_y == 2);
	REQUIRE(grown.max_x == 12);
	REQUIRE(grown.max_y == 10);

	const PositionRect edge = PositionRect::fromCorners(Position(0, 0, 7), Position(1, 1, 7)).expanded(2);
	REQUIRE(edge.min_x == 0);
	REQUIRE(edge.min_y == 0);

	REQUIRE(PositionRect {}.empty());
}

TEST_CASE("item family groups same-name variants and keeps nameless items apart", "[magic_wand][plan]") {
	World world;
	world.Define(100, "Stone Tile");
	world.Define(101, "Stone Tile");
	world.Define(102, "Wheat");
	world.Define(103, "");
	world.Define(104, "");

	Item stone_a(100, 0);
	Item stone_b(101, 0);
	Item wheat(102, 0);
	Item nameless_a(103, 0);
	Item nameless_b(104, 0);

	REQUIRE(MagicWand::ItemFamilyKey(stone_a) == MagicWand::ItemFamilyKey(stone_b));
	REQUIRE(MagicWand::ItemFamilyKey(stone_a) != MagicWand::ItemFamilyKey(wheat));
	REQUIRE(MagicWand::ItemFamilyKey(nameless_a) != MagicWand::ItemFamilyKey(nameless_b));
}

TEST_CASE("target resolution follows the picked entity", "[magic_wand][plan]") {
	World world;
	GroundBrush ground;
	WallBrush wall;
	DoodadBrush doodad;
	CarpetBrush carpet;
	TableBrush table;

	world.Define(200, "grass", ITEM_GROUP_GROUND, 0, &ground);
	world.Define(201, "wall", ITEM_GROUP_NONE, flagMask(ItemFlag::IsWall), &wall);
	world.Define(202, "doodad", ITEM_GROUP_NONE, 0, nullptr, &doodad);
	world.Define(203, "carpet", ITEM_GROUP_NONE, flagMask(ItemFlag::IsCarpet), &carpet);
	world.Define(204, "table", ITEM_GROUP_NONE, flagMask(ItemFlag::IsTable), &table);
	world.Define(205, "wheat");
	world.Define(206, "border", ITEM_GROUP_NONE, flagMask(ItemFlag::IsBorder));

	world.Put(0, 0, 200);
	world.Put(0, 1, 201);
	world.Put(0, 2, 202);
	world.Put(0, 3, 203);
	world.Put(0, 4, 204);
	world.Put(0, 5, 205);
	world.Put(0, 6, 206);

	const auto ground_hit = world.HitAt(0, 0, MagicWand::HitKind::Ground, &*world.At(0, 0).ground);
	REQUIRE(std::holds_alternative<GroundBrush*>(*MagicWand::ResolveTarget(ground_hit)));
	REQUIRE(std::get<GroundBrush*>(*MagicWand::ResolveTarget(ground_hit)) == &ground);

	REQUIRE(std::holds_alternative<WallBrush*>(*MagicWand::ResolveTarget(world.HitAt(0, 1))));
	REQUIRE(std::holds_alternative<DoodadBrush*>(*MagicWand::ResolveTarget(world.HitAt(0, 2))));
	REQUIRE(std::holds_alternative<CarpetBrush*>(*MagicWand::ResolveTarget(world.HitAt(0, 3))));
	REQUIRE(std::holds_alternative<TableBrush*>(*MagicWand::ResolveTarget(world.HitAt(0, 4))));

	const auto wheat_target = MagicWand::ResolveTarget(world.HitAt(0, 5));
	REQUIRE(std::holds_alternative<MagicWand::ItemFamily>(*wheat_target));
	REQUIRE(std::get<MagicWand::ItemFamily>(*wheat_target).key == "wheat");

	// A border without a ground that owns it falls back to the item family, never to the ground.
	const auto border_target = MagicWand::ResolveTarget(world.HitAt(0, 6));
	REQUIRE(std::holds_alternative<MagicWand::ItemFamily>(*border_target));
}

TEST_CASE("target resolution accepts a doodad stored in the ground slot", "[magic_wand][plan]") {
	World world;
	DoodadBrush doodad;
	world.Define(300, "bush", ITEM_GROUP_GROUND, 0, nullptr, &doodad);
	world.Put(0, 0, 300);

	REQUIRE(world.At(0, 0).ground != nullptr);
	const auto target = MagicWand::ResolveTarget(world.HitAt(0, 0, MagicWand::HitKind::Ground, world.At(0, 0).ground.get()));
	REQUIRE(target.has_value());
	REQUIRE(std::holds_alternative<DoodadBrush*>(*target));
	REQUIRE(std::get<DoodadBrush*>(*target) == &doodad);
}

TEST_CASE("ground plan collects the patch and its ring inside the viewport", "[magic_wand][plan]") {
	World world;
	GroundBrush ground;
	GroundBrush other;
	world.Define(400, "grass", ITEM_GROUP_GROUND, 0, &ground);
	world.Define(401, "sand", ITEM_GROUP_GROUND, 0, &other);

	world.Put(0, 0, 400);
	world.Put(1, 0, 400);
	world.Put(0, 1, 400);
	world.Put(1, 1, 401);

	const PositionRect viewport = Rect(0, 0, 1, 1);
	const auto plan = MagicWand::BuildPlan(&ground, world.Lookup(), Position(0, 0, kFloor), viewport, {});
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 3);
	REQUIRE(plan->entries.size() == 4);
	REQUIRE(HasEntry(*plan, 1, 1));

	for (const MagicWand::Entry& entry : plan->entries) {
		const bool patch = entry.position.x + entry.position.y <= 1;
		REQUIRE(entry.select_ground == patch);
	}
}

TEST_CASE("ground plan is clamped by the viewport", "[magic_wand][plan]") {
	World world;
	GroundBrush ground;
	world.Define(410, "grass", ITEM_GROUP_GROUND, 0, &ground);
	for (int x = 0; x < 6; ++x) {
		world.Put(x, 0, 410);
	}

	const auto plan = MagicWand::BuildPlan(&ground, world.Lookup(), Position(0, 0, kFloor), Rect(0, 0, 2, 0), {});
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 3);
	REQUIRE_FALSE(HasEntry(*plan, 3, 0));
}

TEST_CASE("same-name item family scans the visible floor", "[magic_wand][plan]") {
	World world;
	world.Define(500, "Stone Tile");
	world.Define(501, "stone tile");
	world.Define(502, "STONE TILE");
	world.Define(503, "Wheat");

	world.Put(0, 0, 500);
	world.Put(1, 0, 501);
	world.Put(2, 0, 502);
	world.Put(5, 0, 502);
	world.Put(1, 1, 503);

	const auto plan = MagicWand::BuildPlan(
		MagicWand::ItemFamily { "Stone Tile" },
		world.Lookup(),
		Position(0, 0, kFloor),
		Rect(0, 0, 9, 9),
		{}
	);
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 4);
	REQUIRE(HasEntry(*plan, 0, 0));
	REQUIRE(HasEntry(*plan, 1, 0));
	REQUIRE(HasEntry(*plan, 2, 0));
	REQUIRE(HasEntry(*plan, 5, 0));
	REQUIRE_FALSE(HasEntry(*plan, 1, 1));
}

TEST_CASE("raw ground family floods over variants and selects the ground slot", "[magic_wand][plan]") {
	World world;
	world.Define(600, "cobble", ITEM_GROUP_GROUND);
	world.Define(601, "cobble", ITEM_GROUP_GROUND);

	world.Put(0, 0, 600);
	world.Put(1, 0, 601);

	const auto plan = MagicWand::BuildPlan(MagicWand::ItemFamily { "cobble" }, world.Lookup(), Position(0, 0, kFloor), Rect(0, 0, 9, 9), {});
	REQUIRE(plan.has_value());
	REQUIRE(plan->entries.size() == 2);
	for (const MagicWand::Entry& entry : plan->entries) {
		REQUIRE(entry.select_ground);
	}
}

TEST_CASE("doodad plan covers items and ground-slot pieces in the viewport", "[magic_wand][plan]") {
	World world;
	DoodadBrush doodad;
	world.Define(700, "bush", ITEM_GROUP_NONE, 0, nullptr, &doodad);
	world.Define(701, "bush ground", ITEM_GROUP_GROUND, 0, nullptr, &doodad);
	world.Define(702, "bush", ITEM_GROUP_NONE, 0, nullptr, &doodad);

	world.Put(0, 0, 700);
	world.Put(1, 0, 701);
	world.Put(10, 0, 702);

	const auto plan = MagicWand::BuildPlan(&doodad, world.Lookup(), Position(0, 0, kFloor), Rect(0, 0, 2, 0), {});
	REQUIRE(plan.has_value());
	REQUIRE(plan->entries.size() == 2);
	REQUIRE_FALSE(HasEntry(*plan, 10, 0));

	for (const MagicWand::Entry& entry : plan->entries) {
		REQUIRE(entry.select_ground == (entry.position.x == 1));
	}
}

TEST_CASE("same-name entities of other kinds join a connected selection", "[magic_wand][plan]") {
	World world;
	WallBrush wall;
	DoodadBrush doodad;
	wall.setName("Stone Wall");
	doodad.setName("Stone Wall");

	world.Define(1200, "stone wall", ITEM_GROUP_NONE, flagMask(ItemFlag::IsWall), &wall);
	world.Define(1201, "stone wall", ITEM_GROUP_NONE, 0, nullptr, &doodad);

	world.Put(0, 0, 1200);
	world.Put(1, 0, 1201);
	world.Put(2, 0, 1200);

	const auto plan = MagicWand::BuildPlan(&wall, world.Lookup(), Position(0, 0, kFloor), Rect(0, 0, 9, 9), {});
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 3);
	REQUIRE(HasEntry(*plan, 1, 0));
	REQUIRE(HasEntry(*plan, 2, 0));

	Tile& doodad_tile = world.At(1, 0);
	TileOperations::deselect(&doodad_tile);
	MagicWand::SelectMatching(doodad_tile, &wall, false, "Stone Wall");
	REQUIRE(doodad_tile.items[0]->isSelected());
}

TEST_CASE("ground brushes with the same name connect by name", "[magic_wand][plan]") {
	World world;
	GroundBrush heavy;
	GroundBrush light;
	heavy.setName("stone tile");
	light.setName("stone tile");

	world.Define(1300, "stone tile", ITEM_GROUP_GROUND, 0, &heavy);
	world.Define(1301, "stone tile", ITEM_GROUP_GROUND, 0, &light);

	world.Put(0, 0, 1300);
	world.Put(1, 0, 1301);

	const auto plan = MagicWand::BuildPlan(&heavy, world.Lookup(), Position(0, 0, kFloor), Rect(0, 0, 9, 9), {});
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 2);
	for (const MagicWand::Entry& entry : plan->entries) {
		REQUIRE(entry.select_ground);
	}

	Tile& other_brush_tile = world.At(1, 0);
	TileOperations::deselect(&other_brush_tile);
	MagicWand::SelectMatching(other_brush_tile, &heavy, false, "stone tile");
	REQUIRE(other_brush_tile.ground->isSelected());
}

TEST_CASE("creature plan matches the same creature name in the viewport", "[magic_wand][plan]") {
	World world;
	world.Define(750, "rat");
	world.At(0, 0).creature = std::make_unique<Creature>("rat");
	world.At(1, 0).creature = std::make_unique<Creature>("dragon");
	world.At(2, 0).creature = std::make_unique<Creature>("rat");
	// An item that shares the creature name is not a creature and stays out of the plan.
	world.Put(3, 0, 750);

	const auto plan = MagicWand::BuildPlan(MagicWand::CreatureTarget { "rat" }, world.Lookup(), Position(0, 0, kFloor), Rect(0, 0, 9, 9), {});
	REQUIRE(plan.has_value());
	REQUIRE(plan->entries.size() == 2);
	REQUIRE(HasEntry(*plan, 0, 0));
	REQUIRE(HasEntry(*plan, 2, 0));
	REQUIRE_FALSE(HasEntry(*plan, 3, 0));
}

TEST_CASE("wall, carpet and table plans collect their connected runs", "[magic_wand][plan]") {
	World world;
	WallBrush wall;
	CarpetBrush carpet;
	TableBrush table;

	world.Define(1400, "wall", ITEM_GROUP_NONE, flagMask(ItemFlag::IsWall), &wall);
	world.Define(1401, "carpet", ITEM_GROUP_NONE, flagMask(ItemFlag::IsCarpet), &carpet);
	world.Define(1402, "table", ITEM_GROUP_NONE, flagMask(ItemFlag::IsTable), &table);

	world.Put(0, 0, 1400);
	world.Put(1, 0, 1400);
	world.Put(0, 1, 1401);
	world.Put(1, 1, 1401);
	world.Put(0, 2, 1402);
	world.Put(1, 2, 1402);

	const auto wall_plan = MagicWand::BuildPlan(&wall, world.Lookup(), Position(0, 0, kFloor), Rect(0, 0, 9, 9), {});
	REQUIRE(wall_plan.has_value());
	REQUIRE(wall_plan->entries.size() == 2);

	const auto carpet_plan = MagicWand::BuildPlan(&carpet, world.Lookup(), Position(0, 1, kFloor), Rect(0, 0, 9, 9), {});
	REQUIRE(carpet_plan.has_value());
	REQUIRE(carpet_plan->entries.size() == 2);

	const auto table_plan = MagicWand::BuildPlan(&table, world.Lookup(), Position(0, 2, kFloor), Rect(0, 0, 9, 9), {});
	REQUIRE(table_plan.has_value());
	REQUIRE(table_plan->entries.size() == 2);

	Tile& carpet_tile = world.At(1, 1);
	TileOperations::deselect(&carpet_tile);
	MagicWand::SelectMatching(carpet_tile, &carpet, false, "");
	REQUIRE(carpet_tile.items[0]->isSelected());

	Tile& table_tile = world.At(1, 2);
	TileOperations::deselect(&table_tile);
	MagicWand::SelectMatching(table_tile, &table, false, "");
	REQUIRE(table_tile.items[0]->isSelected());
}

TEST_CASE("plan building reports cancellation from every phase", "[magic_wand][plan]") {
	World world;
	GroundBrush ground;
	world.Define(800, "grass", ITEM_GROUP_GROUND, 0, &ground);
	for (int x = 0; x < 4; ++x) {
		world.Put(x, 0, 800);
	}

	size_t calls = 0;
	const auto cancel_after_first = [&calls](std::string_view, size_t, size_t) {
		return ++calls < 2;
	};
	REQUIRE_FALSE(MagicWand::BuildPlan(&ground, world.Lookup(), Position(0, 0, kFloor), Rect(0, 0, 9, 9), cancel_after_first).has_value());

	calls = 0;
	world.At(5, 0).creature = std::make_unique<Creature>("rat");
	const auto cancel_immediately = [](std::string_view, size_t, size_t) {
		return false;
	};
	REQUIRE_FALSE(
		MagicWand::BuildPlan(
			MagicWand::CreatureTarget { "rat" },
			world.Lookup(),
			Position(0, 0, kFloor),
			Rect(0, 0, 9, 9),
			cancel_immediately
		).has_value()
	);
}

TEST_CASE("SelectMatching picks only the target entities", "[magic_wand][plan]") {
	World world;
	GroundBrush ground;
	world.Define(900, "grass", ITEM_GROUP_GROUND, 0, &ground);
	world.Define(901, "Stone Tile");
	world.Define(902, "Stone Tile");
	world.Define(903, "Wheat");

	Tile& tile = world.At(0, 0);
	tile.addItem(std::make_unique<Item>(900, 0));
	tile.addItem(std::make_unique<Item>(901, 0));
	tile.addItem(std::make_unique<Item>(902, 0));
	tile.addItem(std::make_unique<Item>(903, 0));

	TileOperations::deselect(&tile);
	MagicWand::SelectMatching(tile, MagicWand::ItemFamily { "Stone Tile" }, false, "Stone Tile");
	REQUIRE_FALSE(tile.ground->isSelected());
	REQUIRE(tile.items[0]->isSelected());
	REQUIRE(tile.items[1]->isSelected());
	REQUIRE_FALSE(tile.items[2]->isSelected());

	Tile other(0, 0, kFloor);
	other.addItem(std::make_unique<Item>(900, 0));
	other.addItem(std::make_unique<Item>(901, 0));
	other.addItem(std::make_unique<Item>(902, 0));
	other.addItem(std::make_unique<Item>(903, 0));
	TileOperations::deselect(&other);
	REQUIRE_FALSE(MagicWand::SameSelection(tile, other));

	MagicWand::SelectMatching(tile, &ground, true, "grass");
	REQUIRE(tile.ground->isSelected());
}

TEST_CASE("SameSelection compares every slot", "[magic_wand][plan]") {
	World world;
	world.Define(950, "grass", ITEM_GROUP_GROUND);
	world.Define(951, "wheat");

	Tile live(0, 0, kFloor);
	live.addItem(std::make_unique<Item>(950, 0));
	live.addItem(std::make_unique<Item>(951, 0));
	Tile copy(0, 0, kFloor);
	copy.addItem(std::make_unique<Item>(950, 0));
	copy.addItem(std::make_unique<Item>(951, 0));

	REQUIRE(MagicWand::SameSelection(live, copy));
	copy.items[0]->select();
	REQUIRE_FALSE(MagicWand::SameSelection(live, copy));
	live.items[0]->select();
	REQUIRE(MagicWand::SameSelection(live, copy));
}

TEST_CASE("replace selection scope follows entity selection", "[magic_wand][plan]") {
	World world;
	world.Define(1100, "tree");
	Item item(1100, 0);

	const ReplaceScopePolicy everywhere;
	REQUIRE(everywhere.Allows(item));

	const ReplaceScopePolicy selected_only { .selection_only = true };
	REQUIRE_FALSE(selected_only.Allows(item));

	item.select();
	REQUIRE(selected_only.Allows(item));
}

TEST_CASE("replace tool collects only selected entities", "[magic_wand][plan]") {
	World world;
	world.Define(1000, "grass", ITEM_GROUP_GROUND);
	world.Define(1001, "tree");
	world.Define(1002, "wheat");

	Tile tile(0, 0, kFloor);
	tile.addItem(std::make_unique<Item>(1000, 0));
	tile.addItem(std::make_unique<Item>(1001, 0));
	tile.addItem(std::make_unique<Item>(1002, 0));

	std::vector<Tile*> tiles { &tile };
	REQUIRE(rme::replace_tool::MapItemCollector::CollectSelectedItemIds(tiles).empty());

	tile.items[0]->select();
	const auto single = rme::replace_tool::MapItemCollector::CollectSelectedItemIds(tiles);
	REQUIRE(single == std::vector<uint16_t> { 1001 });

	tile.ground->select();
	const auto with_ground = rme::replace_tool::MapItemCollector::CollectSelectedItemIds(tiles);
	REQUIRE(with_ground == std::vector<uint16_t> { 1000, 1001 });

	TileOperations::select(&tile);
	const auto whole_tile = rme::replace_tool::MapItemCollector::CollectSelectedItemIds(tiles);
	REQUIRE(whole_tile == std::vector<uint16_t> { 1000, 1001, 1002 });
}
