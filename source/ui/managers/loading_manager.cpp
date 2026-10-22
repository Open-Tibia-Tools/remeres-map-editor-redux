//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/managers/loading_manager.h"
#include <algorithm>
#include <format>

#include "ui/gui.h"
#include "editor/editor.h"
#include "ui/map_tab.h"
#include "live/live_server.h"

LoadingManager g_loading;

LoadingManager::LoadingManager() :
	progressBar(nullptr),
	infoDialog(nullptr),
	infoText(nullptr),
	infoDisabler(nullptr),
	cancelled(false),
	cancellable(false),
	progressFrom(0),
	progressTo(100),
	currentProgress(-1) {
}

LoadingManager::~LoadingManager() {
	DestroyLoadBar();
}

void LoadingManager::CreateInfoBar(wxString message, const wxString& title) {
	DestroyLoadBar();

	progressText = message;
	progressFrom = 0;
	progressTo = 100;
	currentProgress = -1;
	cancelled = false;
	cancellable = false;

	// Same family as the progress bar: owned by the editor window, centred on
	// it, and app-modal so the editor cannot be used (or dragged around) while
	// the operation runs. No gauge and no cancel button, since there is nothing
	// to measure and nothing to abort.
	infoDialog = newd wxDialog(g_gui.root, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxCAPTION);
	infoDialog->SetCursor(*wxHOURGLASS_CURSOR);

	wxBoxSizer* sizer = newd wxBoxSizer(wxVERTICAL);
	infoText = newd wxStaticText(infoDialog, wxID_ANY, message, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	infoText->SetMinSize(infoDialog->FromDIP(wxSize(320, -1)));
	sizer->Add(infoText, wxSizerFlags(1).Expand().DoubleBorder());
	infoDialog->SetSizerAndFit(sizer);
	infoDialog->CentreOnParent();

	infoDisabler = std::make_unique<wxWindowDisabler>(infoDialog);
	infoDialog->Show(true);
	infoDialog->Update();
}

void LoadingManager::CreateLoadBar(wxString message, bool canCancel, const wxString& title) {
	DestroyLoadBar();

	progressText = message;

	progressFrom = 0;
	progressTo = 100;
	currentProgress = -1;
	cancelled = false;
	cancellable = canCancel;

	progressBar = newd wxGenericProgressDialog(title, progressText + " (0%)", 100, g_gui.root, wxPD_APP_MODAL | wxPD_SMOOTH | (canCancel ? wxPD_CAN_ABORT : 0));
	progressBar->Show(true);

	if (g_gui.tabbook) {
		for (int idx = 0; idx < g_gui.tabbook->GetTabCount(); ++idx) {
			auto* mt = dynamic_cast<MapTab*>(g_gui.tabbook->GetTab(idx));
			if (mt && mt->GetEditor()->live_manager.IsServer()) {
				mt->GetEditor()->live_manager.GetServer()->startOperation(progressText);
			}
		}
	}
	progressBar->Update(0);
}

void LoadingManager::SetLoadScale(int32_t from, int32_t to) {
	progressFrom = from;
	progressTo = to;
}

bool LoadingManager::SetLoadDone(int32_t done, const wxString& newMessage) {
	if (done == 100) {
		if (progressBar) {
			DestroyLoadBar();
		}
		return true;
	} else if (done == currentProgress && newMessage.empty() && !cancellable) {
		// Same value and same text with no Abort button to poll: nothing to
		// repaint. Cancellable bars must keep pumping so the click is seen.
		return true;
	}

	if (!newMessage.empty()) {
		progressText = newMessage;
	}

	int32_t newProgress = progressFrom + static_cast<int32_t>((done / 100.f) * (progressTo - progressFrom));
	newProgress = std::clamp(newProgress, 0, 100);

	bool skip = false;
	if (progressBar) {
		// Update() returns false once the user cancelled; the skip out-param
		// only covers the (unused here) Skip button.
		const bool still_running = progressBar->Update(
			newProgress,
			wxstr(std::format("{} ({}%)", progressText.ToStdString(), newProgress)),
			&skip
		);
		currentProgress = newProgress;
		if (!still_running || skip) {
			cancelled = true;
		}
	}

	if (g_gui.tabbook) {
		for (int32_t index = 0; index < g_gui.tabbook->GetTabCount(); ++index) {
			auto* mapTab = dynamic_cast<MapTab*>(g_gui.tabbook->GetTab(index));
			if (mapTab && mapTab->GetEditor()) {
				LiveServer* server = mapTab->GetEditor()->live_manager.GetServer();
				if (server) {
					server->updateOperation(newProgress);
				}
			}
		}
	}

	return skip;
}

void LoadingManager::SetLoadText(const wxString& message) {
	if (message == progressText) {
		return;
	}

	progressText = message;

	if (infoDialog) {
		// Informational mode has no gauge: just repaint the new text.
		infoText->SetLabel(message);
		infoDialog->Layout();
		infoDialog->Update();
		return;
	}

	if (!progressBar) {
		return;
	}

	const int32_t visible = std::max<int32_t>(0, currentProgress);
	bool skip = false;
	if (!progressBar->Update(visible, wxstr(std::format("{} ({}%)", progressText.ToStdString(), visible)), &skip) || skip) {
		cancelled = true;
	}
}

bool LoadingManager::PollCancellation() {
	if (!progressBar) {
		return false;
	}

	const int32_t visible = std::max<int32_t>(0, currentProgress);
	bool skip = false;
	if (!progressBar->Update(visible, wxstr(std::format("{} ({}%)", progressText.ToStdString(), visible)), &skip) || skip) {
		cancelled = true;
	}
	return cancelled;
}

void LoadingManager::DestroyLoadBar() {
	if (progressBar) {
		progressBar->Show(false);
		currentProgress = -1;

		progressBar->Destroy();
		progressBar = nullptr;

		if (g_gui.root && g_gui.root->IsActive()) {
			g_gui.root->Raise();
		} else if (g_gui.root) {
			g_gui.root->RequestUserAttention();
		}
	}

	infoDisabler.reset();

	if (infoDialog) {
		infoDialog->Show(false);
		infoDialog->Destroy();
		infoDialog = nullptr;
		infoText = nullptr;
		currentProgress = -1;
	}

	// Cancellation belongs to the dialog being closed; later operations that
	// run with no dialog must not inherit it.
	cancelled = false;
	cancellable = false;
}
