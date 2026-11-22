#include "ui/item_context_menu_model.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using ItemContextMenu::Build;
using ItemContextMenu::Command;
using ItemContextMenu::Entry;
using ItemContextMenu::Facts;

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

	std::vector<Command> Commands(const std::vector<Entry>& entries) {
		std::vector<Command> commands;
		for (const Entry& entry : entries) {
			commands.push_back(entry.command);
		}
		return commands;
	}

	// A plain stacked item that has a RAW brush (most items do).
	Facts PlainItem() {
		Facts facts;
		facts.item.has_raw_brush = true;
		return facts;
	}

} // namespace

TEST_CASE("a plain item offers only row operations", "[item_context_menu]") {
	const auto entries = Build(PlainItem());

	REQUIRE(Has(entries, Command::Copy));
	REQUIRE(Has(entries, Command::Delete));
	REQUIRE(Has(entries, Command::CopyServerId));
	REQUIRE(Has(entries, Command::CopyClientId));
	REQUIRE(Has(entries, Command::CopyName));
	REQUIRE(Has(entries, Command::SelectRaw));

	REQUIRE_FALSE(Has(entries, Command::Rotate));
	REQUIRE_FALSE(Has(entries, Command::GotoDestination));
	REQUIRE_FALSE(Has(entries, Command::OpenDoor));
	REQUIRE_FALSE(Has(entries, Command::CloseDoor));
	REQUIRE_FALSE(Has(entries, Command::SelectDoodad));
	REQUIRE_FALSE(Has(entries, Command::SelectDoor));
	REQUIRE_FALSE(Has(entries, Command::SelectWall));
	REQUIRE_FALSE(Has(entries, Command::SelectCarpet));
	REQUIRE_FALSE(Has(entries, Command::SelectTable));
	REQUIRE_FALSE(Has(entries, Command::SelectGround));
	REQUIRE_FALSE(Has(entries, Command::SelectCollection));

	REQUIRE(Count(entries, Command::Delete) == 1);
}

TEST_CASE("a row without a RAW brush hides Select RAW", "[item_context_menu]") {
	const auto entries = Build(Facts {});

	REQUIRE_FALSE(Has(entries, Command::SelectRaw));
	REQUIRE_FALSE(Has(entries, Command::SelectWall));
	REQUIRE(entries.back().command == Command::CopyName);
	REQUIRE(Count(entries, Command::Separator) == 1);
}

TEST_CASE("the item menu starts with the row clipboard block", "[item_context_menu]") {
	const auto entries = Build(PlainItem());
	REQUIRE(Commands(entries) == std::vector<Command> {
									  Command::Copy,
									  Command::Delete,
									  Command::Separator,
									  Command::CopyServerId,
									  Command::CopyClientId,
									  Command::CopyName,
									  Command::Separator,
									  Command::SelectRaw,
								  });
}

TEST_CASE("rotate and teleport entries follow the item", "[item_context_menu]") {
	Facts facts;
	facts.item.is_rotatable = true;
	auto entries = Build(facts);
	REQUIRE(Has(entries, Command::Rotate));
	REQUIRE_FALSE(Has(entries, Command::GotoDestination));

	facts = Facts {};
	facts.item.is_teleport = true;
	entries = Build(facts);
	REQUIRE_FALSE(Has(entries, Command::GotoDestination));

	facts.item.teleport_has_destination = true;
	entries = Build(facts);
	REQUIRE(Has(entries, Command::GotoDestination));
	REQUIRE_FALSE(Has(entries, Command::Rotate));
}

TEST_CASE("a brush door offers its switch and its brush", "[item_context_menu]") {
	Facts facts;
	facts.item.is_door = true;
	facts.item.is_open_door = true;
	facts.item.is_brush_door = true;
	facts.item.has_door_brush = true;
	auto entries = Build(facts);

	REQUIRE(Has(entries, Command::CloseDoor));
	REQUIRE_FALSE(Has(entries, Command::OpenDoor));
	REQUIRE(Has(entries, Command::SelectDoor));

	facts.item.is_open_door = false;
	entries = Build(facts);
	REQUIRE(Has(entries, Command::OpenDoor));
	REQUIRE_FALSE(Has(entries, Command::CloseDoor));

	// SwitchDoor only acts on brush doors, so a plain door item gets no dead entry.
	facts.item.is_brush_door = false;
	entries = Build(facts);
	REQUIRE_FALSE(Has(entries, Command::SelectDoor));
	REQUIRE_FALSE(Has(entries, Command::OpenDoor));
	REQUIRE_FALSE(Has(entries, Command::CloseDoor));
}

