//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_ALL_ITEMS_ALL_ITEMS_PALETTE_H_
#define RME_PALETTE_ALL_ITEMS_ALL_ITEMS_PALETTE_H_

#include "palette/palette_common.h"
#include "game/material_database.h"
#include "util/grid_view_options.h"

class TilesetGridToolbar;
class VirtualBrushGrid;

class AllItemsPalette : public PalettePanel {
public:
	AllItemsPalette(wxWindow* parent, wxWindowID id = wxID_ANY);
	~AllItemsPalette() override;

	wxString GetName() const override;

	void InvalidateContents() override;
	void LoadCurrentContents() override;
	void LoadAllContents() override;

	void SelectFirstBrush() override;
	Brush* GetSelectedBrush() const override;
	bool SelectBrush(const Brush* whatbrush) override;

	void OnSwitchIn() override;

	void SetSort(TilesetSortKey key, TilesetSortDirection dir);
	void ClearSort();
	void SetShowLabels(bool show);
	void SetTileSize(int sizePx);

	void ResetFilter();
	void ApplyFilter();

protected:
	void EnsureBrushesLoaded();

	DynamicTilesetDefinition tileset_def;
	wxBoxSizer* main_sizer = nullptr;
	TilesetGridToolbar* toolbar = nullptr;
	VirtualBrushGrid* grid = nullptr;

	std::string filter_query;
	TilesetSortKey sort_key = TilesetSortKey::ID;
	TilesetSortDirection sort_dir = TilesetSortDirection::Ascending;
	bool has_sort = true;
	bool show_labels = false;
	int tile_size = 32;
	bool is_list_mode = false;
	bool loaded = false;
};

#endif
