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

#include "ui/theme.h"
#include "ui/dialog_util.h"
#include "app/application.h"
#include "util/file_system.h"
#include "util/system_specs.h"
#include "rendering/core/hardware_profile.h"
#include "editor/hotkey_manager.h"

#include "game/sprites.h"
#include "ui/icons/editor_icon_registry.h"
#include "util/image_manager.h"
#include "editor/editor.h"
#include "ui/dialogs/goto_position_dialog.h"
#include "palette/palette_window.h"
#include "app/preferences.h"
#include "net/net_connection.h"
#include "lua/lua_script_manager.h"
#include "lua/lua_scripts_window.h"
#include "ui/result_window.h"
#include "rendering/ui/minimap_window.h"
#include "ui/about_window.h"
#include "ui/main_menubar.h"
#include "app/updater.h"
#include <wx/stattext.h>
#include <wx/slider.h>

#include "game/materials.h"
#include "map/map.h"
#include "game/complexitem.h"
#include "game/creature.h"

#include "ingame_preview/ingame_preview_manager.h"

#include <wx/snglinst.h>
#include <wx/stdpaths.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <chrono>

wxIMPLEMENT_APP_NO_MAIN(Application);

int main(int argc, char** argv) {
	return wxEntry(argc, argv);
}

// OnRun is implemented below

Application::~Application() {
}

