#include "app/application.h"
#include "app/definitions.h"
#include "app/main.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/ground/auto_border.h"
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
// from the test link so its main() does not conflict with the test entry point.
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

	// GroundBrush keeps its border tables protected; the wand tests register border pieces the
	// same way the materials.xml loader does, through the optional border.
	class TestGroundBrush : public GroundBrush {
	public:
		void ownBorderId(uint32_t id) {
			owned_optional_border = std::make_unique<AutoBorder>(1);
			owned_optional_border->tiles.fill(0);
			owned_optional_border->tiles[0] = id;
			optional_border = owned_optional_border.get();
		}
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

	bool EntrySelectsGround(const MagicWand::Plan& plan, int x, int y) {
		for (const MagicWand::Entry& entry : plan.entries) {
			if (entry.position.x == x && entry.position.y == y) {
				return entry.select_ground;
			}
		}
		return false;
	}

	// The production path of one wand click: resolve the hit, then build the plan for it.
	std::optional<MagicWand::Plan> PlanFromHit(
		World& world,
		const MagicWand::WandHit& hit,
		MagicWand::SearchMode mode,
		const PositionRect& viewport
	) {
		const auto resolved = MagicWand::ResolveTarget(hit, world.Lookup());
		if (!resolved) {
			return std::nullopt;
		}
		return MagicWand::BuildPlan(resolved->target, resolved->names, mode, world.Lookup(), resolved->origin, viewport, {});
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

TEST_CASE("search mode sanitize maps stored settings onto modes", "[magic_wand][plan]") {
	REQUIRE(MagicWand::SanitizeSearchMode(0) == MagicWand::SearchMode::Viewport);
	REQUIRE(MagicWand::SanitizeSearchMode(static_cast<int>(MagicWand::SearchMode::Neighbour)) == MagicWand::SearchMode::Neighbour);
	REQUIRE(MagicWand::SanitizeSearchMode(99) == MagicWand::SearchMode::Viewport);
	REQUIRE(MagicWand::SanitizeSearchMode(-1) == MagicWand::SearchMode::Viewport);
}

TEST_CASE("item family groups same-name variants and nameless items together", "[magic_wand][plan]") {
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
	// Nameless items share the empty name, so they form one family instead of per-id islands.
	REQUIRE(MagicWand::ItemFamilyKey(nameless_a) == MagicWand::ItemFamilyKey(nameless_b));
	REQUIRE(MagicWand::ItemFamilyKey(nameless_a) != MagicWand::ItemFamilyKey(stone_a));
}

TEST_CASE("identity names match case-insensitively and treat the empty name as a name", "[magic_wand][plan]") {
	const MagicWand::TargetNames names { "Stone Wall", "lawn" };
	REQUIRE(MagicWand::NameMatches(names, "stone wall"));
	REQUIRE(MagicWand::NameMatches(names, "LAWN"));
	REQUIRE(MagicWand::NameMatches(names, "Stone Wall"));
	REQUIRE_FALSE(MagicWand::NameMatches(names, "wheat"));
	REQUIRE_FALSE(MagicWand::NameMatches(names, ""));

	// An empty identity name matches the empty definition name and nothing else.
	const MagicWand::TargetNames empty { "", "" };
	REQUIRE(MagicWand::NameMatches(empty, ""));
	REQUIRE_FALSE(MagicWand::NameMatches(empty, "Stone Wall"));
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

	const auto ground_resolved = MagicWand::ResolveTarget(world.HitAt(0, 0, MagicWand::HitKind::Ground, &*world.At(0, 0).ground), world.Lookup());
	REQUIRE(ground_resolved.has_value());
	REQUIRE(std::holds_alternative<GroundBrush*>(ground_resolved->target));
	REQUIRE(std::get<GroundBrush*>(ground_resolved->target) == &ground);
	// The brushes are nameless, so identity falls back to the clicked item's definition name.
	REQUIRE(ground_resolved->names.primary == "grass");
	REQUIRE(ground_resolved->names.item == "grass");

	const auto wall_resolved = MagicWand::ResolveTarget(world.HitAt(0, 1), world.Lookup());
	REQUIRE(wall_resolved.has_value());
	REQUIRE(std::holds_alternative<WallBrush*>(wall_resolved->target));
	REQUIRE(wall_resolved->names.primary == "wall");

	const auto doodad_resolved = MagicWand::ResolveTarget(world.HitAt(0, 2), world.Lookup());
	REQUIRE(doodad_resolved.has_value());
	REQUIRE(std::holds_alternative<DoodadBrush*>(doodad_resolved->target));

	const auto carpet_resolved = MagicWand::ResolveTarget(world.HitAt(0, 3), world.Lookup());
	REQUIRE(carpet_resolved.has_value());
	REQUIRE(std::holds_alternative<CarpetBrush*>(carpet_resolved->target));

	const auto table_resolved = MagicWand::ResolveTarget(world.HitAt(0, 4), world.Lookup());
	REQUIRE(table_resolved.has_value());
	REQUIRE(std::holds_alternative<TableBrush*>(table_resolved->target));

	const auto wheat_resolved = MagicWand::ResolveTarget(world.HitAt(0, 5), world.Lookup());
	REQUIRE(std::holds_alternative<MagicWand::ItemFamily>(wheat_resolved->target));
	REQUIRE(std::get<MagicWand::ItemFamily>(wheat_resolved->target).key == "wheat");

	// A border without a ground that owns it falls back to the item family, never to the ground.
	const auto border_resolved = MagicWand::ResolveTarget(world.HitAt(0, 6), world.Lookup());
	REQUIRE(std::holds_alternative<MagicWand::ItemFamily>(border_resolved->target));
	REQUIRE(std::get<MagicWand::ItemFamily>(border_resolved->target).key == "border");
}

TEST_CASE("target resolution accepts a doodad stored in the ground slot", "[magic_wand][plan]") {
	World world;
	DoodadBrush doodad;
	world.Define(300, "bush", ITEM_GROUP_GROUND, 0, nullptr, &doodad);
	world.Put(0, 0, 300);

	REQUIRE(world.At(0, 0).ground != nullptr);
	const auto resolved = MagicWand::ResolveTarget(world.HitAt(0, 0, MagicWand::HitKind::Ground, world.At(0, 0).ground.get()), world.Lookup());
	REQUIRE(resolved.has_value());
	REQUIRE(std::holds_alternative<DoodadBrush*>(resolved->target));
	REQUIRE(std::get<DoodadBrush*>(resolved->target) == &doodad);
}

TEST_CASE("a border click resolves to the neighbour ground that produced it", "[magic_wand][plan]") {
	World world;
	TestGroundBrush grass;
	grass.ownBorderId(1601);
	world.Define(1600, "grass", ITEM_GROUP_GROUND, 0, &grass);
	world.Define(1601, "grass border", ITEM_GROUP_NONE, flagMask(ItemFlag::IsBorder));

	world.Put(0, 0, 1600);
	world.Put(1, 0, 1600);
	// The border piece sits on a ring tile without any ground, like a real map.
	world.Put(0, 1, 1601);

	Tile& border_tile = world.At(0, 1);
	const auto resolved = MagicWand::ResolveTarget(world.HitAt(0, 1, MagicWand::HitKind::Item, border_tile.items[0].get()), world.Lookup());
	REQUIRE(resolved.has_value());
	REQUIRE(std::holds_alternative<GroundBrush*>(resolved->target));
	REQUIRE(std::get<GroundBrush*>(resolved->target) == &grass);
	// The collection starts on the owning ground, not on the border tile.
	REQUIRE(resolved->origin == Position(0, 0, kFloor));

	// Neighbour mode: the patch floods from the owner; the border tile is not a ground, so it
	// joins through the ring instead of chaining the flood.
	const auto plan = MagicWand::BuildPlan(resolved->target, resolved->names, MagicWand::SearchMode::Neighbour, world.Lookup(), resolved->origin, Rect(0, 0, 9, 9), {});
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 2);
	REQUIRE(HasEntry(*plan, 0, 1));
	REQUIRE_FALSE(EntrySelectsGround(*plan, 0, 1));

	TileOperations::deselect(&border_tile);
	MagicWand::SelectMatching(border_tile, resolved->target, resolved->names, false);
	REQUIRE(border_tile.items[0]->isSelected());
}

TEST_CASE("wand mode drives scattered or connected collection", "[magic_wand][plan]") {
	World world;
	WallBrush wall;
	world.Define(1400, "wall", ITEM_GROUP_NONE, flagMask(ItemFlag::IsWall), &wall);

	world.Put(0, 0, 1400);
	world.Put(1, 0, 1400);
	world.Put(5, 0, 1400);

	const auto viewport_plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Viewport, Rect(0, 0, 9, 9));
	REQUIRE(viewport_plan.has_value());
	REQUIRE(viewport_plan->target_tiles == 3);
	REQUIRE(HasEntry(*viewport_plan, 0, 0));
	REQUIRE(HasEntry(*viewport_plan, 1, 0));
	REQUIRE(HasEntry(*viewport_plan, 5, 0));

	const auto neighbour_plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Neighbour, Rect(0, 0, 9, 9));
	REQUIRE(neighbour_plan.has_value());
	REQUIRE(neighbour_plan->target_tiles == 2);
	REQUIRE(HasEntry(*neighbour_plan, 0, 0));
	REQUIRE(HasEntry(*neighbour_plan, 1, 0));
	REQUIRE_FALSE(HasEntry(*neighbour_plan, 5, 0));
}

