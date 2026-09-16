//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_LIGHT_GATHERER_H_
#define RME_RENDERING_CORE_LIGHT_GATHERER_H_

#include "rendering/core/light_buffer.h"

class Map;
struct RenderView;
struct DrawingOptions;

class LightGatherer {
public:
	static void Gather(
		const Map& map,
		const RenderView& view,
		const DrawingOptions& options,
		LightBuffer& light_buffer
	);
};

#endif
