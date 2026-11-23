//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/quick_tilesets/quick_tilesets.h"

#include "app/main.h"
#include "app/managers/version_manager.h"
#include "palette/managers/palette_manager.h"
#include "palette/palette_window.h"
#include "palette/quick_tilesets/brush_collection_menu_model.h"
#include "palette/quick_tilesets/brush_reference.h"
#include "palette/quick_tilesets/quick_tileset_store.h"
#include "palette/quick_tilesets/quick_tilesets_window.h"
#include "ui/brush_items/brush_items_window.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "util/file_system.h"

#include <wx/aui/aui.h>
#include <wx/dnd.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>

namespace QuickTilesets {

	namespace {

		constexpr const char* kPaneName = "QuickTilesets";

		Store g_store;
		QuickTilesetsWindow* g_window = nullptr;
		bool g_dragging = false;
		bool g_error_reported = false;

		wxString StoragePath() {
			return FileSystem::GetLocalDataDirectory() + "quick-tilesets.json";
		}

		// Shows a store error once per loaded version, outside the current event handler.
		void ReportError(const std::string& message) {
			if (g_error_reported) {
				return;
			}
			g_error_reported = true;
			wxTheApp->CallAfter([message]() {
				wxMessageBox(wxString::FromUTF8(message), "Quick Tilesets", wxOK | wxICON_WARNING, g_gui.root);
			});
		}

		// Loads the state of the current client version on first use; false without a version.
		bool EnsureLoaded() {
			if (!g_version.IsVersionLoaded()) {
				return false;
			}
			const std::string current = g_version.GetCurrentVersionID();
			if (g_store.IsLoaded() && g_store.Version() == current) {
				return true;
			}
			g_error_reported = false;
			if (auto error = g_store.Load(StoragePath(), current)) {
				ReportError(*error);
			}
			return true;
		}

		void NotifyFavoritesChanged() {
			for (PaletteWindow* palette : g_palettes.GetPalettes()) {
				palette->OnFavoritesChanged();
			}
			if (g_gui.brush_items_window) {
				g_gui.brush_items_window->OnFavoritesChanged();
			}
		}

