//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/ui/inspection_badge_collector.h"
#include "rendering/drawers/overlays/map_overlay_collector.h"
#include "rendering/core/render_view.h"

void InspectionBadgeCollector::Collect(
	const Map& map,
	const RenderView& view,
	const DrawingOptions& options,
	TooltipDrawer& out_tooltip_drawer,
	const Editor& editor
) {
	const int map_z = view.floor;
	const ViewBounds bounds = view.getBoundsForFloor(map_z);
	MapOverlayCollector::Collect(map, view, bounds, options, &editor, &out_tooltip_drawer, nullptr, nullptr);
}
