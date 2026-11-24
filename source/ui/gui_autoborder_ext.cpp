
#include "ui/gui.h"
#include "brushes/brush.h"
#include "app/settings.h"
#include "editor/editor.h"
#include "map/map.h"
#include "map/position.h"
#include "brushes/managers/autoborder_preview_manager.h"
#include "brushes/doodad/doodad_brush.h"

#include "rendering/ui/map_display.h"
#include "ui/autoborder_ui_state.h"
#include "ui/main_frame.h"
#include "ui/main_menubar.h"
#include "ui/managers/status_manager.h"
#include "ui/map_tab.h"
#include "util/common.h"

void GUI::SetAutoborderEnabled(bool enabled) {
	g_settings.setInteger(Config::USE_AUTOMAGIC, enabled ? 1 : 0);
	g_settings.setInteger(Config::BORDER_IS_GROUND, enabled ? 1 : 0);

	if (root) {
		if (MainMenuBar* menu_bar = root->GetMainMenuBar()) {
			menu_bar->CheckItem(MenuBar::AUTOMAGIC, enabled);
		}
	}

	g_status.SetStatusText(wxstr(rme::ui::AutoborderToggleStatusMessage(enabled)));
	g_status.UpdateAutoborderIndicator();

	if (MapTab* map_tab = GetCurrentMapTab()) {
		if (auto* canvas = map_tab->GetCanvas()) {
			const Position cursor_pos = canvas->GetCursorPosition();
			if (cursor_pos.isValid()) {
				UpdateAutoborderPreview(cursor_pos);
			} else if (!enabled && map_tab->GetSession()->secondary_map == g_autoborder_preview.GetBufferMap()) {
				g_autoborder_preview.Clear();
				SetCurrentMapSecondaryMap(nullptr);
			}
		}
	}

	SyncCurrentMapCanvasPreviewState();
	RefreshView();
}

void GUI::UpdateAutoborderPreview(Position pos) {
	MapTab* mapTab = GetCurrentMapTab();
	Brush* brush = GetCurrentBrush();
	if (brush && brush->is<DoodadBrush>()) {
		if (mapTab && mapTab->GetSession()->secondary_map == g_autoborder_preview.GetBufferMap()) {
			g_autoborder_preview.Clear();
			SetCurrentMapSecondaryMap(nullptr);
		}
		return;
	}

	// Check for editor to ensure we don't dereference a null pointer
	const bool should_run = rme::ui::ShouldRunAutoborderPreview(
		IsDrawingMode(),
		brush && brush->supportsAutoborder(),
		false,
		g_settings.getInteger(Config::USE_AUTOMAGIC) != 0,
		g_settings.getInteger(Config::SHOW_AUTOBORDER_PREVIEW) != 0
	);
	if (should_run && mapTab) {
		g_autoborder_preview.Update(*(mapTab->GetEditor()), pos, wxGetKeyState(WXK_ALT));
		SetCurrentMapSecondaryMap(g_autoborder_preview.GetBufferMap());
	} else if (mapTab) {
		if (mapTab->GetSession()->secondary_map == g_autoborder_preview.GetBufferMap()) {
			g_autoborder_preview.Clear();
			SetCurrentMapSecondaryMap(nullptr);
		}
	}
}
