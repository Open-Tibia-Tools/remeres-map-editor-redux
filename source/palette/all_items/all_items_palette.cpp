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

#include "palette/all_items/all_items_palette.h"

#include "app/main.h"
#include "app/settings.h"
#include "brushes/raw/raw_brush.h"
#include "game/materials.h"
#include "item_definitions/core/item_definition_store.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "palette/controls/virtual_brush_grid.h"
#include "ui/gui.h"

AllItemsPalette::AllItemsPalette(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id) {
	tileset_def.name = "All items";

	main_sizer = newd wxBoxSizer(wxVERTICAL);

	TilesetGridToolbar::Actions actions;
	actions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		SetSort(key, dir);
	};
	actions.clear_sort = [this]() {
		ClearSort();
	};
	actions.show_labels = [this](bool show) {
		SetShowLabels(show);
	};
	actions.icon_size = [this](int size) {
		if (is_list_mode) {
			is_list_mode = false;
			if (grid) {
				grid->SetDisplayMode(TilesetDisplayMode::Grid);
			}
		}
		SetTileSize(size);
	};
	actions.list_mode = [this](bool list) {
		is_list_mode = list;
		if (grid) {
			grid->SetDisplayMode(list ? TilesetDisplayMode::List : TilesetDisplayMode::Grid);
		}
		if (toolbar) {
			toolbar->SetState(show_labels, tile_size, has_sort, sort_key, sort_dir, false, is_list_mode);
		}
	};
	actions.filter = [this](const std::string& query) {
		filter_query = query;
		ApplyFilter();
	};
	actions.filter_committed = [this]() {
		ApplyFilter();
		if (grid) {
			grid->SelectFirstBrush();
		}
	};
	actions.filter_cleared = [this]() {
		ResetFilter();
	};
	actions.filter_focus_lost = []() {};

	toolbar = newd TilesetGridToolbar(this, std::move(actions), TilesetGridToolbar::Part_Standard);
	toolbar->SetState(show_labels, tile_size, has_sort, sort_key, sort_dir, false, is_list_mode);
	main_sizer->Add(toolbar, 0, wxEXPAND | wxBOTTOM, 2);

	SetSizer(main_sizer);
}

AllItemsPalette::~AllItemsPalette() = default;

wxString AllItemsPalette::GetName() const {
	return "All items";
}

void AllItemsPalette::EnsureBrushesLoaded() {
	if (loaded) {
		return;
	}

	tileset_def.brushes.clear();
	const auto& all_ids = g_item_definitions.allIds();
	tileset_def.brushes.reserve(all_ids.size());

	for (ServerItemId id : all_ids) {
		if (id < 100) {
			continue;
		}
		RAWBrush* brush = g_materials.ensureRawBrush(id);
		if (brush) {
			tileset_def.brushes.push_back(brush);
		}
	}

	loaded = true;
}

void AllItemsPalette::InvalidateContents() {
	loaded = false;
	tileset_def.brushes.clear();
	if (grid) {
		main_sizer->Detach(grid);
		grid->Destroy();
		grid = nullptr;
	}
}

void AllItemsPalette::LoadCurrentContents() {
	EnsureBrushesLoaded();
	if (!grid) {
		grid = newd VirtualBrushGrid(this, &tileset_def, tile_size);
		if (is_list_mode) {
			grid->SetDisplayMode(TilesetDisplayMode::List);
		}
		if (has_sort) {
			grid->SetSort(sort_key, sort_dir);
		}
		grid->SetShowLabels(show_labels);
		if (!filter_query.empty()) {
			grid->SetFilterQuery(filter_query);
		}
		main_sizer->Add(grid, 1, wxEXPAND);
		Layout();
	}
}

void AllItemsPalette::LoadAllContents() {
	LoadCurrentContents();
}

void AllItemsPalette::SelectFirstBrush() {
	if (grid) {
		grid->SelectFirstBrush();
	}
}

Brush* AllItemsPalette::GetSelectedBrush() const {
	if (grid) {
		return grid->GetSelectedBrush();
	}
	return nullptr;
}

bool AllItemsPalette::SelectBrush(const Brush* whatbrush) {
	if (!whatbrush) {
		return false;
	}
	EnsureBrushesLoaded();
	if (!grid) {
		LoadCurrentContents();
	}
	if (grid) {
		return grid->SelectBrush(whatbrush);
	}
	return false;
}

void AllItemsPalette::OnSwitchIn() {
	LoadCurrentContents();
}

void AllItemsPalette::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	sort_key = key;
	sort_dir = dir;
	has_sort = true;
	if (grid) {
		grid->SetSort(key, dir);
	}
}

void AllItemsPalette::ClearSort() {
	has_sort = false;
	if (grid) {
		grid->ClearSort();
	}
}

void AllItemsPalette::SetShowLabels(bool show) {
	show_labels = show;
	if (grid) {
		grid->SetShowLabels(show);
	}
}

void AllItemsPalette::SetTileSize(int sizePx) {
	tile_size = sizePx;
	if (is_list_mode) {
		is_list_mode = false;
		if (grid) {
			grid->SetDisplayMode(TilesetDisplayMode::Grid);
		}
	}
	if (toolbar) {
		toolbar->SetState(show_labels, tile_size, has_sort, sort_key, sort_dir, false, is_list_mode);
	}
	if (grid) {
		grid->SetTileSize(sizePx);
	}
}

void AllItemsPalette::ResetFilter() {
	filter_query.clear();
	if (toolbar) {
		toolbar->SetFilterQuery("");
	}
	ApplyFilter();
}

void AllItemsPalette::ApplyFilter() {
	if (grid) {
		grid->SetFilterQuery(filter_query);
	}
}
