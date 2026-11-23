//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_FAVORITES_FAVORITES_PALETTE_H_
#define RME_PALETTE_FAVORITES_FAVORITES_PALETTE_H_

#include "palette/palette_common.h"
#include "game/material_database.h"

#include <string>

class TilesetGridToolbar;
class VirtualBrushGrid;
class wxStaticBoxSizer;
class wxStaticText;

// "Favorites" palette page: the brushes and RAW items marked as favorite (Quick Tilesets state of
// the loaded client version), in the order they were added.
class FavoritesPalette : public PalettePanel {
public:
	explicit FavoritesPalette(wxWindow* parent, wxWindowID id = wxID_ANY);
	~FavoritesPalette() override = default;

	wxString GetName() const override;

	void InvalidateContents() override;
	void LoadCurrentContents() override;
	void LoadAllContents() override;

	void SelectFirstBrush() override;
	Brush* GetSelectedBrush() const override;
	bool SelectBrush(const Brush* whatbrush) override;

	void OnSwitchIn() override;
	void OnUpdate() override;

	// Favorites were added or removed: reload the grid.
	void OnFavoritesChanged();

	// Palette-wide grid options, forwarded by PaletteWindow like for the other brush pages.
	void SetBrushIndicators(bool dogEar);
	void SetGridSpacing(int spacing);

private:
	void ReloadFavorites();

	DynamicTilesetDefinition tileset_def;
	wxStaticBoxSizer* box_sizer = nullptr;
	TilesetGridToolbar* toolbar = nullptr;
	VirtualBrushGrid* grid = nullptr;
	wxStaticText* hint = nullptr;

	std::string filter_query;
	bool show_labels = false;
	int tile_size = 32;
	bool is_list_mode = false;
	bool indicator_dogear = false;
	int grid_spacing = 4;
};

#endif
