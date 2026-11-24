//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/quick_tilesets/quick_tileset_drop_target.h"

#include "palette/quick_tilesets/quick_tilesets.h"

#include <string_view>
#include <utility>

namespace QuickTilesets {

	PayloadDropTarget::PayloadDropTarget(Handler handler) :
		data_(new wxCustomDataObject(wxDataFormat(DragFormatId()))),
		handler_(std::move(handler)) {
		SetDataObject(data_);
	}

	wxDragResult PayloadDropTarget::OnData(wxCoord x, wxCoord y, wxDragResult) {
		if (!GetData()) {
			return wxDragNone;
		}
		const auto payload = ParseDragPayload(std::string_view(static_cast<const char*>(data_->GetData()), data_->GetSize()));
		if (!payload || payload->version != LoadedVersion() || payload->items.empty()) {
			return wxDragNone;
		}
		return handler_(x, y, *payload) ? wxDragCopy : wxDragNone;
	}

} // namespace QuickTilesets
