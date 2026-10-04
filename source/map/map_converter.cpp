#include "map/map_converter.h"
#include "map/map.h"
#include "map/tile_operations.h"
#include "ui/gui.h"
#include "item_definitions/core/item_definition_store.h"
#include "game/item.h"
#include <unordered_map>
#include <unordered_set>
#include <ranges>
#include <algorithm>

bool MapConverter::convert(Map& map, MapVersion to, bool showdialog) {
	if (map.mapVersion.client == to.client && map.mapVersion.items_major == to.items_major) {
		// Only OTBM version differs
		// No changes necessary
		map.mapVersion = to;
		return true;
	}

	/* TODO
	if(to.otbm == MAP_OTBM_4 && to.client < CLIENT_VERSION_850)
		return false;
	*/

	map.mapVersion = to;

	return true;
}

namespace {
	size_t applyMTMConversion(Tile* tile, const ConversionMap& rm, const std::unordered_map<const std::vector<uint16_t>*, std::unordered_set<uint16_t>>& mtm_lookups, std::vector<uint16_t>& id_list) {
		id_list.clear();
		id_list.reserve(tile->items.size() + 1);

		if (tile->ground) {
			id_list.push_back(tile->ground->getID());
		}

		auto border_view = tile->items | std::views::filter([](const auto& i) { return i->isBorder(); });
		auto border_ids = border_view | std::views::transform([](const auto& i) { return i->getID(); });
		id_list.insert(id_list.end(), border_ids.begin(), border_ids.end());

		std::ranges::sort(id_list);

		ConversionMap::MTM::const_iterator cfmtm = rm.mtm.end();

		while (!id_list.empty()) {
			cfmtm = rm.mtm.find(id_list);
			if (cfmtm != rm.mtm.end()) {
				break;
			}
			id_list.pop_back();
		}

		size_t inserted_items = 0;

		if (cfmtm != rm.mtm.end()) {
			const std::vector<uint16_t>& v = cfmtm->first;
			const auto& ids_to_remove = mtm_lookups.at(&v);

			if (tile->ground && ids_to_remove.contains(tile->ground->getID())) {
				tile->ground.reset();
			}

			auto part_iter = std::stable_partition(tile->items.begin(), tile->items.end(), [&ids_to_remove](const std::unique_ptr<Item>& item) {
				return !ids_to_remove.contains(item->getID());
			});

			tile->items.erase(part_iter, tile->items.end());

			const std::vector<uint16_t>& new_items = cfmtm->second;
			for (uint16_t new_id : new_items) {
				std::unique_ptr<Item> item = Item::Create(new_id);
				if (item->isGroundTile()) {
					tile->ground = std::move(item);
				} else {
					tile->items.insert(tile->items.begin(), std::move(item));
					++inserted_items;
				}
			}
		}

		return inserted_items;
	}

	void applySTMConversion(Tile* tile, const ConversionMap& rm, size_t& inserted_items) {
		if (tile->ground) {
			ConversionMap::STM::const_iterator cfstm = rm.stm.find(tile->ground->getID());
			if (cfstm != rm.stm.end()) {
				uint16_t aid = tile->ground->getActionID();
				uint16_t uid = tile->ground->getUniqueID();
				tile->ground.reset();

				const std::vector<uint16_t>& v = cfstm->second;
				for (uint16_t new_id : v) {
					std::unique_ptr<Item> item = Item::Create(new_id);
					if (item->isGroundTile()) {
						item->setActionID(aid);
						item->setUniqueID(uid);
						tile->addItem(std::move(item));
					} else {
						tile->items.insert(tile->items.begin(), std::move(item));
						++inserted_items;
					}
				}
			}
		}

		for (auto replace_item_iter = tile->items.begin() + inserted_items; replace_item_iter != tile->items.end();) {
			uint16_t id = (*replace_item_iter)->getID();
			ConversionMap::STM::const_iterator cf = rm.stm.find(id);
			if (cf != rm.stm.end()) {
				replace_item_iter = tile->items.erase(replace_item_iter);
				const std::vector<uint16_t>& v = cf->second;
				for (uint16_t new_id : v) {
					replace_item_iter = tile->items.insert(replace_item_iter, Item::Create(new_id));
					++replace_item_iter;
				}
			} else {
				++replace_item_iter;
			}
		}
	}

} // namespace

