#include "rendering/indicators/technical_item_registry.h"
#include "brushes/brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/house/house_exit_brush.h"
#include "brushes/house/house_brush.h"
#include "brushes/flag/flag_brush.h"
#include "map/tile.h"
#include "game/sprites.h"

#include <toml++/toml.h>
#include <algorithm>
#include <limits>
#include <ranges>

namespace rme::rendering {

TechnicalItemLists TechnicalItemRegistry::CreateDefaultTechnicalLists() {
	TechnicalItemLists lists;
	lists.invisible_stairs.server_ids = { 459 };
	lists.invisible_stairs.client_ids = { 469 };
	lists.invisible_walkable.server_ids = { 460 };
	lists.invisible_walkable.client_ids = { 470, 17970, 20028, 34168 };
	lists.invisible_walls.server_ids = { 1548 };
	lists.invisible_walls.client_ids = { 2187 };
	lists.primal_lights.client_ids = {
		39092, 39093, 39094, 39095, 39096, 39097, 39098, 39099, 39100,
		39236, 39367, 39368
	};
	lists.invisible_stairs.sort_and_dedup();
	lists.invisible_walkable.sort_and_dedup();
	lists.invisible_walls.sort_and_dedup();
	lists.primal_lights.sort_and_dedup();
	return lists;
}

namespace {
	TechnicalItemLists s_technical_lists = TechnicalItemRegistry::CreateDefaultTechnicalLists();
}

bool TechnicalIdFilter::matches(uint32_t sid, uint32_t cid) const noexcept {
	if (sid != 0 && !server_ids.empty() && std::ranges::binary_search(server_ids, sid)) {
		return true;
	}
	if (cid != 0 && !client_ids.empty() && std::ranges::binary_search(client_ids, cid)) {
		return true;
	}
	return false;
}

void TechnicalIdFilter::sort_and_dedup() {
	auto clean = [](std::vector<uint32_t>& vec) {
		std::ranges::sort(vec);
		auto [first, last] = std::ranges::unique(vec);
		vec.erase(first, last);
	};
	clean(server_ids);
	clean(client_ids);
}

void TechnicalItemRegistry::Initialize(const toml::table& config_table) {
	TechnicalItemLists lists = CreateDefaultTechnicalLists();

	const auto* tech_section = config_table.get_as<toml::table>("technical_items");
	if (tech_section) {
		auto read_id_array = [](const toml::table& tbl, std::string_view key, std::vector<uint32_t>& dest) {
			const auto* arr = tbl.get_as<toml::array>(key);
			if (arr) {
				dest.clear();
				dest.reserve(dest.size() + arr->size());
				for (const auto& elem : *arr) {
					if (auto opt = elem.value<int64_t>()) {
						if (*opt >= 0 && *opt <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
							dest.push_back(static_cast<uint32_t>(*opt));
						}
					}
				}
			}
		};

		auto read_filter = [&](std::string_view key, TechnicalIdFilter& filter) {
			// 1. Try sub-table: [technical_items.<key>]
			if (const auto* sub = tech_section->get_as<toml::table>(key)) {
				read_id_array(*sub, "server_ids", filter.server_ids);
				read_id_array(*sub, "client_ids", filter.client_ids);
			}
			// 2. Also check flat keys in [technical_items]: <key>_server_ids, <key>_client_ids
			read_id_array(*tech_section, std::string(key) + "_server_ids", filter.server_ids);
			read_id_array(*tech_section, std::string(key) + "_client_ids", filter.client_ids);
		};

		read_filter("invisible_stairs", lists.invisible_stairs);
		read_filter("invisible_walkable", lists.invisible_walkable);
		read_filter("invisible_walls", lists.invisible_walls);
		read_filter("primal_lights", lists.primal_lights);
	}

	SetLists(std::move(lists));
}

void TechnicalItemRegistry::SetLists(TechnicalItemLists lists) {
	lists.invisible_stairs.sort_and_dedup();
	lists.invisible_walkable.sort_and_dedup();
	lists.invisible_walls.sort_and_dedup();
	lists.primal_lights.sort_and_dedup();

	s_technical_lists = std::move(lists);
}

const TechnicalItemLists& TechnicalItemRegistry::GetLists() noexcept {
	return s_technical_lists;
}

TileIndicatorType TechnicalItemRegistry::Classify(uint32_t server_id, uint32_t client_id) noexcept {
	if (s_technical_lists.invisible_stairs.matches(server_id, client_id)) {
		return TileIndicatorType::TechInvisibleStair;
	}
	if (s_technical_lists.invisible_walkable.matches(server_id, client_id)) {
		return TileIndicatorType::TechInvisibleWalkable;
	}
	if (s_technical_lists.invisible_walls.matches(server_id, client_id)) {
		return TileIndicatorType::TechInvisibleWall;
	}
	if (s_technical_lists.primal_lights.matches(server_id, client_id)) {
		return TileIndicatorType::TechPrimalLight;
	}

	return TileIndicatorType::None;
}

TileIndicatorType TechnicalItemRegistry::GetBrushIndicatorType(const Brush* brush) {
	if (!brush) {
		return TileIndicatorType::None;
	}
	if (brush->is<SpawnBrush>()) {
		return TileIndicatorType::Spawn;
	}
	if (brush->is<WaypointBrush>()) {
		return TileIndicatorType::Waypoint;
	}
	if (brush->is<HouseExitBrush>()) {
		return TileIndicatorType::HouseEntry;
	}
	if (brush->is<HouseBrush>()) {
		return TileIndicatorType::House;
	}
	if (brush->is<FlagBrush>()) {
		const auto* fb = brush->as<FlagBrush>();
		switch (fb->getFlag()) {
			case TILESTATE_PROTECTIONZONE: return TileIndicatorType::ZonePZ;
			case TILESTATE_NOPVP:          return TileIndicatorType::ZoneNoPvP;
			case TILESTATE_NOLOGOUT:       return TileIndicatorType::ZoneNoLogout;
			case TILESTATE_PVPZONE:        return TileIndicatorType::ZonePvP;
			default: break;
		}
	}
	const int look_id = brush->getLookID();
	if (look_id == EDITOR_SPRITE_PZ_TOOL)    return TileIndicatorType::ZonePZ;
	if (look_id == EDITOR_SPRITE_NOPVP_TOOL) return TileIndicatorType::ZoneNoPvP;
	if (look_id == EDITOR_SPRITE_NOLOG_TOOL) return TileIndicatorType::ZoneNoLogout;
	if (look_id == EDITOR_SPRITE_PVPZ_TOOL)  return TileIndicatorType::ZonePvP;

	if (brush->is<RAWBrush>()) {
		const auto* raw = brush->as<RAWBrush>();
		const uint32_t s_id = static_cast<uint32_t>(raw->getItemID());
		const uint32_t c_id = raw->getLookID() > 0 ? static_cast<uint32_t>(raw->getLookID()) : 0;
		auto tech = Classify(s_id, c_id);
		if (tech != TileIndicatorType::None) {
			return tech;
		}
	}
	if (look_id > 0) {
		auto tech = Classify(0, static_cast<uint32_t>(look_id));
		if (tech != TileIndicatorType::None) {
			return tech;
		}
	}
	return TileIndicatorType::None;
}

} // namespace rme::rendering
