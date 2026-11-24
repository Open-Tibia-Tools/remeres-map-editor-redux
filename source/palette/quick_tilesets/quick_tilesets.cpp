//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/quick_tilesets/quick_tilesets.h"

#include "app/main.h"
#include "app/managers/version_manager.h"
#include "palette/managers/palette_manager.h"
#include "palette/palette_window.h"
#include "palette/quick_tilesets/brush_reference.h"
#include "palette/quick_tilesets/quick_tileset_drag_payload.h"
#include "palette/quick_tilesets/quick_tileset_store.h"
#include "palette/quick_tilesets/quick_tilesets_window.h"
#include "ui/brush_items/brush_items_window.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "util/file_system.h"

#include <wx/aui/aui.h>
#include <wx/dnd.h>
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

		void NotifyFavoritesChanged() {
			for (PaletteWindow* palette : g_palettes.GetPalettes()) {
				palette->OnFavoritesChanged();
			}
			if (g_gui.brush_items_window) {
				g_gui.brush_items_window->OnFavoritesChanged();
			}
		}

	} // namespace

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

	void BeginDrag(wxWindow* source, std::span<Brush* const> brushes, std::string_view source_tab) {
		if (!source || !EnsureLoaded()) {
			return;
		}
		const std::vector<Item> items = MakeReferences(brushes);
		if (items.empty()) {
			return;
		}
		const std::string payload = EncodeDragPayload(DragPayload {
			.version = g_store.Version(),
			.tab = std::string(source_tab),
			.items = items,
		});

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

	bool InsertIntoTab(std::string_view id, std::span<const Item> items, std::string_view before, bool reorder) {
		return Apply([&](State& state) {
			Tab* tab = FindTab(state, id);
			if (!tab) {
				return false;
			}
			const auto previous = tab->items;
			Insert(*tab, items, before, reorder);
			return tab->items != previous;
		}, ChangeScope::Items);
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
