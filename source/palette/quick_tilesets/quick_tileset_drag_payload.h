//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_DRAG_PAYLOAD_H_
#define RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_DRAG_PAYLOAD_H_

#include "palette/quick_tilesets/quick_tileset_model.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Data of a Quick Tileset drag & drop (JSON text in a custom clipboard format). No wx.
namespace QuickTilesets {

	// Client version, source tab (empty outside a user tab) and the dragged references.
	struct DragPayload {
		std::string version;
		std::string tab;
		std::vector<Item> items;
	};

	[[nodiscard]] const char* DragFormatId();
	[[nodiscard]] std::string EncodeDragPayload(const DragPayload& payload);
	// The payload in `data`, or nullopt when it is not valid payload JSON.
	[[nodiscard]] std::optional<DragPayload> ParseDragPayload(std::string_view data);

} // namespace QuickTilesets

#endif
