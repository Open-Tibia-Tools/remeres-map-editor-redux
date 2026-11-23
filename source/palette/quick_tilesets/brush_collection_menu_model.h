//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_BRUSH_COLLECTION_MENU_MODEL_H_
#define RME_PALETTE_QUICK_TILESETS_BRUSH_COLLECTION_MENU_MODEL_H_

#include <cstddef>
#include <string>
#include <vector>

// Context menu of the brushes selected in a palette or Quick Tileset grid, decided without wx
// (see docs/ui/context-menus.md). The popup builder turns entries into menu items and the
// dispatcher runs the command for the chosen one.
namespace QuickTilesets::BrushMenu {

	enum class Command {
		AddFavorites,
		RemoveFavorites,
		AddToTab, // entry.tab_index selects the tab
		NewTab,
		RemoveFromTab,
	};

	struct Facts {
		size_t selection_count = 0;
		bool all_favorite = false;
		std::vector<std::string> tab_names; // user tabs, in pane order
		bool inside_user_tab = false; // the grid shows a user tab (not Recent or a palette)
	};

	struct Entry {
		Command command;
		std::string label;
		size_t tab_index = 0;
		bool in_tab_submenu = false;
		bool separator_before = false;
	};

	inline constexpr const char* kTabSubmenuLabel = "Add to Quick Tileset";

	[[nodiscard]] inline std::vector<Entry> Build(const Facts& facts) {
		std::vector<Entry> entries;
		if (facts.selection_count == 0) {
			return entries;
		}

		if (facts.all_favorite) {
			entries.push_back({ .command = Command::RemoveFavorites, .label = "Remove from Favorites" });
		} else {
			entries.push_back({ .command = Command::AddFavorites, .label = "Add to Favorites" });
		}

		for (size_t index = 0; index < facts.tab_names.size(); ++index) {
			entries.push_back({ .command = Command::AddToTab, .label = facts.tab_names[index], .tab_index = index, .in_tab_submenu = true });
		}
		entries.push_back({ .command = Command::NewTab, .label = "New Quick Tileset...", .in_tab_submenu = true, .separator_before = !facts.tab_names.empty() });

		if (facts.inside_user_tab) {
			entries.push_back({ .command = Command::RemoveFromTab, .label = "Remove from this set", .separator_before = true });
		}
		return entries;
	}

} // namespace QuickTilesets::BrushMenu

#endif
