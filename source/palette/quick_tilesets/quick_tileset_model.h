//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_MODEL_H_
#define RME_PALETTE_QUICK_TILESETS_QUICK_TILESET_MODEL_H_

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Pure model of the Quick Tilesets pane: the Recent list, Favorites and user-named sets. No wx,
// no brushes, no JSON (quick_tileset_json.h): entries are string references resolved by the store.
namespace QuickTilesets {

	inline constexpr size_t kRecentBrushLimit = 20;
	inline constexpr size_t kRecentRawLimit = 60;
	// The palette field is kept for file compatibility with BME; RME Redux does not read it.
	inline constexpr int kUnknownPalette = 8;

	// "r:<server id>" (RAW item), "c:<creature name>" or "b:<brush name>".
	struct Item {
		std::string key;
		int palette = kUnknownPalette;

		[[nodiscard]] bool isRaw() const {
			return key.starts_with("r:");
		}
		bool operator==(const Item&) const = default;
	};

	struct Tab {
		std::string id;
		std::string name;
		std::vector<Item> items;
		bool operator==(const Tab&) const = default;
	};

	struct State {
		std::vector<Item> recent;
		std::vector<Item> favorites;
		std::vector<Tab> tabs;
		std::string active;
		double split = 0.5;
	};

	[[nodiscard]] inline std::string RawKey(uint32_t server_id) {
		return "r:" + std::to_string(server_id);
	}

	// Server id of a RAW reference ("r:<decimal id>", id > 0, fits in 32 bits), or nullopt.
	[[nodiscard]] inline std::optional<uint32_t> ParseRawKey(std::string_view key) {
		if (!key.starts_with("r:")) {
			return std::nullopt;
		}
		const std::string_view digits = key.substr(2);
		uint32_t value = 0;
		const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
		if (digits.empty() || error != std::errc {} || end != digits.data() + digits.size() || value == 0) {
			return std::nullopt;
		}
		return value;
	}

	[[nodiscard]] inline bool ContainsKey(std::span<const Item> items, std::string_view key) {
		return std::ranges::any_of(items, [&](const Item& item) { return item.key == key; });
	}

	// Moves `item` to the front of Recent and trims the list to 20 brushes / 60 RAW items.
	// Returns false (and changes nothing) when it already is the front entry.
	inline bool Remember(State& state, const Item& item) {
		if (!state.recent.empty() && state.recent.front().key == item.key) {
			return false;
		}
		std::erase_if(state.recent, [&](const Item& entry) { return entry.key == item.key; });
		state.recent.insert(state.recent.begin(), item);
		size_t raw = 0;
		size_t brushes = 0;
		std::erase_if(state.recent, [&](const Item& entry) {
			return entry.isRaw() ? ++raw > kRecentRawLimit : ++brushes > kRecentBrushLimit;
		});
		return true;
	}

	// Inserts `items` before the entry with key `before` (append when absent). Without `reorder`,
	// entries already in the tab keep their position (copy from another palette); with `reorder`
	// (drag within the same tab) they move. Dropping onto one of the moved entries is a no-op.
	inline void Insert(Tab& tab, std::span<const Item> items, std::string_view before = {}, bool reorder = false) {
		std::vector<Item> add;
		for (const Item& item : items) {
			if (ContainsKey(add, item.key) || (!reorder && ContainsKey(tab.items, item.key))) {
				continue;
			}
			add.push_back(item);
		}
		if (ContainsKey(add, before)) {
			return;
		}
		if (reorder) {
			std::erase_if(tab.items, [&](const Item& entry) { return ContainsKey(add, entry.key); });
		}
		const auto position = std::ranges::find_if(tab.items, [&](const Item& entry) { return entry.key == before; });
		tab.items.insert(position, add.begin(), add.end());
	}

	// Removes every entry of `tab` whose key is in `items`. Returns whether anything changed.
	inline bool RemoveFromTab(Tab& tab, std::span<const Item> items) {
		return std::erase_if(tab.items, [&](const Item& entry) { return ContainsKey(items, entry.key); }) > 0;
	}

