#include "rendering/indicators/technical_item_registry.h"
#include "brushes/brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/house/house_exit_brush.h"

#include <toml++/toml.h>
#include <algorithm>
#include <ranges>

namespace rme::rendering {

namespace {
	TechnicalItemLists s_technical_lists;
}

bool TechnicalIdFilter::matches(uint16_t sid, uint16_t cid) const noexcept {
	if (sid != 0 && !server_ids.empty() && std::ranges::binary_search(server_ids, sid)) {
		return true;
	}
	if (cid != 0 && !client_ids.empty() && std::ranges::binary_search(client_ids, cid)) {
		return true;
	}
	return false;
}

void TechnicalIdFilter::sort_and_dedup() {
	auto clean = [](std::vector<uint16_t>& vec) {
		std::ranges::sort(vec);
		auto [first, last] = std::ranges::unique(vec);
		vec.erase(first, last);
	};
	clean(server_ids);
	clean(client_ids);
}

void TechnicalItemRegistry::Initialize(const toml::table& config_table) {
	TechnicalItemLists lists;

	const auto* tech_section = config_table.get_as<toml::table>("technical_items");
	if (tech_section) {
		auto read_id_array = [](const toml::table& tbl, std::string_view key, std::vector<uint16_t>& dest) {
			const auto* arr = tbl.get_as<toml::array>(key);
			if (arr) {
				dest.reserve(dest.size() + arr->size());
				for (const auto& elem : *arr) {
					if (auto opt = elem.value<int64_t>()) {
						if (*opt >= 0 && *opt <= 65535) {
							dest.push_back(static_cast<uint16_t>(*opt));
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

TileIndicatorType TechnicalItemRegistry::Classify(uint16_t server_id, uint16_t client_id) noexcept {
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
	if (brush->is<RAWBrush>()) {
		const auto* raw = brush->as<RAWBrush>();
		uint16_t s_id = raw->getItemID();
		uint16_t c_id = static_cast<uint16_t>(raw->getLookID());
		auto tech = Classify(s_id, c_id);
		if (tech != TileIndicatorType::None) {
			return tech;
		}
	}
	int look_id = brush->getLookID();
	if (look_id > 0) {
		auto tech = Classify(0, static_cast<uint16_t>(look_id));
		if (tech != TileIndicatorType::None) {
			return tech;
		}
	}
	const std::string& bname = brush->getName();
	if (bname == "stairs" || bname == "invisible stairs" || bname == "stair") {
		return TileIndicatorType::TechInvisibleStair;
	}
	return TileIndicatorType::None;
}

} // namespace rme::rendering
