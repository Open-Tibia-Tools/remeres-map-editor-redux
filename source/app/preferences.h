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

#ifndef RME_PREFERENCES_WINDOW_H_
#define RME_PREFERENCES_WINDOW_H_

#include <wx/dialog.h>
#include <wx/listbook.h>
#include <wx/listctrl.h>

#include "app/main.h"
#include "app/preferences/client_version_page.h"
#include "app/preferences/editor_page.h"
#include "app/preferences/general_page.h"
#include "app/preferences/graphics_page.h"
#include "app/preferences/interface_page.h"
#include "app/preferences/map_compatibility_page.h"

class PreferencesWindow : public wxDialog {
public:
	PreferencesWindow(wxWindow* parent, bool clientVersionSelected = false);
	~PreferencesWindow() override;

	void OnClickApply(wxCommandEvent& event);
	void OnClickOK(wxCommandEvent& event);
	void OnClickCancel(wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);

protected:
	void Apply();

	wxListbook* book = nullptr;

	GeneralPage* general_page = nullptr;
	EditorPage* editor_page = nullptr;
	GraphicsPage* graphics_page = nullptr;
	InterfacePage* interface_page = nullptr;
	ClientVersionPage* client_version_page = nullptr;
	MapCompatibilityPage* map_compatibility_page = nullptr;
};

#endif

