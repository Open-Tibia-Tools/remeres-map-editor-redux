//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_RENDERING_CORE_LIGHT_GATHERER_H_
#define RME_RENDERING_CORE_LIGHT_GATHERER_H_

#include "rendering/core/light_types.h"
#include "rendering/core/light_buffer.h"
#include <vector>

class BaseMap;
class GraphicManager;
struct RenderView;
struct DrawingOptions;

class LightGatherer {
public:
	LightGatherer();
	~LightGatherer();

	void clear() noexcept {
		lights_.clear();
	}

	/**
	 * Gather all light sources affecting the target chunk (cx, cy, z),
	 * querying a 3x3 chunk neighborhood across visible floors.
	 */
	void gatherForChunk(
		const BaseMap& map,
		int32_t cx, int32_t cy, int32_t target_z,
		int32_t start_floor, int32_t superend_floor,
		GraphicManager& gfx
	);

	[[nodiscard]] const std::vector<rme::lighting::LightSource>& getLights() const noexcept {
		return lights_;
	}

	// Legacy gather compatibility for IngamePreviewRenderer
	static void Gather(
		const BaseMap& map,
		const RenderView& view,
		const DrawingOptions& options,
		LightBuffer& light_buffer
	);

private:
	std::vector<rme::lighting::LightSource> lights_;
};

namespace rme::lighting {
using LightGatherer = ::LightGatherer;
}

#endif // RME_RENDERING_CORE_LIGHT_GATHERER_H_
