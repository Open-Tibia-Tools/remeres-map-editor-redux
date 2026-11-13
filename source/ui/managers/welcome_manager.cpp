#include "ui/managers/welcome_manager.h"
#include "ui/gui.h"
#include "ui/welcome_dialog.h"
#include "ui/main_menubar.h"
#include "app/main.h"

WelcomeManager g_welcome;

WelcomeManager::WelcomeManager() :
	welcomeDialog(nullptr) {
}

WelcomeManager::~WelcomeManager() {
	// welcomeDialog is destroyed in FinishWelcomeDialog or OnWelcomeDialogClosed
}

void WelcomeManager::ShowWelcomeDialog(const wxBitmap& icon) {
	std::vector<wxString> recent_files = g_gui.root->GetRecentFiles();
	welcomeDialog = new WelcomeDialog(__W_RME_APPLICATION_NAME__, "Version " + __W_RME_VERSION__, g_gui.root->FromDIP(wxSize(1320, 820)), icon, recent_files);
	welcomeDialog->Bind(wxEVT_CLOSE_WINDOW, &WelcomeManager::OnWelcomeDialogClosed, this);
	welcomeDialog->Bind(WELCOME_DIALOG_ACTION, &WelcomeManager::OnWelcomeDialogAction, this);
	welcomeDialog->Show();
	g_gui.UpdateMenubar();
}

void WelcomeManager::FinishWelcomeDialog() {
	if (welcomeDialog != nullptr) {
		welcomeDialog->Hide();
		g_gui.root->Show();
		welcomeDialog->Destroy();
		welcomeDialog = nullptr;
	}
}

bool WelcomeManager::IsWelcomeDialogShown() {
	return welcomeDialog != nullptr && welcomeDialog->IsShown();
}

void WelcomeManager::OnWelcomeDialogClosed(wxCloseEvent& event) {
	welcomeDialog->Destroy();
	g_gui.root->Close();
}

void WelcomeManager::OnWelcomeDialogAction(wxCommandEvent& event) {
	if (event.GetId() == wxID_NEW) {
		g_gui.NewMap();
	} else if (event.GetId() == wxID_OPEN) {
		if (welcomeDialog != nullptr) {
			if (auto load_request = welcomeDialog->ConsumePendingLoadRequest()) {
				g_gui.LoadMap(FileName(load_request->map_path.ToStdWstring()), load_request->load_options);
				return;
			}
		}
		g_gui.LoadMap(FileName(event.GetString().ToStdWstring()));
	}
}
