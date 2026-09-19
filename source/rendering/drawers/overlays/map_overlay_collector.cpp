//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/drawers/overlays/map_overlay_collector.h"
#include "rendering/ui/tooltip_drawer.h"
#include "rendering/drawers/overlays/door_indicator_drawer.h"
#include "rendering/drawers/overlays/hook_indicator_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "editor/editor.h"
#include "item_definitions/core/item_definition_store.h"
#include "brushes/brush_enums.h"
#include <algorithm>
#include <string_view>

namespace {
	bool FillItemTooltipData(TooltipData& data, Item* item, const Position& pos, bool isHouseTile, float zoom) {
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

		const bool is_complex = item->isComplex();
		const Door* door = isHouseTile ? item->asDoor() : nullptr;
		const Teleport* tp = item->asTeleport();
		const Container* container = item->asContainer();

		if (!is_complex && !door && !tp && !container) {
			return false;
		}

		if (is_complex) {
			unique = item->getUniqueID();
			action = item->getActionID();
			text = item->getText();
			description = item->getDescription();
		}

		if (door && door->isRealDoor()) {
			doorId = door->getDoorID();
		}

		if (tp && tp->hasDestination()) {
			destination = tp->getDestination();
		}

		if (container) {
			hasContent = container->getItemCount() > 0;
		}

		if (unique == 0 && action == 0 && doorId == 0 && text.empty() && description.empty() && destination.x == 0 && !hasContent) {
			return false;
		}

		const ItemDefinitionView it = item->getDefinition();
		std::string_view itemName = it ? it.name() : std::string_view{};
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

	const bool need_tooltips = options.show_tooltips && out_tooltip_drawer != nullptr;
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
			if (need_tooltips && tile->ground && tile->ground->isComplex()) {
				TooltipData& groundData = out_tooltip_drawer->requestTooltipData();
				if (FillItemTooltipData(groundData, tile->ground.get(), pos, tile->isHouseTile(), view.zoom)) {
					if (groundData.hasVisibleFields()) {
						out_tooltip_drawer->commitTooltip();
					}
				}
			}

			// 3. Tile items (single pass over items for both door indicators and tooltips)
			if (!tile->items.empty() && (need_doors || need_tooltips)) {
				const bool is_house_tile = tile->isHouseTile();
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

					// Tooltip badges
					if (need_tooltips) {
						const bool is_complex = item->isComplex();
						const Door* door = is_house_tile ? item->asDoor() : nullptr;
						const Teleport* tp = item->asTeleport();
						const Container* container = item->asContainer();

						if (is_complex || door || tp || container) {
							TooltipData& itemData = out_tooltip_drawer->requestTooltipData();
							if (FillItemTooltipData(itemData, item.get(), pos, is_house_tile, view.zoom)) {
								if (itemData.hasVisibleFields()) {
									out_tooltip_drawer->commitTooltip();
								}
							}
						}
					}
				}
			}

			// 4. Waypoint tooltip
			if (need_tooltips && location->getWaypointCount() > 0 && editor != nullptr) {
				if (const Waypoint* waypoint = editor->map.waypoints.getWaypoint(location)) {
					out_tooltip_drawer->addWaypointTooltip(pos, waypoint->name);
				}
			}
		}
	});
}
