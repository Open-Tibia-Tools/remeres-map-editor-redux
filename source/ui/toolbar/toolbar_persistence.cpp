//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/toolbar/toolbar_persistence.h"
#include "app/settings.h"
#include "ui/gui_ids.h"
#include "ui/toolbar/brush_toolbar.h"
#include "ui/toolbar/position_toolbar.h"
#include "ui/toolbar/size_toolbar.h"
#include "ui/toolbar/standard_toolbar.h"

void ToolbarPersistence::LoadPerspective(wxAuiManager* manager, MainToolBar* mainToolbar) {
	if (!manager || !mainToolbar) {
		return;
	}

	auto loadToolbar = [&](ToolBarID id, uint32_t showKey, uint32_t layoutKey) {
		wxAuiPaneInfo& pane = mainToolbar->GetPane(id);
		if (!pane.IsOk()) {
			return;
		}
		if (g_settings.getBoolean(showKey)) {
			std::string info = g_settings.getString(layoutKey);
			if (!info.empty()) {
				manager->LoadPaneInfo(wxString(info), pane);
			}
			pane.Show();
		} else {
			pane.Hide();
		}
	};

	loadToolbar(TOOLBAR_STANDARD, Config::SHOW_TOOLBAR_STANDARD, Config::TOOLBAR_STANDARD_LAYOUT);
	loadToolbar(TOOLBAR_BRUSHES, Config::SHOW_TOOLBAR_BRUSHES, Config::TOOLBAR_BRUSHES_LAYOUT);
	loadToolbar(TOOLBAR_POSITION, Config::SHOW_TOOLBAR_POSITION, Config::TOOLBAR_POSITION_LAYOUT);
	loadToolbar(TOOLBAR_SIZES, Config::SHOW_TOOLBAR_SIZES, Config::TOOLBAR_SIZES_LAYOUT);
	loadToolbar(TOOLBAR_LIGHT, Config::SHOW_TOOLBAR_LIGHT, Config::TOOLBAR_LIGHT_LAYOUT);

	manager->Update();
}

void ToolbarPersistence::SavePerspective(wxAuiManager* manager, MainToolBar* mainToolbar) {
	if (!manager || !mainToolbar) {
		return;
	}

	auto saveToolbar = [&](ToolBarID id, uint32_t showKey, uint32_t layoutKey) {
		wxAuiPaneInfo& pane = mainToolbar->GetPane(id);
		if (!pane.IsOk()) {
			return;
		}
		bool isShown = pane.IsShown();
		g_settings.setInteger(showKey, isShown ? 1 : 0);
		if (isShown) {
			wxString info = manager->SavePaneInfo(pane);
			g_settings.setString(layoutKey, info.ToStdString());
		}
	};

	saveToolbar(TOOLBAR_STANDARD, Config::SHOW_TOOLBAR_STANDARD, Config::TOOLBAR_STANDARD_LAYOUT);
	saveToolbar(TOOLBAR_BRUSHES, Config::SHOW_TOOLBAR_BRUSHES, Config::TOOLBAR_BRUSHES_LAYOUT);
	saveToolbar(TOOLBAR_POSITION, Config::SHOW_TOOLBAR_POSITION, Config::TOOLBAR_POSITION_LAYOUT);
	saveToolbar(TOOLBAR_SIZES, Config::SHOW_TOOLBAR_SIZES, Config::TOOLBAR_SIZES_LAYOUT);
	saveToolbar(TOOLBAR_LIGHT, Config::SHOW_TOOLBAR_LIGHT, Config::TOOLBAR_LIGHT_LAYOUT);
}
