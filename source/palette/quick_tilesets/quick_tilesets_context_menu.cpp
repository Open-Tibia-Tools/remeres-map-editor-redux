//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

// Brush context menu of the Quick Tilesets facade (quick_tilesets.h): favorites, sets, removal.

#include "palette/quick_tilesets/quick_tilesets.h"

#include "app/main.h"
#include "palette/quick_tilesets/brush_collection_menu_model.h"
#include "palette/quick_tilesets/brush_reference.h"

#include <wx/menu.h>

#include <memory>

namespace QuickTilesets {

	namespace {

		void RunMenuCommand(const BrushMenu::Entry& entry, const std::vector<Item>& items, const std::vector<std::string>& tab_ids, std::string_view tab_id, wxWindow* parent) {
			using BrushMenu::Command;
			switch (entry.command) {
				case Command::AddFavorites:
					Apply([&](State& state) { return AddFavorites(state, items); }, ChangeScope::Favorites);
					break;
				case Command::RemoveFavorites:
					Apply([&](State& state) { return RemoveFavorites(state, items); }, ChangeScope::Favorites);
					break;
				case Command::AddToTab:
					if (entry.tab_index < tab_ids.size()) {
						InsertIntoTab(tab_ids[entry.tab_index], items);
					}
					break;
				case Command::NewTab:
					if (const std::string id = CreateTabInteractive(parent); !id.empty()) {
						InsertIntoTab(id, items);
						ShowWindow();
					}
					break;
				case Command::RemoveFromTab:
					Apply([&](State& state) {
						Tab* tab = FindTab(state, tab_id);
						return tab && RemoveFromTab(*tab, items);
					}, ChangeScope::Items);
					break;
			}
		}

	} // namespace

	void ShowBrushContextMenu(wxWindow* parent, wxPoint position, std::span<Brush* const> brushes, std::string_view tab_id) {
		if (!parent || !EnsureLoaded()) {
			return;
		}
		const std::vector<Item> items = MakeReferences(brushes);
		if (items.empty()) {
			return;
		}

		const State& state = GetState();
		BrushMenu::Facts facts {
			.selection_count = items.size(),
			.all_favorite = AllFavorite(state, items),
			.tab_names = {},
			.inside_user_tab = !tab_id.empty() && FindTab(state, tab_id) != nullptr,
		};
		std::vector<std::string> tab_ids;
		for (const Tab& tab : state.tabs) {
			facts.tab_names.push_back(tab.name);
			tab_ids.push_back(tab.id);
		}
		const std::vector<BrushMenu::Entry> entries = BrushMenu::Build(facts);

		constexpr int kFirstId = wxID_HIGHEST + 1;
		wxMenu menu;
		// Owned here until AppendSubMenu() hands it to `menu`; freed if no entry needs it.
		auto tab_menu = std::make_unique<wxMenu>();
		wxMenu* tab_target = tab_menu.get();
		for (size_t index = 0; index < entries.size(); ++index) {
			const BrushMenu::Entry& entry = entries[index];
			if (entry.in_tab_submenu && tab_menu) {
				menu.AppendSubMenu(tab_menu.release(), BrushMenu::kTabSubmenuLabel);
			}
			wxMenu& target = entry.in_tab_submenu ? *tab_target : menu;
			if (entry.separator_before) {
				target.AppendSeparator();
			}
			target.Append(kFirstId + static_cast<int>(index), wxstr(EscapeMnemonics(entry.label)));
		}

		const int chosen = parent->GetPopupMenuSelectionFromUser(menu, position);
		if (chosen == wxID_NONE) {
			return;
		}
		const size_t index = static_cast<size_t>(chosen - kFirstId);
		if (index < entries.size()) {
			const std::string tab(tab_id);
			RunMenuCommand(entries[index], items, tab_ids, tab, parent);
		}
	}

} // namespace QuickTilesets