bool Application::OnInit() {
#if defined __DEBUG_MODE__ && defined __WINDOWS__
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	// #ifdef __WINDOWS__
	// 	// Allocate console for debug output
	// 	AllocConsole();
	// 	FILE* fp;
	// 	freopen_s(&fp, "CONOUT$", "w", stdout);
	// 	freopen_s(&fp, "CONOUT$", "w", stderr);
	// #endif

	// Load settings early for theme support
	g_settings.load();

	// Initialize hardware detection and active performance profile early
	const auto initial_specs = SystemSpecsDetector::detect(false);
	auto& profile_mgr = HardwareProfileManager::get();
	profile_mgr.initialize(initial_specs);
	profile_mgr.setProfileMode(static_cast<HardwareProfileMode>(g_settings.getInteger(Config::HARDWARE_PROFILE_MODE)));

	int rawTheme = g_settings.getInteger(Config::THEME);
	Theme::Type theme = Theme::Type::System;
	if (rawTheme >= static_cast<int>(Theme::Type::System) && rawTheme <= static_cast<int>(Theme::Type::Light)) {
		theme = static_cast<Theme::Type>(rawTheme);
	}
	Theme::setType(theme);

	// Enable modern appearance handling (wxWidgets 3.3+)
#if wxCHECK_VERSION(3, 3, 0)
	switch (theme) {
		case Theme::Type::Dark:
			SetAppearance(wxApp::Appearance::Dark);
			break;
		case Theme::Type::Light:
			SetAppearance(wxApp::Appearance::Light);
			break;
		case Theme::Type::System:
		default:
			SetAppearance(wxApp::Appearance::System);
			break;
	}
#endif

#ifdef __WXMSW__
	#if wxCHECK_VERSION(3, 3, 0)
	// Enable dark mode support for Windows
	// Note: SetAppearance() above handles this internally in newer versions,
	// but explicit calls here ensure improved behavior on some system configurations.
	switch (theme) {
		case Theme::Type::Dark:
			MSWEnableDarkMode(wxApp::DarkMode_Always);
			break;
		case Theme::Type::Light:
			// "DarkMode_Never" is not available in wxWidgets 3.3.1 API.
			// Light mode is the default on MSW, so no action is required here.
			break;
		case Theme::Type::System:
		default:
			MSWEnableDarkMode(wxApp::DarkMode_Auto);
			break;
	}
	#endif
#endif

	// Discover data directory
	FileSystem::DiscoverDataDirectory("menubar.xml");

	// Tell that we are the real thing
	wxAppConsole::SetInstance(this);

	// Load some internal stuff
	// g_settings.load(); - Already loaded above
	FixVersionDiscrapencies();
	g_hotkeys.LoadHotkeys();
	ClientVersion::loadVersions();


#ifdef _USE_PROCESS_COM
	m_single_instance_checker = newd wxSingleInstanceChecker; // Instance checker has to stay alive throughout the applications lifetime
	if (g_settings.getInteger(Config::ONLY_ONE_INSTANCE) && m_single_instance_checker->IsAnotherRunning()) {
		RMEProcessClient client;
		wxLogNull nolog; // Prevent wxWidgets popup dialog on connection failure
		wxConnectionBase* connection = client.MakeConnection("localhost", "rme_host", "rme_talk");
		if (connection) {
			wxString fileName;
			if (ParseCommandLineMap(fileName)) {
				connection->Execute(fileName);
			}
			connection->Disconnect();
			wxDELETE(connection);
			wxDELETE(m_single_instance_checker);
			return false; // Since we return false - OnExit is never called
		} else {
			// Another instance was reported running but not responding (stale lock after crash or shutdown).
			// Proceed to launch as primary instance instead of failing.
			spdlog::warn("Another instance was reported running but IPC connection failed. Continuing as new instance.");
			wxDELETE(m_single_instance_checker);
		}
	}
	// We act as server then
	m_proc_server = newd RMEProcessServer();
	if (!m_proc_server->Create("rme_host")) {
		wxLogWarning("Could not register IPC service!");
	}
#endif

	// Image handlers
	wxInitAllImageHandlers();

	EditorIconRegistry::Load();

	// Initialize Lua scripting system EARLY (before MainFrame)
	if (!g_luaScripts.initialize()) {
		spdlog::warn("Failed to initialize Lua scripting: {}", g_luaScripts.getLastError());
	}

	// wxHandleFatalExceptions(true);
	wxHandleFatalExceptions(true);
	// Load all the dependency files
	std::string error;
	StringVector warnings;

	m_file_to_open = wxEmptyString;
	ParseCommandLineMap(m_file_to_open);

	g_gui.root = newd MainFrame(__W_RME_APPLICATION_NAME__, wxDefaultPosition, wxSize(700, 500));
	SetTopWindow(g_gui.root);
	g_gui.SetTitle("");

	g_gui.root->LoadRecentFiles();

	// Load palette
	g_gui.LoadPerspective();

	wxIcon icon;
	const wxBitmap icon_bmp = IMAGE_MANAGER.GetBitmap(IMAGE_EDITOR_ICON);
	if (icon_bmp.IsOk()) {
		icon.CopyFromBitmap(icon_bmp);
		g_gui.root->SetIcon(icon);
	}

	if (g_settings.getInteger(Config::WELCOME_DIALOG) == 1 && m_file_to_open == wxEmptyString) {
		g_gui.ShowWelcomeDialog(icon);
	} else {
		g_gui.root->Show();
	}

	if (g_luaScripts.isInitialized() && g_gui.root && g_gui.root->menu_bar) {
		g_gui.root->menu_bar->LoadScriptsMenu();
	}

	// Set idle event handling mode
	wxIdleEvent::SetMode(wxIDLE_PROCESS_SPECIFIED);

	// Goto RME website?
	if (g_settings.getInteger(Config::GOTO_WEBSITE_ON_BOOT) == 1) {
		::wxLaunchDefaultBrowser(__SITE_URL__, wxBROWSER_NEW_WINDOW);
		g_settings.setInteger(Config::GOTO_WEBSITE_ON_BOOT, 0);
	}

	// Check for updates
#ifdef _USE_UPDATER_
	if (g_settings.getInteger(Config::USE_UPDATER) == -1) {
		int ret = DialogUtil::PopupDialog(
			"Notice",
			"Do you want the editor to automatically check for updates?\n"
			"It will connect to the internet if you choose yes.\n"
			"You can change this setting in the preferences later.",
			wxYES | wxNO
		);
		if (ret == wxID_YES) {
			g_settings.setInteger(Config::USE_UPDATER, 1);
		} else {
			g_settings.setInteger(Config::USE_UPDATER, 0);
		}
	}
	if (g_settings.getInteger(Config::USE_UPDATER) == 1) {
		m_updater = std::make_unique<UpdateChecker>();
		m_updater->connect(g_gui.root);
	}
#endif

	FileName save_failed_file = FileSystem::GetLocalDataDirectory();
	save_failed_file.SetName(".saving.txt");
	if (save_failed_file.FileExists()) {
		std::ifstream f(nstr(save_failed_file.GetFullPath()).c_str(), std::ios::in);

		std::string backup_otbm, backup_house, backup_spawn;

		getline(f, backup_otbm);
		getline(f, backup_house);
		getline(f, backup_spawn);

		// Remove the file
		f.close();
		std::remove(nstr(save_failed_file.GetFullPath()).c_str());

		// Query file retrieval if possible
		if (!backup_otbm.empty()) {
			long ret = DialogUtil::PopupDialog(
				"Editor Crashed",
				wxString(
					"IMPORTANT! THE EDITOR CRASHED WHILE SAVING!\n\n"
					"Do you want to recover the lost map? (it will be opened immediately):\n"
				) << wxstr(backup_otbm)
				  << "\n"
				  << wxstr(backup_house) << "\n"
				  << wxstr(backup_spawn) << "\n",
				wxYES | wxNO
			);

			if (ret == wxID_YES) {
				// Recover if the user so wishes
				std::remove(backup_otbm.substr(0, backup_otbm.size() - 1).c_str());
				std::rename(backup_otbm.c_str(), backup_otbm.substr(0, backup_otbm.size() - 1).c_str());

				if (!backup_house.empty()) {
					std::remove(backup_house.substr(0, backup_house.size() - 1).c_str());
					std::rename(backup_house.c_str(), backup_house.substr(0, backup_house.size() - 1).c_str());
				}
				if (!backup_spawn.empty()) {
					std::remove(backup_spawn.substr(0, backup_spawn.size() - 1).c_str());
					std::rename(backup_spawn.c_str(), backup_spawn.substr(0, backup_spawn.size() - 1).c_str());
				}

				// Load the map
				g_gui.LoadMap(wxstr(backup_otbm.substr(0, backup_otbm.size() - 1)));
				return true;
			}
		}
	}
	// Keep track of first event loop entry
	m_startup = true;
	return true;
}