TEST_CASE("item family neighbour plan stops at a gap", "[magic_wand][plan]") {
	World world;
	world.Define(600, "cobble", ITEM_GROUP_GROUND);
	world.Define(601, "cobble", ITEM_GROUP_GROUND);

	world.Put(0, 0, 600);
	world.Put(1, 0, 601);
	world.Put(5, 0, 601);

	const auto viewport_plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Viewport, Rect(0, 0, 9, 9));
	REQUIRE(viewport_plan.has_value());
	REQUIRE(viewport_plan->target_tiles == 3);

	const auto neighbour_plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Neighbour, Rect(0, 0, 9, 9));
	REQUIRE(neighbour_plan.has_value());
	REQUIRE(neighbour_plan->target_tiles == 2);
	REQUIRE(HasEntry(*neighbour_plan, 1, 0));
	REQUIRE_FALSE(HasEntry(*neighbour_plan, 5, 0));
	for (const MagicWand::Entry& entry : neighbour_plan->entries) {
		REQUIRE(entry.select_ground);
	}
}

TEST_CASE("creature neighbour plan only takes creatures touching the click", "[magic_wand][plan]") {
	World world;
	world.Define(750, "rat");
	world.At(0, 0).creature = std::make_unique<Creature>("rat");
	world.At(1, 0).creature = std::make_unique<Creature>("rat");
	// Diagonal touching counts as neighbour in Neighbour mode.
	world.At(2, 1).creature = std::make_unique<Creature>("rat");
	world.At(5, 0).creature = std::make_unique<Creature>("rat");
	// An item that shares the creature name is not a creature and stays out of the plan.
	world.Put(6, 0, 750);

	const auto neighbour_plan = PlanFromHit(world, world.HitAt(0, 0, MagicWand::HitKind::Creature, nullptr, world.At(0, 0).creature.get()), MagicWand::SearchMode::Neighbour, Rect(0, 0, 9, 9));
	REQUIRE(neighbour_plan.has_value());
	REQUIRE(neighbour_plan->target_tiles == 3);
	REQUIRE(HasEntry(*neighbour_plan, 2, 1));
	REQUIRE_FALSE(HasEntry(*neighbour_plan, 5, 0));

	const auto viewport_plan = PlanFromHit(world, world.HitAt(0, 0, MagicWand::HitKind::Creature, nullptr, world.At(0, 0).creature.get()), MagicWand::SearchMode::Viewport, Rect(0, 0, 9, 9));
	REQUIRE(viewport_plan.has_value());
	REQUIRE(viewport_plan->target_tiles == 4);
	REQUIRE(HasEntry(*viewport_plan, 5, 0));
	REQUIRE_FALSE(HasEntry(*viewport_plan, 6, 0));
}