TEST_CASE("doodad, wall, carpet and table selectors need the matching item", "[item_context_menu]") {
	Facts facts;
	facts.item.has_visible_doodad_brush = true;
	REQUIRE(Has(Build(facts), Command::SelectDoodad));

	facts = Facts {};
	facts.item.has_visible_wall_brush = true;
	REQUIRE(Has(Build(facts), Command::SelectWall));
	REQUIRE_FALSE(Has(Build(facts), Command::SelectCarpet));
	REQUIRE_FALSE(Has(Build(facts), Command::SelectTable));

	facts = Facts {};
	facts.item.has_visible_carpet_brush = true;
	REQUIRE(Has(Build(facts), Command::SelectCarpet));
	REQUIRE_FALSE(Has(Build(facts), Command::SelectWall));

	facts = Facts {};
	facts.item.has_visible_table_brush = true;
	REQUIRE(Has(Build(facts), Command::SelectTable));
	REQUIRE_FALSE(Has(Build(facts), Command::SelectCarpet));
}

TEST_CASE("a ground row offers its ground brush", "[item_context_menu]") {
	Facts facts;
	facts.item.has_visible_ground_brush = true;
	facts.item.has_raw_brush = true;
	const auto entries = Build(facts);

	REQUIRE(Has(entries, Command::SelectGround));
	REQUIRE(Has(entries, Command::SelectRaw));
	REQUIRE_FALSE(Has(entries, Command::SelectDoodad));
	REQUIRE_FALSE(Has(entries, Command::SelectWall));

	// Ground is an item row, not a tile-level action: the ground row must not need a stack item.
	facts.item.has_visible_ground_brush = false;
	REQUIRE_FALSE(Has(Build(facts), Command::SelectGround));
}

TEST_CASE("a collection selector needs a collection brush on the item", "[item_context_menu]") {
	Facts facts;
	REQUIRE_FALSE(Has(Build(facts), Command::SelectCollection));
	facts.item.has_collection_brush = true;
	REQUIRE(Has(Build(facts), Command::SelectCollection));
}

TEST_CASE("a door separator never leaves a dangling separator", "[item_context_menu]") {
	Facts facts;
	facts.item.is_door = true;
	facts.item.is_brush_door = true;
	facts.item.has_raw_brush = true;
	auto entries = Build(facts);
	REQUIRE(entries.front().command != Command::Separator);
	REQUIRE(entries.back().command != Command::Separator);
	REQUIRE(Count(entries, Command::Separator) == 3);
	REQUIRE(entries.back().command == Command::SelectRaw);

	// No brush selector after the door: the door separator must disappear too.
	facts.item.has_raw_brush = false;
	entries = Build(facts);
	REQUIRE(entries.back().command == Command::OpenDoor);
	REQUIRE(Count(entries, Command::Separator) == 2);
}

TEST_CASE("the full item menu keeps the canvas order for shared entries", "[item_context_menu]") {
	Facts facts;
	facts.item.is_rotatable = true;
	facts.item.is_teleport = true;
	facts.item.teleport_has_destination = true;
	facts.item.has_visible_doodad_brush = true;
	facts.item.has_visible_wall_brush = true;
	facts.item.has_visible_ground_brush = true;
	facts.item.has_collection_brush = true;
	facts.item.has_raw_brush = true;

	REQUIRE(Commands(Build(facts)) == std::vector<Command> {
											Command::Copy,
											Command::Delete,
											Command::Separator,
											Command::CopyServerId,
											Command::CopyClientId,
											Command::CopyName,
											Command::Separator,
											Command::Rotate,
											Command::GotoDestination,
											Command::SelectRaw,
											Command::SelectWall,
											Command::SelectDoodad,
											Command::SelectGround,
											Command::SelectCollection,
										});
}
