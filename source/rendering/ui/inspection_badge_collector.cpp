//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/ui/inspection_badge_collector.h"
#include "rendering/ui/tooltip_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "editor/editor.h"
#include <algorithm>
#include <string_view>

namespace {
	bool FillItemTooltipData(TooltipData& data, Item* item, const ItemDefinitionView& it, const Position& pos, bool isHouseTile, float zoom) {
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
		Position destination;
		bool hasContent = false;

		bool is_complex = item->isComplex();
		if (!is_complex && !it.isTooltipable()) {
			return false;
		}

		bool is_container = it.isContainer();
		bool is_door = isHouseTile && item->isDoor();
		bool is_teleport = item->isTeleport();

		if (is_complex) {
			unique = item->getUniqueID();
			action = item->getActionID();
			text = item->getText();
			description = item->getDescription();
		}

		if (is_door) {
			if (const Door* door = item->asDoor()) {
				if (door->isRealDoor()) {
					doorId = door->getDoorID();
				}
			}
		}

		if (is_teleport) {
			Teleport* tp = static_cast<Teleport*>(item);
			if (tp->hasDestination()) {
				destination = tp->getDestination();
			}
		}

		if (is_container) {
			if (const Container* container = item->asContainer()) {
				hasContent = container->getItemCount() > 0;
			}
		}

		if (unique == 0 && action == 0 && doorId == 0 && text.empty() && description.empty() && destination.x == 0 && !hasContent) {
			return false;
		}

		std::string_view itemName = it.name();
		if (itemName.empty()) {
			itemName = "Item";
		}

		data.pos = pos;
		data.itemId = id;
		data.itemName = itemName;
		data.actionId = action;
		data.uniqueId = unique;
		data.doorId = doorId;
		data.text = text;
		data.description = description;
		data.destination = destination;

		if (it.isContainer() && zoom <= 1.5f) {
			if (const Container* container = item->asContainer()) {
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
		}

		data.updateCategory();
		return true;
	}
}

void InspectionBadgeCollector::Collect(
	const Map& map,
	const RenderView& view,
	const DrawingOptions& options,
	TooltipDrawer& out_tooltip_drawer,
	const Editor& editor
) {
	if (!options.show_tooltips) {
		return;
	}

	// Skip collecting inspection badges when zoomed beyond 10% zoom (matching editor LOD policy)
	if (view.zoom > 10.0f) {
		return;
	}

	const int map_z = view.floor;
	const ViewBounds bounds = view.getBoundsForFloor(map_z);

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
			const bool is_house_tile = tile->isHouseTile();

			// Ground tooltip
			if (tile->ground) {
				const ItemDefinitionView ground_it = tile->ground->getDefinition();
				if (ground_it) {
					TooltipData& groundData = out_tooltip_drawer.requestTooltipData();
					if (FillItemTooltipData(groundData, tile->ground.get(), ground_it, pos, is_house_tile, view.zoom)) {
						if (groundData.hasVisibleFields()) {
							out_tooltip_drawer.commitTooltip();
						}
					}
				}
			}

			// Items on tile
			for (const auto& item : tile->items) {
				if (!item) {
					continue;
				}
				const ItemDefinitionView it = item->getDefinition();
				if (!it) {
					continue;
				}

				TooltipData& itemData = out_tooltip_drawer.requestTooltipData();
				if (FillItemTooltipData(itemData, item.get(), it, pos, is_house_tile, view.zoom)) {
					if (itemData.hasVisibleFields()) {
						out_tooltip_drawer.commitTooltip();
					}
				}
			}

			// Waypoint tooltip
			if (location->getWaypointCount() > 0) {
				if (const Waypoint* waypoint = editor.map.waypoints.getWaypoint(location)) {
					out_tooltip_drawer.addWaypointTooltip(pos, waypoint->name);
				}
			}
		}
	});
}
