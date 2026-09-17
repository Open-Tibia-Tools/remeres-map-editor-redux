//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_DRAWERS_OVERLAYS_MAP_OVERLAY_COLLECTOR_H_
#define RME_RENDERING_DRAWERS_OVERLAYS_MAP_OVERLAY_COLLECTOR_H_

class Map;
class Editor;
struct RenderView;
struct DrawingOptions;
struct ViewBounds;
class TooltipDrawer;
class DoorIndicatorDrawer;
class HookIndicatorDrawer;

class MapOverlayCollector {
public:
	static void Collect(
		const Map& map,
		const RenderView& view,
		const ViewBounds& bounds,
		const DrawingOptions& options,
		const Editor* editor,
		TooltipDrawer* out_tooltip_drawer,
		DoorIndicatorDrawer* out_door_drawer,
		HookIndicatorDrawer* out_hook_drawer
	);
};

#endif
