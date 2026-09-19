//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_DOODAD_PREVIEW_MANAGER_H_
#define RME_DOODAD_PREVIEW_MANAGER_H_

#include "app/definitions.h"
#include "brushes/brush_footprint.h"
#include <memory>

class BaseMap;
class DoodadBrush;

class DoodadPreviewManager {
public:
	DoodadPreviewManager();
	~DoodadPreviewManager();

	void FillBuffer(
		DoodadBrush* brush,
		int variation,
		const BrushFootprint& footprint,
		bool useCustomThickness,
		float customThicknessMod
	);
	BaseMap* GetBufferMap() {
		return doodad_buffer_map.get();
	}

	void Clear();

private:
	std::unique_ptr<BaseMap> doodad_buffer_map;
};

extern DoodadPreviewManager g_doodad_preview;

#endif