TEST_CASE("ground neighbour plan fills a checkerboard through the diagonals", "[magic_wand][plan]") {
	World world;
	GroundBrush grass;
	GroundBrush sand;
	world.Define(1700, "grass", ITEM_GROUP_GROUND, 0, &grass);
	world.Define(1701, "sand", ITEM_GROUP_GROUND, 0, &sand);

	// Same-ground tiles only ever touch diagonally, like the reported checkerboard map.
	for (int y = 0; y < 4; ++y) {
		for (int x = 0; x < 4; ++x) {
			world.Put(x, y, (x + y) % 2 == 0 ? 1700 : 1701);
		}
	}

	const auto plan = PlanFromHit(world, world.HitAt(0, 0, MagicWand::HitKind::Ground, world.At(0, 0).ground.get()), MagicWand::SearchMode::Neighbour, Rect(0, 0, 3, 3));
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 8);
	for (const MagicWand::Entry& entry : plan->entries) {
		if ((entry.position.x + entry.position.y) % 2 == 0) {
			REQUIRE(entry.select_ground);
		}
	}
}

TEST_CASE("ground neighbour plan never chains through same-name items or borders", "[magic_wand][plan]") {
	World world;
	GroundBrush grass;
	world.Define(1600, "grass", ITEM_GROUP_GROUND, 0, &grass);
	// The separator carries the same definition name as the ground; on real maps these are the
	// border pieces and decorations that used to bridge two separated patches.
	world.Define(1601, "grass", ITEM_GROUP_NONE, flagMask(ItemFlag::IsBorder));

	world.Put(0, 0, 1600);
	world.Put(1, 0, 1600);
	// Separator without any ground, holding the "grass"-named item.
	world.Put(2, 0, 1601);
	// A separate grass patch beyond the separator.
	world.Put(3, 0, 1600);
	world.Put(4, 0, 1600);

	const auto resolved = MagicWand::ResolveTarget(world.HitAt(0, 0, MagicWand::HitKind::Ground, world.At(0, 0).ground.get()), world.Lookup());
	REQUIRE(resolved.has_value());
	const auto plan = MagicWand::BuildPlan(resolved->target, resolved->names, MagicWand::SearchMode::Neighbour, world.Lookup(), resolved->origin, Rect(0, 0, 9, 9), {});
	REQUIRE(plan.has_value());
	// Only the clicked patch's grounds are targets; the flood cannot cross the separator.
	REQUIRE(plan->target_tiles == 2);
	REQUIRE(HasEntry(*plan, 0, 0));
	REQUIRE(HasEntry(*plan, 1, 0));
	REQUIRE_FALSE(HasEntry(*plan, 3, 0));
	REQUIRE_FALSE(HasEntry(*plan, 4, 0));
	// The separator is a ring tile, so its same-name item still gets selected with the patch.
	REQUIRE(HasEntry(*plan, 2, 0));
	REQUIRE_FALSE(EntrySelectsGround(*plan, 2, 0));

	Tile& separator = world.At(2, 0);
	TileOperations::deselect(&separator);
	MagicWand::SelectMatching(separator, resolved->target, resolved->names, false);
	REQUIRE(separator.items[0]->isSelected());
}

