//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_MANAGER_H_
#define RME_PALETTE_MANAGER_H_

#include "app/main.h"
#include <list>
#include <string_view>

class PaletteWindow;
class Map;

class PaletteManager {
public:
	PaletteManager();
	~PaletteManager();

	PaletteWindow* GetPalette();
	const std::list<PaletteWindow*>& GetPalettes() const {
		return palettes;
	}

	PaletteWindow* NewPalette();
	void ActivatePalette(PaletteWindow* p);
	void RebuildPalettes();
	void RefreshPalettes(Map* m = nullptr, bool usedefault = true);
	void RefreshOtherPalettes(PaletteWindow* p);
	void ShowPalette();
	void SelectPalettePage(std::string_view name);
	void ReloadDynamicPalettes();
	bool SelectTilesetInPalette(std::string_view paletteName, std::string_view tilesetName);
	void DestroyPalettes();
	PaletteWindow* CreatePalette();

	// This is public for now as it's accessed by GUI for iteration
public:
	std::list<PaletteWindow*> palettes;

private:
	uint64_t palette_creation_counter;
};

extern PaletteManager g_palettes;

#endif
