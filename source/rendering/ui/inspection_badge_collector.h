//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_UI_INSPECTION_BADGE_COLLECTOR_H_
#define RME_RENDERING_UI_INSPECTION_BADGE_COLLECTOR_H_

class Map;
class TooltipDrawer;
class Editor;
struct RenderView;
struct DrawingOptions;

class InspectionBadgeCollector {
public:
	static void Collect(
		const Map& map,
		const RenderView& view,
		const DrawingOptions& options,
		TooltipDrawer& out_tooltip_drawer,
		const Editor& editor
	);
};

#endif
