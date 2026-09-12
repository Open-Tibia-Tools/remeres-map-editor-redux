#include "palette/panels/brush_panel.h"
#include "ui/gui.h"
#include "app/settings.h"
#include "palette/palette_window.h" // For PaletteWindow dynamic_casts
#include "palette/controls/virtual_brush_grid.h"
#include <spdlog/spdlog.h>
#include <wx/wrapsizer.h>
#include <algorithm>
#include <iterator>

// ============================================================================
// Brush Panel
// A container of brush buttons

BrushPanel::BrushPanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY),
	tileset(nullptr),
	brushbox(nullptr),
	loaded(false),
	list_type(BRUSHLIST_LISTBOX) {
	sizer = newd wxBoxSizer(wxVERTICAL);
	SetSizerAndFit(sizer);
}

BrushPanel::~BrushPanel() {
	////
}

void BrushPanel::AssignTileset(const DynamicTilesetDefinition* _tileset) {
	if (_tileset != tileset) {
		InvalidateContents();
		tileset = _tileset;
	}
}

void BrushPanel::SetListType(BrushListType ltype) {
	if (list_type != ltype) {
		InvalidateContents();
		list_type = ltype;
	}
}

void BrushPanel::SetListType(wxString ltype) {
	if (ltype == "32x32 px" || ltype == "small icons" || ltype == "large icons") {
		SetListType(BRUSHLIST_ICONS_32);
	} else if (ltype == "64x64 px") {
		SetListType(BRUSHLIST_ICONS_64);
	} else if (ltype == "128x128 px") {
		SetListType(BRUSHLIST_ICONS_128);
	} else if (ltype == "listbox" || ltype == "List style") {
		SetListType(BRUSHLIST_LISTBOX);
	} else if (ltype == "textlistbox") {
		SetListType(BRUSHLIST_TEXT_LISTBOX);
	}
}

void BrushPanel::InvalidateContents() {
	sizer->Clear(true);
	loaded = false;
	brushbox = nullptr;
}

void BrushPanel::LoadContents() {
	if (loaded) {
		return;
	}

	ASSERT(tileset != nullptr);

	int initialSize = tile_size_px;
	switch (list_type) {
		case BRUSHLIST_ICONS_32:
			brushbox = newd VirtualBrushGrid(this, tileset, initialSize);
			break;
		case BRUSHLIST_ICONS_64:
			brushbox = newd VirtualBrushGrid(this, tileset, initialSize > 32 ? initialSize : 64);
			break;
		case BRUSHLIST_ICONS_128:
			brushbox = newd VirtualBrushGrid(this, tileset, initialSize > 32 ? initialSize : 128);
			break;
		case BRUSHLIST_LISTBOX:
		case BRUSHLIST_TEXT_LISTBOX: {
			auto vbg = newd VirtualBrushGrid(this, tileset, initialSize);
			vbg->SetDisplayMode(VirtualBrushGrid::DisplayMode::List);
			brushbox = vbg;
			break;
		}
		default:
			break;
	}

	if (!brushbox) {
		return;
	}

	if (has_sort) {
		brushbox->SetSort(sort_key, sort_dir);
	}
	brushbox->SetShowLabels(show_labels);
	brushbox->SetTileSize(tile_size_px);

	loaded = true;
	sizer->Add(brushbox->GetSelfWindow(), 1, wxEXPAND);
	Layout();
	Fit();
	brushbox->SelectFirstBrush();
}

void BrushPanel::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	has_sort = true;
	sort_key = key;
	sort_dir = dir;
	if (brushbox) {
		brushbox->SetSort(key, dir);
	}
}

void BrushPanel::SetShowLabels(bool show) {
	show_labels = show;
	if (brushbox) {
		brushbox->SetShowLabels(show);
	}
}

void BrushPanel::SetTileSize(int sizePx) {
	tile_size_px = sizePx;
	if (brushbox) {
		brushbox->SetTileSize(sizePx);
	}
}

void BrushPanel::SelectFirstBrush() {
	if (loaded) {
		ASSERT(brushbox != nullptr);
		brushbox->SelectFirstBrush();
	}
}

Brush* BrushPanel::GetSelectedBrush() const {
	if (loaded) {
		ASSERT(brushbox != nullptr);
		return brushbox->GetSelectedBrush();
	}

	if (tileset && tileset->size() > 0) {
		return tileset->brushes[0];
	}
	return nullptr;
}

bool BrushPanel::SelectBrush(const Brush* whatbrush) {
	if (loaded) {
		ASSERT(brushbox != nullptr);
		return brushbox->SelectBrush(whatbrush);
	}

	for (const auto* brush : tileset->brushes) {
		if (brush == whatbrush) {
			LoadContents();
			return brushbox->SelectBrush(whatbrush);
		}
	}
	return false;
}

void BrushPanel::OnSwitchIn() {
	spdlog::info("BrushPanel::OnSwitchIn");
	LoadContents();
}

void BrushPanel::OnSwitchOut() {
	////
}

