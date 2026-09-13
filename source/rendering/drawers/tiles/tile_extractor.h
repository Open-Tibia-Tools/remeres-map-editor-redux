#ifndef RME_RENDERING_TILE_EXTRACTOR_H_
#define RME_RENDERING_TILE_EXTRACTOR_H_

#include "rendering/core/render_chunk.h"
#include <cstdint>

class BaseMap;
class TileRenderer;
struct RenderFrameContext;

namespace rme::rendering {

class TileExtractor {
public:
	static void ExtractChunk(
		RenderChunk& chunk,
		BaseMap& map,
		const RenderFrameContext& ctx,
		const TileRenderer& tile_renderer
	);
};

} // namespace rme::rendering

#endif
