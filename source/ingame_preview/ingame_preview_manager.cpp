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

	void IngamePreviewManager::Create() {

		if (!g_gui.aui_manager) {
			return;
		}

		if (window) {
			g_gui.aui_manager->GetPane(window).Show(true);
		} else {
			window = new IngamePreviewWindow(g_gui.root);

			// Safety: Listen for window destruction (e.g. if parent is destroyed)
			window->Bind(wxEVT_DESTROY, &IngamePreviewManager::OnWindowDestroy, this);

			g_gui.aui_manager->AddPane(window, wxAuiPaneInfo().Name("IngamePreview").Caption("In-game Preview").Right().Dockable(true).FloatingSize(g_gui.root->FromDIP(400), g_gui.root->FromDIP(300)).MinSize(g_gui.root->FromDIP(200), g_gui.root->FromDIP(150)).Hide());

			g_gui.aui_manager->GetPane(window).Show(true);
		}
		g_gui.aui_manager->Update();
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
