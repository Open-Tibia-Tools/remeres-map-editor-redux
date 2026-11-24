//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_DROP_TARGET_H_
#define RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_DROP_TARGET_H_

#include "palette/quick_tilesets/quick_tileset_drag_payload.h"

#include <wx/dnd.h>

#include <functional>

namespace QuickTilesets {

	// Accepts Quick Tileset drags of the loaded client version and hands them to `handler`.
	class PayloadDropTarget : public wxDropTarget {
	public:
		using Handler = std::function<bool(wxCoord, wxCoord, const DragPayload&)>;

		explicit PayloadDropTarget(Handler handler);

		wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult result) override;

	private:
		wxCustomDataObject* data_; // owned by wxDropTarget
		Handler handler_;
	};

} // namespace QuickTilesets

#endif