	[[nodiscard]] inline bool AllFavorite(const State& state, std::span<const Item> items) {
		return !items.empty() && std::ranges::all_of(items, [&](const Item& item) { return ContainsKey(state.favorites, item.key); });
	}

	// Appends the items that are not favorites yet; existing favorites keep their position.
	inline bool AddFavorites(State& state, std::span<const Item> items) {
		bool changed = false;
		for (const Item& item : items) {
			if (!ContainsKey(state.favorites, item.key)) {
				state.favorites.push_back(item);
				changed = true;
			}
		}
		return changed;
	}

	inline bool RemoveFavorites(State& state, std::span<const Item> items) {
		return std::erase_if(state.favorites, [&](const Item& entry) { return ContainsKey(items, entry.key); }) > 0;
	}

	// "Add to Favorites" when at least one item is missing, otherwise "Remove from Favorites".
	inline bool ToggleFavorites(State& state, std::span<const Item> items) {
		return AllFavorite(state, items) ? RemoveFavorites(state, items) : AddFavorites(state, items);
	}

	[[nodiscard]] inline Tab* FindTab(State& state, std::string_view id) {
		const auto it = std::ranges::find_if(state.tabs, [&](const Tab& tab) { return tab.id == id; });
		return it == state.tabs.end() ? nullptr : &*it;
	}

	[[nodiscard]] inline const Tab* FindTab(const State& state, std::string_view id) {
		const auto it = std::ranges::find_if(state.tabs, [&](const Tab& tab) { return tab.id == id; });
		return it == state.tabs.end() ? nullptr : &*it;
	}

	// Trimmed tab name, or nullopt when nothing but whitespace is left.
	[[nodiscard]] inline std::optional<std::string> NormalizeTabName(std::string_view name) {
		constexpr std::string_view kWhitespace = " \t\r\n";
		const size_t first = name.find_first_not_of(kWhitespace);
		if (first == std::string_view::npos) {
			return std::nullopt;
		}
		const size_t last = name.find_last_not_of(kWhitespace);
		return std::string(name.substr(first, last - first + 1));
	}

	// Adds a tab and makes it active. Returns false for an empty name or a duplicate id.
	inline bool CreateTab(State& state, std::string id, std::string_view name) {
		const auto normalized = NormalizeTabName(name);
		if (!normalized || id.empty() || FindTab(state, id)) {
			return false;
		}
		state.tabs.push_back(Tab { .id = id, .name = *normalized, .items = {} });
		state.active = std::move(id);
		return true;
	}

	inline bool RenameTab(State& state, std::string_view id, std::string_view name) {
		Tab* tab = FindTab(state, id);
		const auto normalized = NormalizeTabName(name);
		if (!tab || !normalized || tab->name == *normalized) {
			return false;
		}
		tab->name = *normalized;
		return true;
	}

	inline bool DeleteTab(State& state, std::string_view id) {
		if (std::erase_if(state.tabs, [&](const Tab& tab) { return tab.id == id; }) == 0) {
			return false;
		}
		if (state.active == id) {
			state.active.clear();
		}
		return true;
	}

	// Moves a tab one step left (direction < 0) or right (direction > 0) within bounds.
	inline bool MoveTab(State& state, std::string_view id, int direction) {
		const auto it = std::ranges::find_if(state.tabs, [&](const Tab& tab) { return tab.id == id; });
		if (it == state.tabs.end() || direction == 0) {
			return false;
		}
		const auto index = static_cast<ptrdiff_t>(it - state.tabs.begin());
		const ptrdiff_t next = index + (direction < 0 ? -1 : 1);
		if (next < 0 || next >= static_cast<ptrdiff_t>(state.tabs.size())) {
			return false;
		}
		std::swap(state.tabs[static_cast<size_t>(index)], state.tabs[static_cast<size_t>(next)]);
		return true;
	}

	// The active user tab, or nullptr when Recent is active (no or a stale active id).
	[[nodiscard]] inline const Tab* ActiveTab(const State& state) {
		return state.active.empty() ? nullptr : FindTab(state, state.active);
	}

} // namespace QuickTilesets

#endif
