//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/drawers/overlays/map_overlay_collector.h"
#include "rendering/ui/tooltip_drawer.h"
#include "rendering/drawers/overlays/door_indicator_drawer.h"
#include "rendering/drawers/overlays/hook_indicator_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "rendering/indicators/zone_colors.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "game/srv_item_attributes.h"
#include "game/srv_markers.h"
#include "game/house.h"
#include "game/town.h"
#include "editor/editor.h"
#include "item_definitions/core/item_definition_store.h"
#include "brushes/brush_enums.h"
#include <algorithm>
#include <string_view>

namespace {
	bool FillItemTooltipData(TooltipData& data, Item* item, const Position& pos, uint32_t houseId, float zoom, const Editor* editor) {
		if (!item) {
			return false;
		}

		const uint16_t id = item->getID();
		if (id < 100) {
			return false;
		}

		uint16_t unique = 0;
		uint16_t action = 0;
		std::string_view text;
		std::string_view description;
		uint8_t doorId = 0;
		uint8_t depotId = 0;
		Position destination;
		bool hasContent = false;
		uint16_t count = 0;
		std::array<int32_t, rme::srv::kItemAttributes.size()> srv_attributes {};
		bool hasSrv = false;

		const bool is_complex = item->isComplex();
		const bool is_house_tile = houseId > 0;
		const Door* door = (is_house_tile && item->asDoor() && item->asDoor()->isRealDoor()) ? item->asDoor() : nullptr;
		const Teleport* tp = item->asTeleport();
		const Container* container = item->asContainer();
		const Depot* depot = item->asDepot();

		if (!is_complex && !door && !tp && !container && !depot) {
			return false;
		}

		if (is_complex) {
			unique = item->getUniqueID();
			action = item->getActionID();
			text = item->getText();
			description = item->getDescription();
			for (size_t i = 0; i < srv_attributes.size(); ++i) {
				if (const int32_t* value = item->getIntegerAttribute(rme::srv::key(static_cast<rme::srv::ItemAttribute>(i)))) {
					srv_attributes[i] = *value;
					hasSrv = hasSrv || *value != 0;
				}
			}
		}

		if (door) {
			doorId = door->getDoorID();
		}

		if (depot) {
			depotId = depot->getDepotID();
		}

		if (tp && tp->hasDestination()) {
			destination = tp->getDestination();
		}

		if (container) {
			hasContent = container->getItemCount() > 0;
		}

		if (item->isStackable() || item->getCount() > 1) {
			count = static_cast<uint16_t>(item->getCount());
		}

		if (unique == 0 && action == 0 && doorId == 0 && depotId == 0 && text.empty() &&
		    description.empty() && destination.x == 0 && !hasContent && count <= 1 && !hasSrv) {
			return false;
		}

		const ItemDefinitionView it = item->getDefinition();
		std::string_view itemName = it ? it.name() : std::string_view{};
		if (itemName.empty()) {
			itemName = "Item";
		}

		data.pos = pos;
		data.itemId = id;
		data.clientId = static_cast<uint16_t>(item->getClientID());
		data.itemName = itemName;
		data.count = count;
		data.actionId = action;
		data.uniqueId = unique;
		data.doorId = doorId;
		data.depotId = depotId;
		data.text = text;
		data.description = description;
		data.destination = destination;
		data.srvAttributes = srv_attributes;

		if (door && editor != nullptr) {
			if (const House* house = editor->map.houses.getHouse(houseId)) {
				data.houseId = house->getID();
				data.houseRent = house->rent;
				data.houseSize = house->size();
				data.isGuildhall = house->guildhall;
				if (!house->name.empty()) {
					data.subtitle = house->name;
				}
				if (house->townid > 0) {
					auto townIt = editor->map.towns.find(house->townid);
					if (townIt != editor->map.towns.end() && townIt->second) {
						data.townName = townIt->second->getName();
					}
				}
			}
		}

		if (depotId > 0 && editor != nullptr) {
			auto it = editor->map.towns.find(depotId);
			if (it != editor->map.towns.end() && it->second) {
				data.townName = it->second->getName();
			}
		}

		if (container && zoom <= 1.5f) {
			data.containerCapacity = static_cast<uint8_t>(container->getVolume());
			const auto& items = container->getVector();
			data.containerItems.clear();
			data.containerItems.reserve(std::min(items.size(), size_t(32)));
			for (const auto& subItem : items) {
				if (subItem) {
					ContainerItem ci;
					ci.id = subItem->getID();
					ci.subtype = subItem->getSubtype();
					ci.count = subItem->getCount();
					if (ci.count == 0) {
						ci.count = 1;
					}
					data.containerItems.push_back(ci);
					if (data.containerItems.size() >= 32) {
						break;
					}
				}
			}
		}

		data.updateCategory();
		return true;
	}
}

