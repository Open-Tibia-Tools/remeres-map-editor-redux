#include "rendering/drawers/overlays/world_indicator_collector.h"
#include "rendering/drawers/overlays/door_indicator_drawer.h"
#include "rendering/drawers/overlays/hook_indicator_drawer.h"
#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/item.h"
#include "brushes/brush_enums.h"

void WorldIndicatorCollector::Collect(
	const Map& map,
	const RenderView& view,
	const DrawingOptions& options,
	DoorIndicatorDrawer* out_door_drawer,
	HookIndicatorDrawer* out_hook_drawer
) {
	if (options.ingame) {
		return;
	}

	const bool can_read_labels = (32.0f / view.zoom) >= 10.0f;
	if (!can_read_labels) {
		return;
	}

	const bool need_doors = options.highlight_locked_doors && out_door_drawer != nullptr;
	const bool need_hooks = options.show_hooks && out_hook_drawer != nullptr;

	if (!need_doors && !need_hooks) {
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

			// Hook indicators (fast O(1) bitflag lookup)
			if (need_hooks && (tile->hasHookSouth() || tile->hasHookEast())) {
				out_hook_drawer->addHook(pos, tile->hasHookSouth(), tile->hasHookEast());
			}

			// Door indicators
			if (need_doors && !tile->items.empty()) {
				for (const auto& item : tile->items) {
					if (!item) {
						continue;
					}
					const ItemDefinitionView it = item->getDefinition();
					if (!it || !it.isDoor()) {
						continue;
					}

					const bool locked = item->isLocked();
					const auto border = static_cast<BorderType>(it.attribute(ItemAttributeKey::BorderAlignment));
					const bool south = (border == WALL_HORIZONTAL);
					const bool east = (border == WALL_VERTICAL);
					out_door_drawer->addDoor(pos, locked, south, east);
				}
			}
		}
	});
}
