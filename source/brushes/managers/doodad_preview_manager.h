//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_DOODAD_PREVIEW_MANAGER_H_
#define RME_DOODAD_PREVIEW_MANAGER_H_

#include "app/main.h"
#include <memory>

class BaseMap;
class DoodadBrush;

class DoodadPreviewManager {
public:
	DoodadPreviewManager();
	~DoodadPreviewManager();

	void FillBuffer();
	BaseMap* GetBufferMap() {
		return doodad_buffer_map.get();
	}

	void Clear();

	// Fills `buffer` with the single-tile composite of `variation`, centred like the preview
	// buffer, without reading or changing the GUI's brush state.
	static void FillSingleTile(BaseMap& buffer, DoodadBrush& brush, int variation);

private:
	static void AddSingleTileVariant(BaseMap& buffer, DoodadBrush& brush, int variation);

	std::unique_ptr<BaseMap> doodad_buffer_map;
};

extern DoodadPreviewManager g_doodad_preview;

#endif
