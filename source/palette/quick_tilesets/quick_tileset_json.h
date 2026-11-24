//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_JSON_H_
#define RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_JSON_H_

#include "palette/quick_tilesets/quick_tileset_model.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// JSON form of the Quick Tilesets model: the file document ({"schema":1,"versions":{...}}) and
// the entries used by the drag payload. Included only where JSON is read or written.
namespace QuickTilesets {

	inline constexpr int kDocumentSchema = 1;

	inline void to_json(nlohmann::json& j, const Item& item) {
		j = { { "key", item.key }, { "palette", item.palette } };
	}

	inline void from_json(const nlohmann::json& j, Item& item) {
		item.key = j.at("key").get<std::string>();
		item.palette = j.value("palette", kUnknownPalette);
		const bool known_prefix = item.isRaw() || item.key.starts_with("b:") || item.key.starts_with("c:");
		if (item.key.size() < 3 || !known_prefix || item.palette < 0 || item.palette > kUnknownPalette) {
			throw std::runtime_error("Invalid Quick Tileset entry");
		}
	}

	inline void to_json(nlohmann::json& j, const Tab& tab) {
		j = { { "id", tab.id }, { "name", tab.name }, { "items", tab.items } };
	}

	inline void from_json(const nlohmann::json& j, Tab& tab) {
		j.at("id").get_to(tab.id);
		j.at("name").get_to(tab.name);
		j.at("items").get_to(tab.items);
		if (tab.id.empty() || tab.name.empty()) {
			throw std::runtime_error("Invalid Quick Tileset tab");
		}
	}

	[[nodiscard]] inline nlohmann::json Encode(const State& state) {
		return {
			{ "recent", state.recent },
			{ "favorites", state.favorites },
			{ "tabs", state.tabs },
			{ "active", state.active },
			{ "split", state.split },
		};
	}

	// Throws std::runtime_error (or nlohmann::json::exception) on invalid data. Duplicate entries
	// collapse, unknown references are kept, a duplicate tab id is rejected.
	[[nodiscard]] inline State Decode(const nlohmann::json& j) {
		State state;
		state.active = j.value("active", std::string {});
		state.split = std::clamp(j.value("split", 0.5), 0.15, 0.85);

		const auto recent = j.value("recent", std::vector<Item> {});
		for (auto it = recent.rbegin(); it != recent.rend(); ++it) {
			Remember(state, *it);
		}
		AddFavorites(state, j.value("favorites", std::vector<Item> {}));

		for (Tab& tab : j.value("tabs", std::vector<Tab> {})) {
			if (FindTab(state, tab.id)) {
				throw std::runtime_error("Duplicate Quick Tileset tab");
			}
			Tab clean { .id = std::move(tab.id), .name = std::move(tab.name), .items = {} };
			Insert(clean, tab.items);
			state.tabs.push_back(std::move(clean));
		}
		return state;
	}

	[[nodiscard]] inline nlohmann::json EmptyDocument() {
		return { { "schema", kDocumentSchema }, { "versions", nlohmann::json::object() } };
	}

	// Throws when `document` is not a schema-1 Quick Tilesets file.
	inline void ValidateDocument(const nlohmann::json& document) {
		if (!document.is_object() || document.value("schema", 0) != kDocumentSchema || !document.contains("versions") || !document.at("versions").is_object()) {
			throw std::runtime_error("Unsupported Quick Tilesets file");
		}
	}

	// State of one client version, or an empty state when the document has no section for it.
	[[nodiscard]] inline State DecodeVersion(const nlohmann::json& document, std::string_view version) {
		const auto& versions = document.at("versions");
		const auto it = versions.find(std::string(version));
		return it == versions.end() ? State {} : Decode(*it);
	}

	// Replaces only the section of `version`; other client versions stay as they are on disk.
	inline void MergeVersionSection(nlohmann::json& document, std::string_view version, const State& state) {
		document["versions"][std::string(version)] = Encode(state);
	}

} // namespace QuickTilesets

#endif