TEST_CASE("nameless items share the empty name as one family", "[magic_wand][plan]") {
	World world;
	world.Define(1800, "");
	world.Define(1801, "");
	world.Define(1802, "wheat");

	world.Put(0, 0, 1800);
	world.Put(1, 0, 1801);
	world.Put(5, 0, 1801);
	world.Put(1, 1, 1802);

	const auto neighbour_plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Neighbour, Rect(0, 0, 9, 9));
	REQUIRE(neighbour_plan.has_value());
	REQUIRE(neighbour_plan->target_tiles == 2);
	REQUIRE(HasEntry(*neighbour_plan, 0, 0));
	REQUIRE(HasEntry(*neighbour_plan, 1, 0));
	REQUIRE_FALSE(HasEntry(*neighbour_plan, 5, 0));
	REQUIRE_FALSE(HasEntry(*neighbour_plan, 1, 1));

	const auto viewport_plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Viewport, Rect(0, 0, 9, 9));
	REQUIRE(viewport_plan.has_value());
	REQUIRE(viewport_plan->target_tiles == 3);
	REQUIRE(HasEntry(*viewport_plan, 5, 0));
	REQUIRE_FALSE(HasEntry(*viewport_plan, 1, 1));

	const auto resolved = MagicWand::ResolveTarget(world.HitAt(0, 0), world.Lookup());
	REQUIRE(resolved.has_value());
	REQUIRE(std::holds_alternative<MagicWand::ItemFamily>(resolved->target));
	Tile& sibling = world.At(1, 0);
	TileOperations::deselect(&sibling);
	MagicWand::SelectMatching(sibling, resolved->target, resolved->names, false);
	REQUIRE(sibling.items[0]->isSelected());
}

