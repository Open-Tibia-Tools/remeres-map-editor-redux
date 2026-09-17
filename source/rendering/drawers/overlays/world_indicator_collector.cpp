//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/drawers/overlays/world_indicator_collector.h"
#include "rendering/drawers/overlays/map_overlay_collector.h"
#include "rendering/core/render_view.h"

void WorldIndicatorCollector::Collect(
	const Map& map,
	const RenderView& view,
	const DrawingOptions& options,
	DoorIndicatorDrawer* out_door_drawer,
	HookIndicatorDrawer* out_hook_drawer
) {
	const int map_z = view.floor;
	const ViewBounds bounds = view.getBoundsForFloor(map_z);
	MapOverlayCollector::Collect(map, view, bounds, options, nullptr, nullptr, out_door_drawer, out_hook_drawer);
}
