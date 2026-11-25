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

#include "app/main.h"

#include "editor/editor.h"
#include "editor/editor_tabs.h"
#include "editor/managers/editor_manager.h"
#include "editor/tab_close_decision.h"
#include "live/live_tab.h"
#include "rendering/ui/map_display.h"
#include "ui/managers/status_manager.h"

EditorTab::EditorTab() {
	;
}

EditorTab::~EditorTab() {
	;
}

MapTabbook::MapTabbook(wxWindow* parent, wxWindowID id) :
	wxPanel(parent, id, wxDefaultPosition, wxDefaultSize) {
	wxSizer* wxz = new wxBoxSizer(wxHORIZONTAL);
	notebook = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
	wxz->Add(notebook, wxSizerFlags(1).Expand());
	SetSizerAndFit(wxz);

	notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &MapTabbook::OnNotebookPageClose, this);
	notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSED, &MapTabbook::OnNotebookPageClosed, this);
	notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED, &MapTabbook::OnNotebookPageChanged, this);
}

MapTabbook::~MapTabbook() {
	;
}

void MapTabbook::OnNotebookPageClose(wxAuiNotebookEvent& evt) {
    const int tab_idx = evt.GetSelection();
    EditorTab* editorTab = GetTab(tab_idx);

    MapTab* mapTab = dynamic_cast<MapTab*>(editorTab);
    LiveLogTab* liveTab = dynamic_cast<LiveLogTab*>(editorTab);

    TabCloseKind kind = TabCloseKind::Other;
    if (mapTab && mapTab->GetMap()) {
        kind = mapTab->IsUniqueReference() ? TabCloseKind::UniqueMap : TabCloseKind::SharedMapView;
    } else if (liveTab) {
        kind = TabCloseKind::LiveLog;
    }

    const TabCloseDecision decision = DecideTabCloseAction(TabCloseDecisionInput {
        .tab_index = tab_idx,
        .tab_count = GetTabCount(),
        .kind = kind,
        .map_has_changed = mapTab && mapTab->GetMap() && mapTab->GetMap()->hasChanged(),
        .live_connected = liveTab && liveTab->IsConnected(),
    });

    switch (decision.action) {
        case TabCloseAction::Ignore:
        case TabCloseAction::VetoAndLeaveOpen:
            evt.Veto();
            return;
        case TabCloseAction::AllowNativeClose:
            return;
        case TabCloseAction::CloseThroughManager: {
            wxWindow* const tab_window = mapTab->GetWindow();
            if (decision.prompt_save) {
                if (!g_gui.root || !mapTab->GetEditor() || !g_gui.root->DoQuerySave(*mapTab->GetEditor(), mapTab, false)) {
                    evt.Veto();
                    return;
                }
            }

            // Take the deletion over from wxAuiNotebook (which would otherwise
            // free the whole map right after this handler returns, with no
            // feedback) so CloseTab can show the Closing progress bar.
            evt.Veto();
            const int resolved_idx = GetTabIndex(tab_window);
            if (resolved_idx != wxNOT_FOUND) {
                g_editors.CloseTab(resolved_idx);
            }
            return;
        }
    }
}

void MapTabbook::OnNotebookPageClosed(wxAuiNotebookEvent& evt) {
	// The page is gone by now, so the map has finished being destroyed.
	g_status.SetStatusText("Map closed.");
	evt.Skip();
}

void MapTabbook::OnNotebookPageChanged(wxAuiNotebookEvent& evt) {
	g_gui.UpdateMinimap();

	int32_t oldSelection = evt.GetOldSelection();
	int32_t newSelection = evt.GetSelection();

	MapTab* oldMapTab;
	if (oldSelection != -1) {
		oldMapTab = dynamic_cast<MapTab*>(GetTab(oldSelection));
	} else {
		oldMapTab = nullptr;
	}

	MapTab* newMapTab;
	if (newSelection != -1) {
		newMapTab = dynamic_cast<MapTab*>(GetTab(newSelection));
	} else {
		newMapTab = nullptr;
	}

	if (!newMapTab) {
		g_gui.RefreshPalettes(nullptr);
	} else if (!oldMapTab || !oldMapTab->HasSameReference(newMapTab)) {
		g_gui.RefreshPalettes(newMapTab->GetMap());
		g_gui.UpdateMenus();
	}

	if (oldMapTab) {
		// A lasso drawn in the old tab must never be applied after the switch.
		if (MapCanvas* old_canvas = oldMapTab->GetCanvas()) {
			old_canvas->CancelLasso();
		}
		oldMapTab->VisibilityCheck();
	}
	if (newMapTab) {
		newMapTab->VisibilityCheck();
	}
}

void MapTabbook::OnAllowNotebookDND(wxAuiNotebookEvent& evt) {
	evt.Allow();
}

// Wrappers

void MapTabbook::AddTab(EditorTab* tab, bool select) {
    if (!tab || !notebook) {
        return;
    }
    wxWindow* window = tab->GetWindow();
    if (!window) {
        return;
    }
    window->Reparent(notebook);
    notebook->AddPage(window, tab->GetTitle(), select);
}

void MapTabbook::SetFocusedTab(int idx) {
    if (notebook && idx >= 0 && idx < GetTabCount()) {
        notebook->SetSelection(idx);
    }
}

EditorTab* MapTabbook::GetInternalTab(int idx) {
    if (!notebook || idx < 0 || idx >= GetTabCount()) {
        return nullptr;
    }
    return dynamic_cast<EditorTab*>(notebook->GetPage(idx));
}

EditorTab* MapTabbook::GetCurrentTab() {
    const int sel = GetSelection();
    if (sel < 0 || sel >= GetTabCount()) {
        return nullptr;
    }
    return GetInternalTab(sel);
}

EditorTab* MapTabbook::GetTab(int idx) {
	return GetInternalTab(idx);
}

wxWindow* MapTabbook::GetCurrentPage() {
    EditorTab* tab = GetCurrentTab();
    return tab ? tab->GetWindow() : nullptr;
}

void MapTabbook::OnSwitchEditorMode(EditorMode mode) {
	for (int32_t i = 0; i < GetTabCount(); ++i) {
		EditorTab* editorTab = GetTab(i);
		if (editorTab) {
			editorTab->OnSwitchEditorMode(mode);
		}
	}
}

void MapTabbook::SetTabLabel(int idx, wxString label) {
    if (notebook && idx >= 0 && idx < GetTabCount()) {
        notebook->SetPageText(idx, label);
    }
}

void MapTabbook::DeleteTab(int idx) {
    if (notebook && idx >= 0 && idx < GetTabCount()) {
        notebook->DeletePage(idx);
    }
}

int MapTabbook::GetTabCount() {
	if (notebook) {
		return notebook->GetPageCount();
	}
	return 0;
}

int MapTabbook::GetTabIndex(wxWindow* w) {
    if (notebook && w) {
        return notebook->GetPageIndex(w);
    }
    return wxNOT_FOUND;
}

int MapTabbook::GetSelection() {
	if (notebook) {
		return notebook->GetSelection();
	}
    return wxNOT_FOUND;
}
