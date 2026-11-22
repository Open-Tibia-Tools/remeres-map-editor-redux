#ifndef RME_UI_ITEM_CONTEXT_MENU_MODEL_H_
#define RME_UI_ITEM_CONTEXT_MENU_MODEL_H_

#include "ui/context_menu_item_facts.h"

#include <vector>

// Item-scoped context menu for the Tile Properties list. Every entry acts on the clicked row
// (a ground row or a stacked item); tile-level actions belong to TileContextMenu instead.
// Pure index/fact math, no wx or Tile dependency.
namespace ItemContextMenu {

	enum class Command {
		Separator,
		Copy,
		Delete,
		CopyServerId,
		CopyClientId,
		CopyName,
		Rotate,
		GotoDestination,
		OpenDoor,
		CloseDoor,
		SelectRaw,
		SelectWall,
		SelectCarpet,
		SelectTable,
		SelectDoodad,
		SelectDoor,
		SelectGround,
		SelectCollection,
	};

	struct Facts {
		ContextMenu::SelectedItemFacts item;
	};

	struct Entry {
		Command command = Command::Separator;
		bool enabled = true;
	};

	inline std::vector<Entry> Build(const Facts& facts) {
		const ContextMenu::SelectedItemFacts& item = facts.item;
		std::vector<Entry> entries;

		entries.push_back({ Command::Copy });
		entries.push_back({ Command::Delete });
		entries.push_back({ Command::Separator });
		entries.push_back({ Command::CopyServerId });
		entries.push_back({ Command::CopyClientId });
		entries.push_back({ Command::CopyName });

		std::vector<Entry> brushes;
		if (item.has_raw_brush) {
			brushes.push_back({ Command::SelectRaw });
		}
		if (item.has_visible_wall_brush) {
			brushes.push_back({ Command::SelectWall });
		}
		if (item.has_visible_carpet_brush) {
			brushes.push_back({ Command::SelectCarpet });
		}
		if (item.has_visible_table_brush) {
			brushes.push_back({ Command::SelectTable });
		}
		if (item.has_visible_doodad_brush) {
			brushes.push_back({ Command::SelectDoodad });
		}
		if (item.is_brush_door && item.has_door_brush) {
			brushes.push_back({ Command::SelectDoor });
		}
		if (item.has_visible_ground_brush) {
			brushes.push_back({ Command::SelectGround });
		}
		if (item.has_collection_brush) {
			brushes.push_back({ Command::SelectCollection });
		}

		std::vector<Entry> tail;
		if (item.is_rotatable) {
			tail.push_back({ Command::Rotate });
		}
		if (item.is_teleport && item.teleport_has_destination) {
			tail.push_back({ Command::GotoDestination });
		}
		if (item.is_brush_door) {
			tail.push_back({ item.is_open_door ? Command::CloseDoor : Command::OpenDoor });
			if (!brushes.empty()) {
				tail.push_back({ Command::Separator });
			}
		}
		tail.insert(tail.end(), brushes.begin(), brushes.end());

		if (!tail.empty()) {
			entries.push_back({ Command::Separator });
			entries.insert(entries.end(), tail.begin(), tail.end());
		}
		return entries;
	}

} // namespace ItemContextMenu

#endif