TEST_CASE("ground neighbour plan collects the patch and its ring inside the viewport", "[magic_wand][plan]") {
	World world;
	GroundBrush ground;
	GroundBrush other;
	world.Define(400, "grass", ITEM_GROUP_GROUND, 0, &ground);
	world.Define(401, "sand", ITEM_GROUP_GROUND, 0, &other);

	world.Put(0, 0, 400);
	world.Put(1, 0, 400);
	world.Put(0, 1, 400);
	world.Put(1, 1, 401);

	const auto plan = PlanFromHit(world, world.HitAt(0, 0, MagicWand::HitKind::Ground, world.At(0, 0).ground.get()), MagicWand::SearchMode::Neighbour, Rect(0, 0, 1, 1));
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 3);
	REQUIRE(plan->entries.size() == 4);
	REQUIRE(HasEntry(*plan, 1, 1));

	for (const MagicWand::Entry& entry : plan->entries) {
		const bool patch = entry.position.x + entry.position.y <= 1;
		REQUIRE(entry.select_ground == patch);
	}
}

TEST_CASE("ground neighbour plan is clamped by the viewport", "[magic_wand][plan]") {
	World world;
	GroundBrush ground;
	world.Define(410, "grass", ITEM_GROUP_GROUND, 0, &ground);
	for (int x = 0; x < 6; ++x) {
		world.Put(x, 0, 410);
	}

	const auto plan = PlanFromHit(world, world.HitAt(0, 0, MagicWand::HitKind::Ground, world.At(0, 0).ground.get()), MagicWand::SearchMode::Neighbour, Rect(0, 0, 2, 0));
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 3);
	REQUIRE_FALSE(HasEntry(*plan, 3, 0));
}

TEST_CASE("ground viewport plan covers the whole visible patch and its border pieces", "[magic_wand][plan]") {
	World world;
	TestGroundBrush ground;
	ground.ownBorderId(1601);
	world.Define(1600, "grass", ITEM_GROUP_GROUND, 0, &ground);
	world.Define(1601, "grass border", ITEM_GROUP_NONE, flagMask(ItemFlag::IsBorder));
	world.Define(1602, "sand", ITEM_GROUP_GROUND);

	world.Put(0, 0, 1600);
	world.Put(1, 0, 1600);
	world.Put(5, 0, 1600);
	// Border pieces of the grass brush, on ring tiles that hold no grass.
	world.Put(0, 1, 1601);
	world.Put(2, 2, 1601);
	// A foreign ground and a foreign border stay out.
	world.Put(6, 0, 1602);
	world.Define(1603, "sand border", ITEM_GROUP_NONE, flagMask(ItemFlag::IsBorder));
	world.Put(6, 1, 1603);

	const auto plan = PlanFromHit(world, world.HitAt(0, 0, MagicWand::HitKind::Ground, world.At(0, 0).ground.get()), MagicWand::SearchMode::Viewport, Rect(0, 0, 9, 9));
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 5);
	REQUIRE(HasEntry(*plan, 0, 0));
	REQUIRE(HasEntry(*plan, 1, 0));
	REQUIRE(HasEntry(*plan, 5, 0));
	REQUIRE(HasEntry(*plan, 0, 1));
	REQUIRE(HasEntry(*plan, 2, 2));
	REQUIRE_FALSE(HasEntry(*plan, 6, 0));
	REQUIRE_FALSE(HasEntry(*plan, 6, 1));

	REQUIRE(EntrySelectsGround(*plan, 0, 0));
	REQUIRE(EntrySelectsGround(*plan, 5, 0));
	REQUIRE_FALSE(EntrySelectsGround(*plan, 0, 1));
	REQUIRE_FALSE(EntrySelectsGround(*plan, 2, 2));
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

	const auto plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Viewport, Rect(0, 0, 9, 9));
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 4);
	REQUIRE(HasEntry(*plan, 0, 0));
	REQUIRE(HasEntry(*plan, 1, 0));
	REQUIRE(HasEntry(*plan, 2, 0));
	REQUIRE(HasEntry(*plan, 5, 0));
	REQUIRE_FALSE(HasEntry(*plan, 1, 1));
}

