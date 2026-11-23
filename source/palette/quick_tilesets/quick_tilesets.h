//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_QUICK_TILESETS_H_
#define RME_PALETTE_QUICK_TILESETS_QUICK_TILESETS_H_

#include "palette/quick_tilesets/quick_tileset_model.h"

#include <wx/gdicmn.h>

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class Brush;
class wxWindow;

// Entry points of the Quick Tilesets feature: the dockable pane (Recent + user sets), Favorites,
// the brush context menu and brush drag & drop. State belongs to the loaded client version.
namespace QuickTilesets {

	// Pane lifetime. EnsureWindow() adds the (initially hidden) pane once a client version is
	// loaded; DestroyWindow() detaches it and forgets the version's state. Both are idempotent.
	void EnsureWindow();
	void DestroyWindow();
	[[nodiscard]] wxWindow* GetWindow();
	void ToggleWindow();
	void ShowWindow();

	// A brush the user picked in a brush grid or by committing a palette search.
	void RememberUserPick(Brush* brush);

	[[nodiscard]] std::vector<Brush*> GetFavorites();
	[[nodiscard]] bool IsFavorite(const Brush* brush);
	void ToggleFavorite(Brush* brush);

	// Context menu for `brushes` at `position` (client coordinates of `parent`). `tab_id` names the
	// user tab the brushes are shown in, empty for Recent and palette pages.
	void ShowBrushContextMenu(wxWindow* parent, wxPoint position, std::span<Brush* const> brushes, std::string_view tab_id);

	// Drags `brushes` (collectable ones only) from `source`; `source_tab` makes a drop into the
	// same tab a reorder. Runs the drop source loop synchronously.
	void BeginDrag(wxWindow* source, std::span<Brush* const> brushes, std::string_view source_tab);

	// --- Used by the pane ---

	enum class ChangeScope {
		Items, // entries of Recent or a tab
		Tabs, // tabs added, removed, renamed or reordered
		Favorites,
		Layout, // active tab or splitter position: saved, nothing to refresh
	};

	// The loaded version's state (empty without a client version).
	[[nodiscard]] const State& GetState();
	[[nodiscard]] std::string_view LoadedVersion();
	// Applies and saves a change, then refreshes what shows it. Returns whether it changed.
	bool Apply(const std::function<bool(State&)>& change, ChangeScope scope);
	// Asks for a name and creates a tab; returns its id, or empty when cancelled.
	[[nodiscard]] std::string CreateTabInteractive(wxWindow* parent);

	// Drag payload: client version, source tab and references.
	struct DragPayload {
		std::string version;
		std::string tab;
		std::vector<Item> items;
	};
	[[nodiscard]] const char* DragFormatId();
	[[nodiscard]] std::optional<DragPayload> ParseDragPayload(std::string_view data);
	[[nodiscard]] bool IsDragging();

	// "&" shows as a mnemonic in menus and notebook tabs.
	[[nodiscard]] std::string EscapeMnemonics(std::string_view text);

} // namespace QuickTilesets

#endif
