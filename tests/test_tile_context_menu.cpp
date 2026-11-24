#include "ui/tile_context_menu_model.h"
#include "map/tile_stack_rows.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using TileContextMenu::Build;
using TileContextMenu::Command;
using TileContextMenu::Entry;
using TileContextMenu::Facts;
using TileContextMenu::SelectedItemFacts;

namespace {

	const Entry* Find(const std::vector<Entry>& entries, Command command) {
		const auto it = std::ranges::find(entries, command, &Entry::command);
		return it == entries.end() ? nullptr : &*it;
	}

	bool Has(const std::vector<Entry>& entries, Command command) {
		return Find(entries, command) != nullptr;
	}

	std::size_t Count(const std::vector<Entry>& entries, Command command) {
		return static_cast<std::size_t>(std::ranges::count(entries, command, &Entry::command));
	}

	// One tile selected, ground with a palette-visible ground brush, nothing else.
	Facts GroundTile() {
		Facts facts;
		facts.selected_tile_count = 1;
		facts.has_ground = true;
		facts.has_visible_ground_brush = true;
		return facts;
	}

}

TEST_CASE("without a selection only clipboard entries are shown", "[tile_context_menu]") {
	Facts facts;
	facts.can_paste = true;
	const auto entries = Build(facts);

	REQUIRE(entries.size() == 5);
	REQUIRE_FALSE(Find(entries, Command::Cut)->enabled);
	REQUIRE_FALSE(Find(entries, Command::Copy)->enabled);
	REQUIRE_FALSE(Find(entries, Command::CopyPosition)->enabled);
	REQUIRE_FALSE(Find(entries, Command::Delete)->enabled);
	REQUIRE(Find(entries, Command::Paste)->enabled);
	REQUIRE_FALSE(Has(entries, Command::SelectRaw));
	REQUIRE_FALSE(Has(entries, Command::BrowseField));
}

TEST_CASE("paste follows the copy buffer state", "[tile_context_menu]") {
	Facts facts = GroundTile();
	REQUIRE_FALSE(Find(Build(facts), Command::Paste)->enabled);
	facts.can_paste = true;
	REQUIRE(Find(Build(facts), Command::Paste)->enabled);
}

TEST_CASE("several selected tiles get no per-tile entries", "[tile_context_menu]") {
	Facts facts = GroundTile();
	facts.selected_tile_count = 3;
	facts.selected_item = SelectedItemFacts {};
	const auto entries = Build(facts);

	REQUIRE(Find(entries, Command::Cut)->enabled);
	REQUIRE(Has(entries, Command::AdvancedReplace));
	REQUIRE(Has(entries, Command::AddToCollection));
	REQUIRE_FALSE(Has(entries, Command::CopyServerId));
	REQUIRE_FALSE(Has(entries, Command::SelectRaw));
	REQUIRE_FALSE(Has(entries, Command::Properties));
}

TEST_CASE("selected ground row exposes ground entries only", "[tile_context_menu]") {
	Facts facts = GroundTile();
	facts.selected_item = SelectedItemFacts {};
	const auto entries = Build(facts);

	REQUIRE(Find(entries, Command::Delete)->enabled);
	REQUIRE(Has(entries, Command::CopyServerId));
	REQUIRE(Has(entries, Command::CopyClientId));
	REQUIRE(Has(entries, Command::CopyName));
	REQUIRE(Has(entries, Command::SelectGround));
	REQUIRE(Has(entries, Command::SelectRaw));
	REQUIRE(Has(entries, Command::Properties));
	REQUIRE(Has(entries, Command::BrowseField));
	REQUIRE(Has(entries, Command::TileProperties));

	REQUIRE_FALSE(Has(entries, Command::SelectDoodad));
	REQUIRE_FALSE(Has(entries, Command::SelectDoor));
	REQUIRE_FALSE(Has(entries, Command::SelectWall));
	REQUIRE_FALSE(Has(entries, Command::Rotate));
	REQUIRE_FALSE(Has(entries, Command::OpenDoor));
	REQUIRE_FALSE(Has(entries, Command::CloseDoor));
	REQUIRE_FALSE(Has(entries, Command::SelectCollection));
}

TEST_CASE("item entries need exactly one selected item", "[tile_context_menu]") {
	Facts facts = GroundTile();
	const auto entries = Build(facts);

	REQUIRE_FALSE(Has(entries, Command::CopyServerId));
	REQUIRE_FALSE(Has(entries, Command::SelectDoodad));
	REQUIRE(Has(entries, Command::SelectRaw));
	REQUIRE(Has(entries, Command::SelectGround));
	REQUIRE(Has(entries, Command::Properties));
}

TEST_CASE("selected doodad row offers its doodad brush", "[tile_context_menu]") {
	Facts facts = GroundTile();
	facts.has_selected_stack_item = true;
	facts.selected_item = SelectedItemFacts { .has_visible_doodad_brush = true };
	const auto entries = Build(facts);

	REQUIRE(Has(entries, Command::SelectDoodad));
	REQUIRE(Has(entries, Command::SelectRaw));
	REQUIRE(Has(entries, Command::CopyServerId));
	REQUIRE_FALSE(Has(entries, Command::SelectDoor));
}