void Application::OnEventLoopEnter(wxEventLoopBase* loop) {

	// First startup?
	if (!m_startup) {
		return;
	}
	m_startup = false;

	// Don't try to create a map if we didn't load the client map.
	if (ClientVersion::getLatestVersion() == nullptr) {
		return;
	}

	// Open a map.
	if (m_file_to_open != wxEmptyString) {
		g_gui.LoadMap(FileName(m_file_to_open));
	} else if (!g_gui.IsWelcomeDialogShown() && g_gui.NewMap()) { // Open a new empty map
		// You generally don't want to save this map...
		g_gui.GetCurrentEditor()->map.clearChanges();
	}
}

void Application::MacOpenFiles(const wxArrayString& fileNames) {
	if (!fileNames.IsEmpty()) {
		g_gui.LoadMap(FileName(fileNames.Item(0)));
	}
}

void Application::FixVersionDiscrapencies() {
	// Here the registry should be fixed, if the version has been changed
	if (g_settings.getInteger(Config::VERSION_ID) != 0 && g_settings.getInteger(Config::VERSION_ID) < MAKE_VERSION_ID(1, 0, 5)) {
		g_settings.setInteger(Config::USE_MEMCACHED_SPRITES_TO_SAVE, 0);
	}

	if (g_settings.getInteger(Config::VERSION_ID) < __RME_VERSION_ID__ && ClientVersion::getLatestVersion() != nullptr) {
		g_settings.setInteger(Config::DEFAULT_CLIENT_VERSION, ClientVersion::getLatestVersion()->getProtocolID());
	}

	wxString ss = wxstr(g_settings.getString(Config::SCREENSHOT_DIRECTORY));
	if (ss.empty()) {
		wxFileName fn(wxStandardPaths::Get().GetDocumentsDir(), "");
		if (fn.GetDirCount() > 0) {
			fn.RemoveLastDir(); // Move from "Documents" to user root
		}
		fn.AppendDir("Pictures");
		fn.AppendDir("RME");

		if (!fn.DirExists()) {
			fn.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
		}
		ss = fn.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
	}
	g_settings.setString(Config::SCREENSHOT_DIRECTORY, nstr(ss));

	// Set registry to newest version
	g_settings.setInteger(Config::VERSION_ID, __RME_VERSION_ID__);
}

void Application::Unload() {
#ifdef _USE_UPDATER_
	m_updater.reset();
#endif

	g_gui.CloseAllEditors();
	g_version.UnloadVersion();
	g_hotkeys.SaveHotkeys();
	g_gui.root->SaveRecentFiles();
	ClientVersion::saveVersions();
	ClientVersion::unloadVersions();
	g_settings.save(true);

	NetworkConnection::getInstance().stop();

	g_preview.Destroy();

	g_gui.root = nullptr;
}

int Application::OnExit() {
	// Shutdown Lua scripting system
	g_luaScripts.shutdown();

#ifdef _USE_PROCESS_COM
	wxDELETE(m_proc_server);
	wxDELETE(m_single_instance_checker);
#endif
	return 0;
}

int Application::OnRun() {
	int ret = -1;
	try {
		ret = wxApp::OnRun();
	} catch (const std::exception& e) {
		spdlog::error("Application::OnRun - Caught std::exception: {}", e.what());
		spdlog::default_logger()->flush();
	} catch (...) {
		spdlog::error("Application::OnRun - Caught unknown exception");
		spdlog::default_logger()->flush();
	}
	return ret;
}

void Application::OnFatalException() {
}

bool Application::OnExceptionInMainLoop() {
	try {
		throw;
	} catch (const std::exception& e) {
		spdlog::error("Application::OnExceptionInMainLoop - Caught std::exception: {}", e.what());
		spdlog::default_logger()->flush();
	} catch (...) {
		spdlog::error("Application::OnExceptionInMainLoop - Caught unknown exception");
		spdlog::default_logger()->flush();
	}
	return true; // Continue running if possible, or false to abort
}

void Application::OnUnhandledException() {
	try {
		throw;
	} catch (const std::exception& e) {
		spdlog::error("Application::OnUnhandledException - Caught std::exception: {}", e.what());
		spdlog::default_logger()->flush();
	} catch (...) {
		spdlog::error("Application::OnUnhandledException - Caught unknown exception");
		spdlog::default_logger()->flush();
	}
}

bool Application::ParseCommandLineMap(wxString& fileName) {
	if (argc == 2) {
		fileName = wxString(argv[1]);
		return true;
	} else if (argc == 3) {
		if (argv[1] == "-ws") {
			g_settings.setInteger(Config::WELCOME_DIALOG, argv[2] == "1" ? 1 : 0);
		}
	}
	return false;
}
