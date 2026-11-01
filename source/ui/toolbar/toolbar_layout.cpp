//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/toolbar/toolbar_layout.h"

void ToolbarLayout::Configure(wxAuiManager* manager, ToolbarRegistry* registry) {
	if (!manager || !registry) {
		return;
	}

	// Add panes at top dock row 0 with 0-indexed positions and grippers
	if (auto tb = registry->GetStandardToolbar()) {
		manager->AddPane(tb->GetToolbar(), wxAuiPaneInfo().Name(StandardToolBar::PANE_NAME).ToolbarPane().Top().Row(0).Position(0).Gripper(true).Floatable(false));
	}
	if (auto tb = registry->GetLightToolbar()) {
		manager->AddPane(tb->GetToolbar(), wxAuiPaneInfo().Name(LightToolBar::PANE_NAME).ToolbarPane().Top().Row(0).Position(1).Gripper(true).Floatable(false));
	}
	if (auto tb = registry->GetBrushToolbar()) {
		manager->AddPane(tb->GetToolbar(), wxAuiPaneInfo().Name(BrushToolBar::PANE_NAME).ToolbarPane().Top().Row(0).Position(2).Gripper(true).Floatable(false));
	}
	if (auto tb = registry->GetSizeToolbar()) {
		manager->AddPane(tb->GetToolbar(), wxAuiPaneInfo().Name(SizeToolBar::PANE_NAME).ToolbarPane().Top().Row(0).Position(3).Gripper(true).Floatable(false));
	}
	if (auto tb = registry->GetPositionToolbar()) {
		manager->AddPane(tb->GetToolbar(), wxAuiPaneInfo().Name(PositionToolBar::PANE_NAME).ToolbarPane().Top().Row(0).Position(4).Gripper(true).Floatable(false));
	}
}