TEST_CASE("doodad viewport plan covers items and ground-slot pieces", "[magic_wand][plan]") {
	World world;
	DoodadBrush doodad;
	world.Define(700, "bush", ITEM_GROUP_NONE, 0, nullptr, &doodad);
	world.Define(701, "bush ground", ITEM_GROUP_GROUND, 0, nullptr, &doodad);
	world.Define(702, "bush", ITEM_GROUP_NONE, 0, nullptr, &doodad);

	world.Put(0, 0, 700);
	world.Put(1, 0, 701);
	world.Put(10, 0, 702);

	const auto plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Viewport, Rect(0, 0, 2, 0));
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 2);
	REQUIRE_FALSE(HasEntry(*plan, 10, 0));

	for (const MagicWand::Entry& entry : plan->entries) {
		REQUIRE(entry.select_ground == (entry.position.x == 1));
	}
}

TEST_CASE("doodad neighbour plan floods through adjacent pieces", "[magic_wand][plan]") {
	World world;
	DoodadBrush doodad;
	world.Define(700, "bush", ITEM_GROUP_NONE, 0, nullptr, &doodad);

	world.Put(0, 0, 700);
	world.Put(1, 0, 700);
	world.Put(5, 0, 700);

	const auto plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Neighbour, Rect(0, 0, 9, 9));
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 2);
	REQUIRE(HasEntry(*plan, 1, 0));
	REQUIRE_FALSE(HasEntry(*plan, 5, 0));
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

	const auto plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Neighbour, Rect(0, 0, 9, 9));
	REQUIRE(plan.has_value());
	REQUIRE(plan->target_tiles == 3);
	REQUIRE(HasEntry(*plan, 1, 0));
	REQUIRE(HasEntry(*plan, 2, 0));

	Tile& doodad_tile = world.At(1, 0);
	TileOperations::deselect(&doodad_tile);
	MagicWand::SelectMatching(doodad_tile, &wall, MagicWand::TargetNames { "Stone Wall", "stone wall" }, false);
	REQUIRE(doodad_tile.items[0]->isSelected());
}