TEST_CASE("selected brush door offers door switch and door brush", "[tile_context_menu]") {
	Facts facts = GroundTile();
	facts.has_selected_stack_item = true;
	facts.selected_item = SelectedItemFacts { .is_door = true, .is_open_door = true, .is_brush_door = true, .has_door_brush = true };
	auto entries = Build(facts);

	REQUIRE(Has(entries, Command::CloseDoor));
	REQUIRE_FALSE(Has(entries, Command::OpenDoor));
	REQUIRE(Has(entries, Command::SelectDoor));

	facts.selected_item->is_open_door = false;
	entries = Build(facts);
	REQUIRE(Has(entries, Command::OpenDoor));
	REQUIRE_FALSE(Has(entries, Command::CloseDoor));
}

TEST_CASE("rotate and teleport entries follow the selected item", "[tile_context_menu]") {
	Facts facts = GroundTile();
	facts.has_selected_stack_item = true;
	facts.selected_item = SelectedItemFacts { .is_rotatable = true };
	auto entries = Build(facts);
	REQUIRE(Has(entries, Command::Rotate));
	REQUIRE_FALSE(Has(entries, Command::GotoDestination));

	facts.selected_item = SelectedItemFacts { .is_teleport = true };
	entries = Build(facts);
	REQUIRE_FALSE(Has(entries, Command::GotoDestination));

	facts.selected_item->teleport_has_destination = true;
	entries = Build(facts);
	REQUIRE(Has(entries, Command::GotoDestination));
	REQUIRE_FALSE(Has(entries, Command::Rotate));
}

TEST_CASE("a door that is no brush door, rotatable or teleport gets no door switch", "[tile_context_menu]") {
	Facts facts = GroundTile();
	facts.has_selected_stack_item = true;
	facts.selected_item = SelectedItemFacts { .is_door = true };
	const auto entries = Build(facts);
	REQUIRE_FALSE(Has(entries, Command::OpenDoor));
	REQUIRE_FALSE(Has(entries, Command::CloseDoor));
}

TEST_CASE("tile-level brushes come from the whole stack", "[tile_context_menu]") {
	Facts facts = GroundTile();
	facts.has_wall_brush = true;
	facts.has_carpet_brush = true;
	facts.has_table_brush = true;
	facts.has_creature = true;
	facts.has_spawn = true;
	facts.is_house_tile = true;
	facts.stack_has_collection = true;
	const auto entries = Build(facts);

	REQUIRE(Has(entries, Command::SelectWall));
	REQUIRE(Has(entries, Command::SelectCarpet));
	REQUIRE(Has(entries, Command::SelectTable));
	REQUIRE(Has(entries, Command::SelectCreature));
	REQUIRE(Has(entries, Command::SelectSpawn));
	REQUIRE(Has(entries, Command::SelectHouse));
	REQUIRE(Count(entries, Command::SelectCollection) == 1);
}

TEST_CASE("collection entry from the selected item or the ground brush", "[tile_context_menu]") {
	Facts facts = GroundTile();
	facts.selected_item = SelectedItemFacts { .has_collection_brush = true };
	REQUIRE(Has(Build(facts), Command::SelectCollection));

	facts.selected_item.reset();
	facts.ground_brush_has_collection = true;
	REQUIRE(Has(Build(facts), Command::SelectCollection));
}

TEST_CASE("tile without ground or selected item keeps tile-level entries", "[tile_context_menu]") {
	Facts facts;
	facts.selected_tile_count = 1;
	facts.has_spawn = true;
	facts.has_carpet_brush = true;
	const auto entries = Build(facts);

	REQUIRE(Has(entries, Command::SelectSpawn));
	REQUIRE(Has(entries, Command::SelectRaw));
	REQUIRE(Has(entries, Command::Properties));
	REQUIRE_FALSE(Has(entries, Command::SelectCarpet));
	REQUIRE_FALSE(Has(entries, Command::SelectGround));

	facts.has_spawn = false;
	REQUIRE_FALSE(Has(Build(facts), Command::Properties));
}

TEST_CASE("Lua entries appear only when registered", "[tile_context_menu]") {
	Facts facts = GroundTile();
	REQUIRE_FALSE(Has(Build(facts), Command::Script));

	facts.script_count = 3;
	const auto entries = Build(facts);
	REQUIRE(Count(entries, Command::Script) == 3);

	std::vector<std::size_t> indices;
	for (const Entry& entry : entries) {
		if (entry.command == Command::Script) {
			indices.push_back(entry.script_index);
		}
	}
	REQUIRE(indices == std::vector<std::size_t> { 0, 1, 2 });
	// Scripts close the menu after their own separator.
	REQUIRE(entries[entries.size() - 4].command == Command::Separator);
}

TEST_CASE("context menu row is the clicked row or the selected row from the keyboard", "[tile_context_menu]") {
	using TileStackRows::ContextMenuRow;

	// ground + 2 items: rows 0 (ground), 1, 2
	REQUIRE(ContextMenuRow(false, 2, 0, true, 2) == 2);
	REQUIRE(ContextMenuRow(false, 0, 2, true, 2) == 0);
	// empty space below the rows is ignored
	REQUIRE_FALSE(ContextMenuRow(false, -1, 1, true, 2));
	REQUIRE_FALSE(ContextMenuRow(false, 3, 1, true, 2));

	// keyboard menu acts on the selected row, and on nothing without one
	REQUIRE(ContextMenuRow(true, -1, 1, true, 2) == 1);
	REQUIRE_FALSE(ContextMenuRow(true, -1, -1, true, 2));

	// the resolved row maps to the exact stacked item, not the top of the stack
	const auto row = TileStackRows::Resolve(*ContextMenuRow(false, 1, 2, true, 2), true, 2);
	REQUIRE(row);
	REQUIRE_FALSE(row->is_ground);
	REQUIRE(row->item_index == 0);
}
