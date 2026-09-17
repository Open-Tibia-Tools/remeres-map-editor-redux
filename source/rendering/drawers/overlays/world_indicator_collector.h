#ifndef RME_WORLD_INDICATOR_COLLECTOR_H_
#define RME_WORLD_INDICATOR_COLLECTOR_H_

class Map;
struct RenderView;
struct DrawingOptions;
class DoorIndicatorDrawer;
class HookIndicatorDrawer;

class WorldIndicatorCollector {
public:
	static void Collect(
		const Map& map,
		const RenderView& view,
		const DrawingOptions& options,
		DoorIndicatorDrawer* out_door_drawer,
		HookIndicatorDrawer* out_hook_drawer
	);
};

#endif