void MapOverlayCollector::Collect(
	const Map& map,
	const RenderView& view,
	const ViewBounds& bounds,
	const DrawingOptions& options,
	const Editor* editor,
	TooltipDrawer* out_tooltip_drawer,
	DoorIndicatorDrawer* out_door_drawer,
	HookIndicatorDrawer* out_hook_drawer
) {
	// Zoom LOD guard: do not collect overlays when zoomed beyond 10% zoom (matching editor LOD policy)
	const bool can_read_labels = view.zoom <= 10.0f;
	if (!can_read_labels) {
		return;
	}

	const bool need_tooltips = out_tooltip_drawer != nullptr;
	const bool need_doors = !options.ingame && options.highlight_locked_doors && out_door_drawer != nullptr;
	const bool need_hooks = !options.ingame && options.show_hooks && out_hook_drawer != nullptr;

	if (!need_tooltips && !need_doors && !need_hooks) {
		return;
	}

	const int map_z = view.floor;

	map.visitLeaves(bounds.start_x, bounds.start_y, bounds.end_x + 1, bounds.end_y + 1, [&](const MapNode* nd, int, int) {
		const Floor* floor = nd->getFloor(map_z);
		if (!floor) {
			return;
		}

		for (int idx = 0; idx < SpatialHashGrid::TILES_PER_NODE; ++idx) {
			const TileLocation* location = &floor->locs[idx];
			const Tile* tile = location->get();
			if (!tile) {
				continue;
			}

			const Position& pos = location->getPosition();

			// 1. Hook indicators (fast O(1) bitflag lookup)
			if (need_hooks && (tile->hasHookSouth() || tile->hasHookEast())) {
				out_hook_drawer->addHook(pos, tile->hasHookSouth(), tile->hasHookEast());
			}

			// 2. Ground tooltip (only complex grounds can have attributes like aid/uid/text)
			const uint32_t tile_house_id = tile->getHouseID();
			if (need_tooltips && options.show_tooltips && tile->ground && tile->ground->isComplex()) {
				TooltipData& groundData = out_tooltip_drawer->requestTooltipData();
				if (FillItemTooltipData(groundData, tile->ground.get(), pos, tile_house_id, view.zoom, editor)) {
					if (groundData.hasVisibleFields()) {
						out_tooltip_drawer->commitTooltip();
					}
				}
			}

			// 3. Tile items (single pass over items for both door indicators and tooltips)
			if (!tile->items.empty() && (need_doors || (need_tooltips && options.show_tooltips))) {
				for (const auto& item : tile->items) {
					if (!item) {
						continue;
					}

					// Door indicators
					if (need_doors) {
						if (const Door* door = item->asDoor()) {
							const ItemDefinitionView it = item->getDefinition();
							if (it && it.isDoor()) {
								const bool locked = item->isLocked();
								const auto border = static_cast<BorderType>(it.attribute(ItemAttributeKey::BorderAlignment));
								const bool south = (border == WALL_HORIZONTAL);
								const bool east = (border == WALL_VERTICAL);
								out_door_drawer->addDoor(pos, locked, south, east);
							}
						}
					}

					// Tooltip badges (only when show_tooltips is enabled)
					if (need_tooltips && options.show_tooltips) {
						const bool is_complex = item->isComplex();
						const Door* door = (tile_house_id > 0 && item->asDoor() && item->asDoor()->isRealDoor()) ? item->asDoor() : nullptr;
						const Teleport* tp = item->asTeleport();
						const Container* container = item->asContainer();
						const Depot* depot = item->asDepot();

						if (is_complex || door || tp || container || depot || item->isStackable()) {
							TooltipData& itemData = out_tooltip_drawer->requestTooltipData();
							if (FillItemTooltipData(itemData, item.get(), pos, tile_house_id, view.zoom, editor)) {
								if (itemData.hasVisibleFields()) {
									out_tooltip_drawer->commitTooltip();
								}
							}
						}
					}
				}
			}

			// 4. Town temple tooltip (triggered by options.show_towns)
			if (need_tooltips && options.show_towns && tile->isTownExit() && editor != nullptr) {
				for (const auto& [town_id, town_ptr] : editor->map.towns) {
					if (town_ptr && town_ptr->getTemplePosition() == pos) {
						TooltipData& tData = out_tooltip_drawer->requestTooltipData();
						tData.pos = pos;
						tData.category = TooltipCategory::TOWN;
						tData.townId = town_id;
						tData.itemName = town_ptr->getName();
						tData.subtitle = "Town Temple";
						tData.badgeType = rme::rendering::TileIndicatorType::TownTemple;
						out_tooltip_drawer->commitTooltip();
						break;
					}
				}
			}

			// 5. Waypoint tooltip (triggered by options.show_waypoints)
			if (need_tooltips && options.show_waypoints && location->getWaypointCount() > 0 && editor != nullptr) {
				if (const Waypoint* waypoint = editor->map.waypoints.getWaypoint(location)) {
					out_tooltip_drawer->addWaypointTooltip(pos, waypoint->name);
				}
			}
		}
	});

	// 6. Macro entity tooltips (Houses, Zones)
	if (need_tooltips && editor != nullptr) {
		// House zone centroid tooltips (triggered by options.show_houses, placed at room centroid on active floor)
		if (options.show_houses) {
			for (const auto& [house_id, house_ptr] : editor->map.houses) {
				if (!house_ptr) {
					continue;
				}
				const House::FloorSummary& summary = house_ptr->getFloorSummary(map_z);
				if (summary.count == 0) {
					continue;
				}
				if (summary.max_x < bounds.start_x || summary.min_x > bounds.end_x ||
				    summary.max_y < bounds.start_y || summary.min_y > bounds.end_y) {
					continue;
				}

				const Position& best_pos = summary.best_tile;
				if (best_pos.x < bounds.start_x || best_pos.x > bounds.end_x ||
				    best_pos.y < bounds.start_y || best_pos.y > bounds.end_y) {
					continue;
				}

				TooltipData& hData = out_tooltip_drawer->requestTooltipData();
				hData.pos = best_pos;
				hData.category = TooltipCategory::HOUSE;
				hData.houseId = house_ptr->getID();

				std::string cleanName;
				cleanName.reserve(house_ptr->name.size());
				for (char c : house_ptr->name) {
					if (static_cast<unsigned char>(c) >= 32) {
						cleanName.push_back(c);
					}
				}
				hData.itemName = cleanName.empty() ? "House" : cleanName;
				hData.subtitle = house_ptr->guildhall ? "Guildhall" : "House";
				hData.houseRent = house_ptr->rent;
				hData.houseSize = house_ptr->size();
				hData.isGuildhall = house_ptr->guildhall;
				hData.badgeType = house_ptr->guildhall ? rme::rendering::TileIndicatorType::GuildHall : rme::rendering::TileIndicatorType::House;
				if (house_ptr->townid > 0) {
					auto townIt = editor->map.towns.find(house_ptr->townid);
					if (townIt != editor->map.towns.end() && townIt->second) {
						hData.townName = townIt->second->getName();
					}
				}
				out_tooltip_drawer->commitTooltip();
			}
		}

		// Zone cluster tooltips (triggered by options.show_zones)
		if (options.show_zones && (map.getVersion().otbm == MAP_OTBM_5)) {
			for (const auto& zone : map.zones) {
				if (zone.id == 0) {
					continue;
				}
				const auto& clusters = editor->map.getZoneClusters(zone.id, !options.is_drawing);
				for (size_t ci = 0; ci < clusters.size(); ++ci) {
					const auto& cluster = clusters[ci];
					if (cluster.centroid_pos.z != map_z) {
						continue;
					}
					if (cluster.centroid_pos.x < bounds.start_x || cluster.centroid_pos.x > bounds.end_x ||
					    cluster.centroid_pos.y < bounds.start_y || cluster.centroid_pos.y > bounds.end_y) {
						continue;
					}
					TooltipData& zData = out_tooltip_drawer->requestTooltipData();
					zData.pos = cluster.centroid_pos;
					zData.category = TooltipCategory::ZONE;
					zData.zoneId = zone.id;
					zData.itemName = !zone.name.empty() ? zone.name : std::format("Zone #{}", zone.id);
					zData.subtitle = (clusters.size() > 1)
						? std::format("Area #{} of {}", ci + 1, clusters.size())
						: std::format("Area #{}", ci + 1);
					zData.zoneRgb = rme::rendering::getZoneColor(zone.id);
					zData.zoneTileCount = editor->map.getZoneTilePositions(zone.id).size();
					zData.zoneAreaCount = clusters.size();
					zData.zoneAreaIndex = static_cast<uint32_t>(ci);
					zData.zoneAreaTileCount = cluster.tile_count;
					out_tooltip_drawer->commitTooltip();
				}
			}
		}
	}

	// 7. Server markers of sector maps (triggered by options.show_server_markers); they need no tile
	if (need_tooltips && !options.ingame && options.show_server_markers && map.server_markers) {
		for (const rme::srv::Marker& marker : map.server_markers->onFloor(map_z)) {
			const Position& pos = marker.position;
			if (pos.x >= bounds.start_x && pos.x <= bounds.end_x && pos.y >= bounds.start_y && pos.y <= bounds.end_y) {
				out_tooltip_drawer->addServerMarkerTooltip(marker);
			}
		}
	}
}
