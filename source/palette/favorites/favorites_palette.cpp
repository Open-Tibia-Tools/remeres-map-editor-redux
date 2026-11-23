//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/favorites/favorites_palette.h"

#include "app/main.h"
#include "app/settings.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "palette/controls/virtual_brush_grid.h"
#include "palette/quick_tilesets/quick_tilesets.h"
#include "ui/theme.h"

FavoritesPalette::FavoritesPalette(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id) {
	tileset_def.name = "Favorites";
	indicator_dogear = g_settings.getBoolean(Config::PALETTE_INDICATOR_DOGEAR);
	const int loadedSpacing = g_settings.getInteger(Config::PALETTE_GRID_SPACING);
	grid_spacing = (loadedSpacing == 2 || loadedSpacing == 6) ? loadedSpacing : 4;

	auto* main_sizer = new wxBoxSizer(wxVERTICAL);
	box_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Favorites");

	TilesetGridToolbar::Actions actions;
	actions.show_labels = [this](bool show) {
		show_labels = show;
		if (grid) {
			grid->SetShowLabels(show);
		}
	};
	actions.icon_size = [this](int size) {
		tile_size = size;
		is_list_mode = false;
		if (grid) {
			grid->SetDisplayMode(TilesetDisplayMode::Grid);
			grid->SetTileSize(size);
		}
		if (toolbar) {
			toolbar->SetState(show_labels, tile_size, false, TilesetSortKey::Name, TilesetSortDirection::Ascending, false, is_list_mode);
		}
	};
	actions.list_mode = [this](bool list) {
		is_list_mode = list;
		if (grid) {
			grid->SetDisplayMode(list ? TilesetDisplayMode::List : TilesetDisplayMode::Grid);
		}
		if (toolbar) {
			toolbar->SetState(show_labels, tile_size, false, TilesetSortKey::Name, TilesetSortDirection::Ascending, false, is_list_mode);
		}
	};
	actions.filter = [this](const std::string& query) {
		filter_query = query;
		if (grid) {
			grid->SetFilterQuery(filter_query);
		}
	};
	actions.filter_committed = [this]() {
		if (grid) {
			grid->SelectFirstBrush();
		}
	};
	actions.filter_cleared = [this]() {
		filter_query.clear();
		if (toolbar) {
			toolbar->SetFilterQuery("");
		}
		if (grid) {
			grid->SetFilterQuery(filter_query);
		}
	};
	actions.filter_focus_lost = []() {};

	toolbar = new TilesetGridToolbar(this, std::move(actions), TilesetGridToolbar::Part_Search | TilesetGridToolbar::Part_Labels | TilesetGridToolbar::Part_Size);
	toolbar->SetState(show_labels, tile_size, false, TilesetSortKey::Name, TilesetSortDirection::Ascending, false, is_list_mode);
	box_sizer->Add(toolbar, wxSizerFlags().Expand().Border(wxBOTTOM, FromDIP(2)));

	hint = new wxStaticText(this, wxID_ANY, "Right-click a brush to add it to Favorites.");
	hint->SetForegroundColour(Theme::Get(Theme::Role::Text));
	box_sizer->Add(hint, wxSizerFlags().Border(wxALL, FromDIP(4)));

	main_sizer->Add(box_sizer, wxSizerFlags(1).Expand());
	SetSizer(main_sizer);
}

wxString FavoritesPalette::GetName() const {
	return "Favorites";
}

void FavoritesPalette::InvalidateContents() {
	tileset_def.brushes.clear();
	if (grid) {
		box_sizer->Detach(grid);
		grid->Destroy();
		grid = nullptr;
	}
}

void FavoritesPalette::LoadCurrentContents() {
	if (!grid) {
		grid = new VirtualBrushGrid(this, &tileset_def, tile_size);
		if (is_list_mode) {
			grid->SetDisplayMode(TilesetDisplayMode::List);
		}
		grid->SetShowLabels(show_labels);
		grid->SetBrushIndicators(indicator_dogear);
		grid->SetGridSpacing(grid_spacing);
		if (!filter_query.empty()) {
			grid->SetFilterQuery(filter_query);
		}
		box_sizer->Add(grid, wxSizerFlags(1).Expand());
	}
	ReloadFavorites();
}

void FavoritesPalette::LoadAllContents() {
	LoadCurrentContents();
}

void FavoritesPalette::SelectFirstBrush() {
	if (grid) {
		grid->SelectFirstBrush();
	}
}

Brush* FavoritesPalette::GetSelectedBrush() const {
	return grid ? grid->GetSelectedBrush() : nullptr;
}

bool FavoritesPalette::SelectBrush(const Brush* whatbrush) {
	if (!whatbrush) {
		if (grid) {
			grid->SelectBrush(nullptr);
		}
		return false;
	}
	if (!grid) {
		LoadCurrentContents();
	}
	return grid && grid->SelectBrush(whatbrush);
}

void FavoritesPalette::OnSwitchIn() {
	LoadCurrentContents();
}

void FavoritesPalette::OnUpdate() {
	if (grid) {
		ReloadFavorites();
	}
}

void FavoritesPalette::OnFavoritesChanged() {
	if (grid) {
		ReloadFavorites();
	}
}

void FavoritesPalette::SetBrushIndicators(bool dogEar) {
	indicator_dogear = dogEar;
	if (grid) {
		grid->SetBrushIndicators(dogEar);
	}
}

void FavoritesPalette::SetGridSpacing(int spacing) {
	grid_spacing = (spacing == 2 || spacing == 6) ? spacing : 4;
	if (grid) {
		grid->SetGridSpacing(grid_spacing);
	}
}

void FavoritesPalette::ReloadFavorites() {
	tileset_def.brushes = QuickTilesets::GetFavorites();
	hint->Show(tileset_def.brushes.empty());
	grid->ReloadBrushes();
	Layout();
}