TEST_CASE("the clicked item's name joins grounds of other brushes", "[magic_wand][plan]") {
	World world;
	// The brush name and the item definition name differ: "Grass Field" vs "lawn".
	GroundBrush heavy;
	heavy.setName("Grass Field");
	GroundBrush light;
	world.Define(1300, "lawn", ITEM_GROUP_GROUND, 0, &heavy);
	world.Define(1301, "Lawn", ITEM_GROUP_GROUND, 0, &light);

	world.Put(0, 0, 1300);
	world.Put(1, 0, 1301);
	world.Put(5, 0, 1301);

	const auto resolved = MagicWand::ResolveTarget(world.HitAt(0, 0, MagicWand::HitKind::Ground, world.At(0, 0).ground.get()), world.Lookup());
	REQUIRE(resolved.has_value());
	REQUIRE(std::get<GroundBrush*>(resolved->target) == &heavy);
	REQUIRE(resolved->names.primary == "Grass Field");
	REQUIRE(resolved->names.item == "lawn");

	// Neighbour mode: the adjacent "Lawn" ground of another brush joins by the item name.
	const auto neighbour_plan = MagicWand::BuildPlan(resolved->target, resolved->names, MagicWand::SearchMode::Neighbour, world.Lookup(), resolved->origin, Rect(0, 0, 9, 9), {});
	REQUIRE(neighbour_plan.has_value());
	REQUIRE(neighbour_plan->target_tiles == 2);
	REQUIRE(HasEntry(*neighbour_plan, 1, 0));
	REQUIRE_FALSE(HasEntry(*neighbour_plan, 5, 0));

	// Viewport mode: every "lawn" ground in sight joins, adjacent or not.
	const auto viewport_plan = MagicWand::BuildPlan(resolved->target, resolved->names, MagicWand::SearchMode::Viewport, world.Lookup(), resolved->origin, Rect(0, 0, 9, 9), {});
	REQUIRE(viewport_plan.has_value());
	REQUIRE(viewport_plan->target_tiles == 3);
	REQUIRE(HasEntry(*viewport_plan, 5, 0));

	Tile& other_brush_tile = world.At(1, 0);
	TileOperations::deselect(&other_brush_tile);
	MagicWand::SelectMatching(other_brush_tile, resolved->target, resolved->names, true);
	REQUIRE(other_brush_tile.ground->isSelected());
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

	const auto wall_plan = PlanFromHit(world, world.HitAt(0, 0), MagicWand::SearchMode::Neighbour, Rect(0, 0, 9, 9));
	REQUIRE(wall_plan.has_value());
	REQUIRE(wall_plan->entries.size() == 2);

	const auto carpet_plan = PlanFromHit(world, world.HitAt(0, 1), MagicWand::SearchMode::Neighbour, Rect(0, 0, 9, 9));
	REQUIRE(carpet_plan.has_value());
	REQUIRE(carpet_plan->entries.size() == 2);

	const auto table_plan = PlanFromHit(world, world.HitAt(0, 2), MagicWand::SearchMode::Neighbour, Rect(0, 0, 9, 9));
	REQUIRE(table_plan.has_value());
	REQUIRE(table_plan->entries.size() == 2);

	Tile& carpet_tile = world.At(1, 1);
	TileOperations::deselect(&carpet_tile);
	MagicWand::SelectMatching(carpet_tile, &carpet, MagicWand::TargetNames { "carpet", "carpet" }, false);
	REQUIRE(carpet_tile.items[0]->isSelected());

	Tile& table_tile = world.At(1, 2);
	TileOperations::deselect(&table_tile);
	MagicWand::SelectMatching(table_tile, &table, MagicWand::TargetNames { "table", "table" }, false);
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
	REQUIRE_FALSE(
		MagicWand::BuildPlan(&ground, MagicWand::TargetNames { "grass", "grass" }, MagicWand::SearchMode::Neighbour, world.Lookup(), Position(0, 0, kFloor), Rect(0, 0, 9, 9), cancel_after_first).has_value()
	);

	world.At(5, 0).creature = std::make_unique<Creature>("rat");
	const auto cancel_immediately = [](std::string_view, size_t, size_t) {
		return false;
	};
	// The viewport scan reports on its first tile.
	REQUIRE_FALSE(
		MagicWand::BuildPlan(MagicWand::CreatureTarget { "rat" }, MagicWand::TargetNames { "rat", "" }, MagicWand::SearchMode::Viewport, world.Lookup(), Position(5, 0, kFloor), Rect(0, 0, 9, 9), cancel_immediately).has_value()
	);
	// The neighbour flood reports on its first found tile.
	REQUIRE_FALSE(
		MagicWand::BuildPlan(MagicWand::CreatureTarget { "rat" }, MagicWand::TargetNames { "rat", "" }, MagicWand::SearchMode::Neighbour, world.Lookup(), Position(5, 0, kFloor), Rect(0, 0, 9, 9), cancel_immediately).has_value()
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
	MagicWand::SelectMatching(tile, MagicWand::ItemFamily { "Stone Tile" }, MagicWand::TargetNames { "Stone Tile", "Stone Tile" }, false);
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

	MagicWand::SelectMatching(tile, &ground, MagicWand::TargetNames { "grass", "grass" }, true);
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
