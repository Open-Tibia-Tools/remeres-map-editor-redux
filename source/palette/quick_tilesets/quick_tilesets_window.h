//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_QUICK_TILESETS_WINDOW_H_
#define RME_PALETTE_QUICK_TILESETS_QUICK_TILESETS_WINDOW_H_

#include <wx/panel.h>

class wxNotebook;
class wxButton;

namespace QuickTilesets {

	struct DragPayload;

	// The "Quick Tilesets" pane: a notebook with Recent and the user sets. Each page
	// (quick_tileset_page.h) is split into a Brushes and a RAW grid. Content comes from the facade
	// (quick_tilesets.h); refreshes are coalesced and wait for a running drag to finish.
	class QuickTilesetsWindow : public wxPanel {
	public:
		explicit QuickTilesetsWindow(wxWindow* parent);

		// Re-reads the state into the grids; with `rebuild_tabs` the pages are recreated.
		void QueueRefresh(bool rebuild_tabs);

	private:
		void Rebuild();
		void RefreshPages();
		void ShowTabList(wxButton* anchor);
		void ShowEditMenu(wxButton* anchor);
		bool DropOnTab(int x, int y, const DragPayload& payload);

		wxNotebook* book_ = nullptr;
		bool refresh_queued_ = false;
		bool rebuild_queued_ = false;
		bool rebuilding_ = false;
	};

} // namespace QuickTilesets

#endif
