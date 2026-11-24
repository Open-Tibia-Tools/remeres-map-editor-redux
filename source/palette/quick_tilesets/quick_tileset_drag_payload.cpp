//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/quick_tilesets/quick_tileset_drag_payload.h"

#include "palette/quick_tilesets/quick_tileset_json.h"

#include <exception>

namespace QuickTilesets {

	const char* DragFormatId() {
		return "application/x-rme-quick-tileset-v1";
	}

	std::string EncodeDragPayload(const DragPayload& payload) {
		return nlohmann::json {
			{ "version", payload.version },
			{ "tab", payload.tab },
			{ "items", payload.items },
		}.dump();
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

} // namespace QuickTilesets
