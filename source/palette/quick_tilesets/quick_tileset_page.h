//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_PAGE_H_
#define RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_PAGE_H_

#include "game/material_database.h"

#include <wx/panel.h>

#include <string>

class VirtualBrushGrid;
class wxCommandEvent;
class wxContextMenuEvent;
class wxSplitterWindow;
class wxStaticText;

namespace QuickTilesets {

	struct DragPayload;

	// One notebook page of the Quick Tilesets pane: Recent (empty tab id) or a user set, split
	// into a Brushes and a RAW grid. Picks feed Recent; drops insert into the set.
	class QuickTilesetPage : public wxPanel {
	public:
		QuickTilesetPage(wxWindow* parent, std::string tab_id);

		[[nodiscard]] const std::string& TabId() const {
			return tab_id_;
		}

		// Re-reads the page's entries from the state into both grids.
		void RefreshItems();

	private:
		wxWindow* MakeGridPanel(const char* title, DynamicTilesetDefinition& set, VirtualBrushGrid*& grid);
		void OnPicked(wxCommandEvent& event);
		void OnBeginDrag(wxCommandEvent& event);
		void OnContextMenu(wxContextMenuEvent& event);
		bool Drop(VirtualBrushGrid* grid, wxCoord x, wxCoord y, const DragPayload& payload);

		std::string tab_id_;
		wxSplitterWindow* split_ = nullptr;
		DynamicTilesetDefinition brushes_;
		DynamicTilesetDefinition raws_;
		VirtualBrushGrid* brush_grid_ = nullptr;
		VirtualBrushGrid* raw_grid_ = nullptr;
		wxStaticText* hint_ = nullptr;
	};

} // namespace QuickTilesets

#endif