bool MapConverter::convert(Map& map, const ConversionMap& rm, bool showdialog) {
	if (showdialog) {
		g_gui.CreateLoadBar("Converting map ...");
	}

	std::unordered_map<const std::vector<uint16_t>*, std::unordered_set<uint16_t>> mtm_lookups;
	for (const auto& entry : rm.mtm) {
		mtm_lookups.emplace(&entry.first, std::unordered_set<uint16_t>(entry.first.begin(), entry.first.end()));
	}

	uint64_t tiles_done = 0;
	std::vector<uint16_t> id_list;

	for (auto& tile_loc : map.tiles()) {
		Tile* tile = tile_loc.get();
		ASSERT(tile);

		if (tile->empty()) {
			continue;
		}

		size_t inserted_items = applyMTMConversion(tile, rm, mtm_lookups, id_list);
		applySTMConversion(tile, rm, inserted_items);

		++tiles_done;
		if (showdialog && tiles_done % 0x10000 == 0) {
			g_gui.SetLoadDone(static_cast<int>(tiles_done * 100.0 / static_cast<double>(map.getTileCount())));
		}
	}

	if (showdialog) {
		g_gui.DestroyLoadBar();
	}

	return true;
}

void MapConverter::cleanInvalidTiles(Map& map, bool showdialog) {
	if (showdialog) {
		g_gui.CreateLoadBar("Removing invalid tiles...");
	}

	uint64_t tiles_done = 0;

	for (auto& tile_loc : map.tiles()) {
		Tile* tile = tile_loc.get();
		ASSERT(tile);

		if (tile->empty()) {
			continue;
		}

		if (tile->ground && tile->ground->isInvalidOTBMItem()) {
			tile->ground.reset();
		}

		// Use std::erase_if from C++20 for cleanup
		std::erase_if(tile->items, [](const std::unique_ptr<Item>& item) {
			return item->isInvalidOTBMItem() || !g_item_definitions.typeExists(item->getID());
		});

		TileOperations::update(tile);

		++tiles_done;
		if (showdialog && tiles_done % 0x10000 == 0) {
			g_gui.SetLoadDone(static_cast<int>(tiles_done * 100.0 / static_cast<double>(map.getTileCount())));
		}
	}

	if (showdialog) {
		g_gui.DestroyLoadBar();
	}
}

void MapConverter::cleanInvalidZones(Map& map, bool showdialog) {
	if (showdialog) {
		g_gui.CreateLoadBar("Removing invalid zones...");
	}

	uint64_t tiles_done = 0;

	for (auto& tile_loc : map.tiles()) {
		Tile* tile = tile_loc.get();
		ASSERT(tile);

		if (!tile->hasInvalidZones()) {
			continue;
		}

		tile->clearInvalidZones();
		TileOperations::update(tile);

		++tiles_done;
		if (showdialog && tiles_done % 0x10000 == 0) {
			g_gui.SetLoadDone(static_cast<int>(tiles_done * 100.0 / static_cast<double>(map.getTileCount())));
		}
	}

	if (showdialog) {
		g_gui.DestroyLoadBar();
	}
}

void MapConverter::convertHouseTiles(Map& map, uint32_t fromId, uint32_t toId) {
	g_gui.CreateLoadBar("Converting house tiles...");
	uint64_t tiles_done = 0;

	auto filtered_tiles = map.tiles() | std::views::filter([fromId](const auto& tile_loc) {
							  const Tile* tile = tile_loc.get();
							  return fromId != 0 && tile && tile->getHouseID() == fromId;
						  });

	for (auto& tile_loc : filtered_tiles) {
		tile_loc.get()->setHouseID(toId);
		++tiles_done;
		if (tiles_done % 0x10000 == 0) {
			g_gui.SetLoadDone(static_cast<int>(tiles_done * 100.0 / static_cast<double>(map.getTileCount())));
		}
	}

	g_gui.DestroyLoadBar();
}