		void RunMenuCommand(const BrushMenu::Entry& entry, const std::vector<Item>& items, const std::vector<std::string>& tab_ids, std::string_view tab_id, wxWindow* parent) {
			using BrushMenu::Command;
			const auto insert_into = [&items](const std::string& id) {
				Apply([&](State& state) {
					Tab* tab = FindTab(state, id);
					if (!tab) {
						return false;
					}
					const auto previous = tab->items;
					Insert(*tab, items);
					return tab->items != previous;
				}, ChangeScope::Items);
			};

			switch (entry.command) {
				case Command::AddFavorites:
					Apply([&](State& state) { return AddFavorites(state, items); }, ChangeScope::Favorites);
					break;
				case Command::RemoveFavorites:
					Apply([&](State& state) { return RemoveFavorites(state, items); }, ChangeScope::Favorites);
					break;
				case Command::AddToTab:
					if (entry.tab_index < tab_ids.size()) {
						insert_into(tab_ids[entry.tab_index]);
					}
					break;
				case Command::NewTab:
					if (const std::string id = CreateTabInteractive(parent); !id.empty()) {
						insert_into(id);
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

	void EnsureWindow() {
		if (g_window || !g_gui.aui_manager || !g_gui.root || !EnsureLoaded()) {
			return;
		}
		g_window = new QuickTilesetsWindow(g_gui.root);
		wxAuiPaneInfo info;
		info.Name(kPaneName)
			.Caption("Quick Tilesets")
			.Right()
			.Layer(1)
			.BestSize(g_window->FromDIP(wxSize(420, 320)))
			.MinSize(g_window->FromDIP(wxSize(240, 180)))
			.FloatingSize(g_window->FromDIP(wxSize(420, 320)))
			.CloseButton(true)
			.Floatable(true)
			.Dockable(true)
			.Resizable(true)
			.DestroyOnClose(false)
			.Hide();
		g_gui.aui_manager->AddPane(g_window, info);
	}

	void DestroyWindow() {
		if (g_window) {
			if (g_gui.aui_manager) {
				g_gui.aui_manager->DetachPane(g_window);
			}
			g_window->Destroy();
			g_window = nullptr;
		}
		g_store.Unload();
		g_error_reported = false;
	}

	wxWindow* GetWindow() {
		return g_window;
	}

	void ToggleWindow() {
		EnsureWindow();
		if (!g_window || !g_gui.aui_manager) {
			return;
		}
		wxAuiPaneInfo& pane = g_gui.aui_manager->GetPane(g_window);
		pane.Show(!pane.IsShown());
		g_gui.aui_manager->Update();
	}

	void ShowWindow() {
		EnsureWindow();
		if (!g_window || !g_gui.aui_manager) {
			return;
		}
		wxAuiPaneInfo& pane = g_gui.aui_manager->GetPane(g_window);
		if (!pane.IsShown()) {
			pane.Show();
			g_gui.aui_manager->Update();
		}
	}

	void RememberUserPick(Brush* brush) {
		if (!brush) {
			return;
		}
		if (const auto reference = MakeReference(*brush)) {
			Apply([&](State& state) { return Remember(state, *reference); }, ChangeScope::Items);
		}
	}

	std::vector<Brush*> GetFavorites() {
		std::vector<Brush*> brushes;
		if (!EnsureLoaded()) {
			return brushes;
		}
		for (const Item& item : g_store.GetState().favorites) {
			if (Brush* brush = Resolve(item)) {
				brushes.push_back(brush);
			}
		}
		return brushes;
	}

	bool IsFavorite(const Brush* brush) {
		if (!brush || !EnsureLoaded()) {
			return false;
		}
		const auto reference = MakeReference(*brush);
		return reference && ContainsKey(g_store.GetState().favorites, reference->key);
	}

	void ToggleFavorite(Brush* brush) {
		if (!brush) {
			return;
		}
		if (const auto reference = MakeReference(*brush)) {
			const std::vector<Item> items { *reference };
			Apply([&](State& state) { return ToggleFavorites(state, items); }, ChangeScope::Favorites);
		}
	}

	void ShowBrushContextMenu(wxWindow* parent, wxPoint position, std::span<Brush* const> brushes, std::string_view tab_id) {
		if (!parent || !EnsureLoaded()) {
			return;
		}
		const std::vector<Item> items = MakeReferences(brushes);
		if (items.empty()) {
			return;
		}

		const State& state = g_store.GetState();
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
		auto* tab_menu = new wxMenu;
		bool tab_menu_attached = false;
		for (size_t index = 0; index < entries.size(); ++index) {
			const BrushMenu::Entry& entry = entries[index];
			if (entry.in_tab_submenu && !tab_menu_attached) {
				menu.AppendSubMenu(tab_menu, BrushMenu::kTabSubmenuLabel);
				tab_menu_attached = true;
			}
			wxMenu& target = entry.in_tab_submenu ? *tab_menu : menu;
			if (entry.separator_before) {
				target.AppendSeparator();
			}
			target.Append(kFirstId + static_cast<int>(index), wxstr(EscapeMnemonics(entry.label)));
		}
		if (!tab_menu_attached) {
			delete tab_menu;
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

	void BeginDrag(wxWindow* source, std::span<Brush* const> brushes, std::string_view source_tab) {
		if (!source || !EnsureLoaded()) {
			return;
		}
		const std::vector<Item> items = MakeReferences(brushes);
		if (items.empty()) {
			return;
		}
		const std::string payload = nlohmann::json {
			{ "version", g_store.Version() },
			{ "tab", std::string(source_tab) },
			{ "items", items },
		}.dump();

		wxCustomDataObject data { wxDataFormat(DragFormatId()) };
		data.SetData(payload.size(), payload.data());
		wxDropSource drop_source(source);
		drop_source.SetData(data);
		g_dragging = true;
		drop_source.DoDragDrop(wxDrag_CopyOnly);
		g_dragging = false;
		if (g_window) {
			g_window->QueueRefresh(false);
		}
	}

	const State& GetState() {
		static const State kEmpty;
		return EnsureLoaded() ? g_store.GetState() : kEmpty;
	}

	std::string_view LoadedVersion() {
		return g_store.Version();
	}

	bool Apply(const std::function<bool(State&)>& change, ChangeScope scope) {
		if (!EnsureLoaded()) {
			return false;
		}
		const Store::MutateResult result = g_store.Mutate(change);
		if (result.error) {
			ReportError(*result.error);
		}
		if (!result.changed) {
			return false;
		}
		switch (scope) {
			case ChangeScope::Items:
				if (g_window) {
					g_window->QueueRefresh(false);
				}
				break;
			case ChangeScope::Tabs:
				if (g_window) {
					g_window->QueueRefresh(true);
				}
				break;
			case ChangeScope::Favorites:
				NotifyFavoritesChanged();
				break;
			case ChangeScope::Layout:
				break;
		}
		return true;
	}

	std::string CreateTabInteractive(wxWindow* parent) {
		wxTextEntryDialog dialog(parent, "Name of the new set:", "Add Quick Tileset");
		if (dialog.ShowModal() != wxID_OK) {
			return {};
		}
		const std::string name = dialog.GetValue().ToStdString(wxConvUTF8);
		std::string id = NewTabId();
		if (!Apply([&](State& state) { return CreateTab(state, id, name); }, ChangeScope::Tabs)) {
			return {};
		}
		return id;
	}

	const char* DragFormatId() {
		return "application/x-rme-quick-tileset-v1";
	}

	std::optional<DragPayload> ParseDragPayload(std::string_view data) {
		try {
			const nlohmann::json json = nlohmann::json::parse(data);
			return DragPayload {
				.version = json.at("version").get<std::string>(),
				.tab = json.value("tab", std::string {}),
				.items = json.at("items").get<std::vector<Item>>(),
			};
		} catch (const std::exception&) {
			return std::nullopt;
		}
	}

	bool IsDragging() {
		return g_dragging;
	}

	std::string EscapeMnemonics(std::string_view text) {
		std::string escaped;
		escaped.reserve(text.size());
		for (const char character : text) {
			escaped += character;
			if (character == '&') {
				escaped += '&';
			}
		}
		return escaped;
	}

} // namespace QuickTilesets
