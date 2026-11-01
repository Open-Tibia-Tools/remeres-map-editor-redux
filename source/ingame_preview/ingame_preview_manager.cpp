#include "app/settings.h"
#include <iostream>
#include "ingame_preview/ingame_preview_manager.h"
#include "ingame_preview/ingame_preview_window.h"
#include "util/image_manager.h"

#include "ui/gui.h"
#include "editor/editor.h"
#include <wx/aui/aui.h>

IngamePreview::IngamePreviewManager g_preview;

namespace IngamePreview {

	IngamePreviewManager::IngamePreviewManager() {
	}

	IngamePreviewManager::~IngamePreviewManager() {
		if (window) {
			window->Unbind(wxEVT_DESTROY, &IngamePreviewManager::OnWindowDestroy, this);
			if (g_gui.aui_manager) {
				g_gui.aui_manager->DetachPane(window);
			}
			// Window is managed by parent (g_gui.root) and nulled via OnWindowDestroy.
		}
	}

	wxAuiPaneInfo IngamePreviewManager::DefaultPaneInfo() {
		return wxAuiPaneInfo()
			.Name("IngamePreview")
			.Caption("In-game Preview")
			.Right()
			.Layer(0)
			.Position(1)
			.Dockable(true)
			.FloatingSize(g_gui.root->FromDIP(400), g_gui.root->FromDIP(300))
			.MinSize(g_gui.root->FromDIP(200), g_gui.root->FromDIP(150))
			.CloseButton(true)
			.MaximizeButton(true);
	}

	void IngamePreviewManager::NormalizePaneInfo(wxAuiPaneInfo& info) {
		info.Name("IngamePreview")
			.Caption("In-game Preview")
			.Dockable(true)
			.CloseButton(true)
			.MaximizeButton(true);
	}

	void IngamePreviewManager::Create(bool updateManager) {
		if (!g_gui.aui_manager) {
			return;
		}

		if (window) {
			g_gui.aui_manager->GetPane(window).Show(true);
		} else {
			window = new IngamePreviewWindow(g_gui.root);
			window->Bind(wxEVT_DESTROY, &IngamePreviewManager::OnWindowDestroy, this);

			wxAuiPaneInfo info = DefaultPaneInfo();
			const wxString layout = wxstr(g_settings.getString(Config::INGAME_PREVIEW_LAYOUT));
			if (!layout.empty()) {
				g_gui.aui_manager->LoadPaneInfo(layout, info);
			}
			NormalizePaneInfo(info);
			info.Show(true);

			g_gui.aui_manager->AddPane(window, info);
		}

		if (updateManager) {
			g_gui.aui_manager->Update();
		}
	}

	void IngamePreviewManager::Hide() {
		if (!g_gui.aui_manager) {
			return;
		}

		if (window) {
			g_gui.aui_manager->GetPane(window).Show(false);
			g_gui.aui_manager->Update();
		}
	}

	void IngamePreviewManager::Destroy() {
		if (window) {
			window->Unbind(wxEVT_DESTROY, &IngamePreviewManager::OnWindowDestroy, this);

			if (g_gui.aui_manager) {
				g_gui.aui_manager->DetachPane(window);
			}

			// Cache window pointer, set to nullptr to avoid reentrancy/handler recursion, then call Destroy().
			wxWindow* win = window;
			window = nullptr;
			win->Destroy();
		}
	}

	void IngamePreviewManager::OnWindowDestroy(wxWindowDestroyEvent& e) {
		if (e.GetEventObject() == window) {
			window = nullptr;
		}
		e.Skip();
	}

	void IngamePreviewManager::Update() {
		if (window && IsVisible()) {
			window->UpdateState();
		}
	}

	bool IngamePreviewManager::IsVisible() const {
		if (window && g_gui.aui_manager) {
			const wxAuiPaneInfo& pi = g_gui.aui_manager->GetPane(window);
			return pi.IsShown();
		}
		return false;
	}

} // namespace IngamePreview
