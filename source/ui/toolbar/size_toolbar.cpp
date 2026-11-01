//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/toolbar/size_toolbar.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "editor/editor.h"
#include "util/image_manager.h"
#include <wx/artprov.h>

const wxString SizeToolBar::PANE_NAME = "sizes_toolbar";

SizeToolBar::SizeToolBar(wxWindow* parent) {
	wxSize icon_size = parent->FromDIP(wxSize(16, 16));

	wxBitmapBundle circular_bitmap = IMAGE_MANAGER.GetBitmapBundle(IMAGE_CIRCULAR_4_SMALL);
	wxBitmapBundle rectangular_bitmap = IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_4_SMALL);
	wxBitmapBundle size1_bitmap = IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_1_SMALL);
	wxBitmapBundle size2_bitmap = IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_2_SMALL);
	wxBitmapBundle size3_bitmap = IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_3_SMALL);
	wxBitmapBundle size4_bitmap = IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_4_SMALL);
	wxBitmapBundle size5_bitmap = IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_5_SMALL);
	wxBitmapBundle size6_bitmap = IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_6_SMALL);
	wxBitmapBundle size7_bitmap = IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_7_SMALL);

	toolbar = new wxAuiToolBar(parent, TOOLBAR_SIZES, wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE);
	toolbar->SetToolBitmapSize(icon_size);
	toolbar->AddTool(TOOLBAR_SIZES_RECTANGULAR, wxEmptyString, rectangular_bitmap, wxNullBitmap, wxITEM_CHECK, "Rectangular Brush", wxEmptyString, nullptr);
	toolbar->AddTool(TOOLBAR_SIZES_CIRCULAR, wxEmptyString, circular_bitmap, wxNullBitmap, wxITEM_CHECK, "Circular Brush", wxEmptyString, nullptr);
	toolbar->AddSeparator();
	toolbar->AddTool(TOOLBAR_SIZES_1, wxEmptyString, size1_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 1", wxEmptyString, nullptr);
	toolbar->AddTool(TOOLBAR_SIZES_2, wxEmptyString, size2_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 2", wxEmptyString, nullptr);
	toolbar->AddTool(TOOLBAR_SIZES_3, wxEmptyString, size3_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 3", wxEmptyString, nullptr);
	toolbar->AddTool(TOOLBAR_SIZES_4, wxEmptyString, size4_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 4", wxEmptyString, nullptr);
	toolbar->AddTool(TOOLBAR_SIZES_5, wxEmptyString, size5_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 5", wxEmptyString, nullptr);
	toolbar->AddTool(TOOLBAR_SIZES_6, wxEmptyString, size6_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 6", wxEmptyString, nullptr);
	toolbar->AddTool(TOOLBAR_SIZES_7, wxEmptyString, size7_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 7", wxEmptyString, nullptr);
	toolbar->Realize();
	toolbar->ToggleTool(TOOLBAR_SIZES_RECTANGULAR, true);
	toolbar->ToggleTool(TOOLBAR_SIZES_1, true);

	toolbar->Bind(wxEVT_COMMAND_MENU_SELECTED, &SizeToolBar::OnToolbarClick, this);
}

SizeToolBar::~SizeToolBar() {
	toolbar->Unbind(wxEVT_COMMAND_MENU_SELECTED, &SizeToolBar::OnToolbarClick, this);
}

void SizeToolBar::Update() {
	Editor* editor = g_gui.GetCurrentEditor();
	bool has_map = editor != nullptr;

	toolbar->EnableTool(TOOLBAR_SIZES_CIRCULAR, has_map);
	toolbar->EnableTool(TOOLBAR_SIZES_RECTANGULAR, has_map);
	toolbar->EnableTool(TOOLBAR_SIZES_1, has_map);
	toolbar->EnableTool(TOOLBAR_SIZES_2, has_map);
	toolbar->EnableTool(TOOLBAR_SIZES_3, has_map);
	toolbar->EnableTool(TOOLBAR_SIZES_4, has_map);
	toolbar->EnableTool(TOOLBAR_SIZES_5, has_map);
	toolbar->EnableTool(TOOLBAR_SIZES_6, has_map);
	toolbar->EnableTool(TOOLBAR_SIZES_7, has_map);

	toolbar->Refresh();
}

void SizeToolBar::UpdateBrushSize(BrushShape shape, int size) {
	const BrushFootprint footprint = g_gui.GetBrushFootprint();
	const bool legacy_compatible = footprint.size_x == footprint.size_y && (!footprint.exact || footprint.isSingleTile());
	const int legacy_size = legacy_compatible ? size : -1;

	if (shape == BRUSHSHAPE_CIRCLE) {
		toolbar->ToggleTool(TOOLBAR_SIZES_CIRCULAR, true);
		toolbar->ToggleTool(TOOLBAR_SIZES_RECTANGULAR, false);

		toolbar->SetToolBitmap(TOOLBAR_SIZES_1, IMAGE_MANAGER.GetBitmapBundle(IMAGE_CIRCULAR_1_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_2, IMAGE_MANAGER.GetBitmapBundle(IMAGE_CIRCULAR_2_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_3, IMAGE_MANAGER.GetBitmapBundle(IMAGE_CIRCULAR_3_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_4, IMAGE_MANAGER.GetBitmapBundle(IMAGE_CIRCULAR_4_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_5, IMAGE_MANAGER.GetBitmapBundle(IMAGE_CIRCULAR_5_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_6, IMAGE_MANAGER.GetBitmapBundle(IMAGE_CIRCULAR_6_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_7, IMAGE_MANAGER.GetBitmapBundle(IMAGE_CIRCULAR_7_SMALL));
	} else {
		toolbar->ToggleTool(TOOLBAR_SIZES_CIRCULAR, false);
		toolbar->ToggleTool(TOOLBAR_SIZES_RECTANGULAR, true);

		toolbar->SetToolBitmap(TOOLBAR_SIZES_1, IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_1_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_2, IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_2_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_3, IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_3_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_4, IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_4_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_5, IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_5_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_6, IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_6_SMALL));
		toolbar->SetToolBitmap(TOOLBAR_SIZES_7, IMAGE_MANAGER.GetBitmapBundle(IMAGE_RECTANGULAR_7_SMALL));
	}

	toolbar->ToggleTool(TOOLBAR_SIZES_1, legacy_size == 0);
	toolbar->ToggleTool(TOOLBAR_SIZES_2, legacy_size == 1);
	toolbar->ToggleTool(TOOLBAR_SIZES_3, legacy_size == 2);
	toolbar->ToggleTool(TOOLBAR_SIZES_4, legacy_size == 4);
	toolbar->ToggleTool(TOOLBAR_SIZES_5, legacy_size == 6);
	toolbar->ToggleTool(TOOLBAR_SIZES_6, legacy_size == 8);
	toolbar->ToggleTool(TOOLBAR_SIZES_7, legacy_size == 11);

	g_gui.GetAuiManager()->Update();
}

void SizeToolBar::OnToolbarClick(wxCommandEvent& event) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	switch (event.GetId()) {
		case TOOLBAR_SIZES_CIRCULAR:
			g_gui.SetBrushShape(BRUSHSHAPE_CIRCLE);
			break;
		case TOOLBAR_SIZES_RECTANGULAR:
			g_gui.SetBrushShape(BRUSHSHAPE_SQUARE);
			break;
		case TOOLBAR_SIZES_1:
			g_gui.SetBrushSize(0);
			break;
		case TOOLBAR_SIZES_2:
			g_gui.SetBrushSize(1);
			break;
		case TOOLBAR_SIZES_3:
			g_gui.SetBrushSize(2);
			break;
		case TOOLBAR_SIZES_4:
			g_gui.SetBrushSize(4);
			break;
		case TOOLBAR_SIZES_5:
			g_gui.SetBrushSize(6);
			break;
		case TOOLBAR_SIZES_6:
			g_gui.SetBrushSize(8);
			break;
		case TOOLBAR_SIZES_7:
			g_gui.SetBrushSize(11);
			break;
		default:
			break;
	}
}
