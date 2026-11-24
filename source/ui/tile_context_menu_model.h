//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_TILE_CONTEXT_MENU_MODEL_H_
#define RME_UI_TILE_CONTEXT_MENU_MODEL_H_

#include "ui/context_menu_item_facts.h"

#include <cstddef>
#include <optional>
#include <vector>

// Decides which entries the tile context menu shows and which of them are enabled.
// The map canvas builds its menu from this model; the Tile Properties list has its own
// item-scoped model (ItemContextMenu) because it acts on one row, not on the whole tile.
// It reads only plain facts about the current selection, so it has no wx or Tile dependency.
namespace TileContextMenu {

	enum class Command {
		Separator,
		Cut,
		Copy,
		CopyPosition,
		Paste,
		Delete,
		AdvancedReplace,
		AddToCollection,
		CopyServerId,
		CopyClientId,
		CopyName,
		Rotate,
		GotoDestination,
		OpenDoor,
		CloseDoor,
		SelectCreature,
		SelectSpawn,
		SelectRaw,
		SelectWall,
		SelectCarpet,
		SelectTable,
		SelectDoodad,
		SelectDoor,
		SelectGround,
		SelectCollection,
		SelectHouse,
		Properties,
		BrowseField,
		TileProperties,
		Script,
	};

	// Facts about the one selected item (ground or stacked item) when exactly one is selected.
	using SelectedItemFacts = ContextMenu::SelectedItemFacts;

	struct Facts {
		// Number of selected tiles.
		std::size_t selected_tile_count = 0;
		bool can_paste = false;

		// The fields below describe the selected tile; they are read only when exactly one tile is selected.
		bool has_ground = false;
		bool has_visible_ground_brush = false;
		bool ground_brush_has_collection = false;
		// At least one stacked item (not the ground) is selected.
		bool has_selected_stack_item = false;
		bool has_creature = false;
		bool has_spawn = false;
		bool is_house_tile = false;
		// Some stacked item carries a palette-visible brush of that kind.
		bool has_wall_brush = false;
		bool has_carpet_brush = false;
		bool has_table_brush = false;
		// Some stacked wall/table/carpet/doodad brush belongs to a collection.
		bool stack_has_collection = false;
		std::optional<SelectedItemFacts> selected_item;

		// Registered Lua context-menu entries, already clamped to the available menu ids.
		std::size_t script_count = 0;
	};

	struct Entry {
		Command command = Command::Separator;
		bool enabled = true;
		// Index into the Lua context-menu entries; only meaningful for Command::Script.
		std::size_t script_index = 0;
	};

	namespace detail {
		inline void AppendSelectionBrushes(std::vector<Entry>& entries, const Facts& facts) {
			const SelectedItemFacts* item = facts.selected_item ? &*facts.selected_item : nullptr;

			if (facts.has_creature) {
				entries.push_back({ Command::SelectCreature });
			}
			if (facts.has_spawn) {
				entries.push_back({ Command::SelectSpawn });
			}
			entries.push_back({ Command::SelectRaw });
			if (facts.has_wall_brush) {
				entries.push_back({ Command::SelectWall });
			}
			if (facts.has_carpet_brush) {
				entries.push_back({ Command::SelectCarpet });
			}
			if (facts.has_table_brush) {
				entries.push_back({ Command::SelectTable });
			}
			if (item && item->has_visible_doodad_brush) {
				entries.push_back({ Command::SelectDoodad });
			}
			if (item && item->is_brush_door && item->has_door_brush) {
				entries.push_back({ Command::SelectDoor });
			}
			if (facts.has_visible_ground_brush) {
				entries.push_back({ Command::SelectGround });
			}
			if (facts.stack_has_collection || (item && item->has_collection_brush) || facts.ground_brush_has_collection) {
				entries.push_back({ Command::SelectCollection });
			}
			if (facts.is_house_tile) {
				entries.push_back({ Command::SelectHouse });
			}
			entries.push_back({ Command::Separator });
			entries.push_back({ Command::Properties });
		}

		// Tile without ground, creature or selected item: only tile-level brushes apply.
		inline void AppendBareTileBrushes(std::vector<Entry>& entries, const Facts& facts) {
			if (facts.has_spawn) {
				entries.push_back({ Command::SelectSpawn });
			}
			entries.push_back({ Command::SelectRaw });
			if (facts.has_wall_brush) {
				entries.push_back({ Command::SelectWall });
			}
			if (facts.stack_has_collection) {
				entries.push_back({ Command::SelectCollection });
			}
			if (facts.is_house_tile) {
				entries.push_back({ Command::SelectHouse });
			}
			if (facts.has_spawn) {
				entries.push_back({ Command::Separator });
				entries.push_back({ Command::Properties });
			}
		}

		inline void AppendItemOperations(std::vector<Entry>& entries, const SelectedItemFacts& item) {
			if (!item.is_brush_door && !item.is_rotatable && !item.is_teleport) {
				return;
			}
			if (item.is_rotatable) {
				entries.push_back({ Command::Rotate });
			}
			if (item.is_teleport && item.teleport_has_destination) {
				entries.push_back({ Command::GotoDestination });
			}
			if (item.is_door) {
				entries.push_back({ item.is_open_door ? Command::CloseDoor : Command::OpenDoor });
				entries.push_back({ Command::Separator });
			}
		}
	} // namespace detail

	inline std::vector<Entry> Build(const Facts& facts) {
		std::vector<Entry> entries;
		const bool anything_selected = facts.selected_tile_count != 0;

		entries.push_back({ Command::Cut, anything_selected });
		entries.push_back({ Command::Copy, anything_selected });
		entries.push_back({ Command::CopyPosition, anything_selected });
		entries.push_back({ Command::Paste, facts.can_paste });
		entries.push_back({ Command::Delete, anything_selected });

		if (!anything_selected) {
			return entries;
		}

		entries.push_back({ Command::AdvancedReplace });
		entries.push_back({ Command::AddToCollection });

		if (facts.selected_tile_count != 1) {
			return entries;
		}

		entries.push_back({ Command::Separator });

		if (facts.selected_item) {
			entries.push_back({ Command::CopyServerId });
			entries.push_back({ Command::CopyClientId });
			entries.push_back({ Command::CopyName });
			entries.push_back({ Command::Separator });
		}

		if (facts.selected_item || facts.has_creature || facts.has_selected_stack_item || facts.has_ground) {
			if (facts.selected_item) {
				detail::AppendItemOperations(entries, *facts.selected_item);
			}
			detail::AppendSelectionBrushes(entries, facts);
		} else {
			detail::AppendBareTileBrushes(entries, facts);
		}

		entries.push_back({ Command::Separator });
		entries.push_back({ Command::BrowseField });
		entries.push_back({ Command::TileProperties });

		if (facts.script_count != 0) {
			entries.push_back({ Command::Separator });
			for (std::size_t i = 0; i < facts.script_count; ++i) {
				entries.push_back({ Command::Script, true, i });
			}
		}

		return entries;
	}

} // namespace TileContextMenu

#endif
