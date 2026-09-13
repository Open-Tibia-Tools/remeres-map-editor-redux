#ifndef RME_RENDERING_CORE_RENDER_FRAME_CONTEXT_H_
#define RME_RENDERING_CORE_RENDER_FRAME_CONTEXT_H_

#include <cstdint>

class AtlasManager;
class GraphicManager;
class ItemDefinitionStore;
struct DrawingOptions;
struct RenderView;

struct RenderFrameContext {
	AtlasManager& atlas;
	GraphicManager& gfx;
	const ItemDefinitionStore& item_definitions;
	const DrawingOptions& options;
	const RenderView& view;
	long elapsed_time = 0;
	uint32_t current_house_id = 0;
};

#endif
