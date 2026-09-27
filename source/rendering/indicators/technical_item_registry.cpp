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

void TechnicalItemRegistry::Initialize(const toml::table& config_table) {
	TechnicalItemLists lists;

	const auto* tech_section = config_table.get_as<toml::table>("technical_items");
	if (tech_section) {
		auto read_ids = [&](std::string_view key, std::vector<uint16_t>& dest) {
			const auto* arr = tech_section->get_as<toml::array>(key);
			if (arr) {
				dest.reserve(arr->size());
				for (const auto& elem : *arr) {
					if (auto opt = elem.value<int64_t>()) {
						if (*opt >= 0 && *opt <= 65535) {
							dest.push_back(static_cast<uint16_t>(*opt));
						}
					}
				}
			}
		};

		read_ids("invisible_stairs", lists.invisible_stairs);
		read_ids("invisible_walkable", lists.invisible_walkable);
		read_ids("invisible_walls", lists.invisible_walls);
		read_ids("primal_lights", lists.primal_lights);
	}

	SetLists(std::move(lists));
}

void TechnicalItemRegistry::SetLists(TechnicalItemLists lists) {
	auto sort_and_dedup = [](std::vector<uint16_t>& vec) {
		std::ranges::sort(vec);
		auto [first, last] = std::ranges::unique(vec);
		vec.erase(first, last);
	};

	sort_and_dedup(lists.invisible_stairs);
	sort_and_dedup(lists.invisible_walkable);
	sort_and_dedup(lists.invisible_walls);
	sort_and_dedup(lists.primal_lights);

	s_technical_lists = std::move(lists);
}

const TechnicalItemLists& TechnicalItemRegistry::GetLists() noexcept {
	return s_technical_lists;
}

TileIndicatorType TechnicalItemRegistry::Classify(uint16_t server_id, uint16_t client_id) noexcept {
	auto contains = [](uint16_t sid, uint16_t cid, const std::vector<uint16_t>& list) noexcept -> bool {
		if (list.empty()) {
			return false;
		}
		return (sid != 0 && std::ranges::binary_search(list, sid)) ||
		       (cid != 0 && std::ranges::binary_search(list, cid));
	};

	if (contains(server_id, client_id, s_technical_lists.invisible_stairs)) {
		return TileIndicatorType::TechInvisibleStair;
	}
	if (contains(server_id, client_id, s_technical_lists.invisible_walkable)) {
		return TileIndicatorType::TechInvisibleWalkable;
	}
	if (contains(server_id, client_id, s_technical_lists.invisible_walls)) {
		return TileIndicatorType::TechInvisibleWall;
	}
	if (contains(server_id, client_id, s_technical_lists.primal_lights)) {
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
	if (const auto* raw = dynamic_cast<const RAWBrush*>(brush)) {
		uint16_t s_id = raw->getItemID();
		uint16_t c_id = static_cast<uint16_t>(raw->getLookID());
		auto tech = Classify(s_id, c_id);
		if (tech != TileIndicatorType::None) {
			return tech;
		}
	}
	int look_id = brush->getLookID();
	if (look_id > 0) {
		auto tech = Classify(static_cast<uint16_t>(look_id), static_cast<uint16_t>(look_id));
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
