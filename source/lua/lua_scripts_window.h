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

#ifndef RME_LUA_SCRIPTS_WINDOW_H
#define RME_LUA_SCRIPTS_WINDOW_H

#include <wx/aui/auibar.h>
#include <wx/dataview.h>
#include <wx/icon.h>
#include <wx/panel.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/stc/stc.h>

#include <optional>
#include <string>

class LuaScriptsWindow : public wxPanel {
public:
	LuaScriptsWindow(wxWindow* parent);
	~LuaScriptsWindow() override;

	void RefreshScriptList();
	void LogMessage(const wxString& message, bool isError = false);
	void ClearConsole();

	static LuaScriptsWindow* Get() {
		return instance;
	}
	static void SetInstance(LuaScriptsWindow* win) {
		instance = win;
	}

private:
	void BuildUI();
	void ApplyTheme();
	void RefreshSelectionSummary();
	void UpdateActionState();
	void SelectScriptById(const std::string& uniqueId);
	std::optional<size_t> GetSelectedScriptIndex() const;
	void RunSelectedScript();
	void DisableSelectedScript();
	void RemoveSelectedScript();
	void OpenSelectedScript();
	void RevealSelectedScript();

	void OnSelectionChanged(wxDataViewEvent& event);
	void OnItemActivated(wxDataViewEvent& event);
	void OnItemValueChanged(wxDataViewEvent& event);
	void OnItemContextMenu(wxDataViewEvent& event);
	void OnReloadScripts(wxCommandEvent& event);
	void OnOpenFolder(wxCommandEvent& event);
	void OnClearConsole(wxCommandEvent& event);
	void OnCopyConsole(wxCommandEvent& event);
	void OnRunScript(wxCommandEvent& event);
	void OnDisableScript(wxCommandEvent& event);
	void OnEditOpen(wxCommandEvent& event);
	void OnReveal(wxCommandEvent& event);
	void OnRemoveScript(wxCommandEvent& event);

	wxSplitterWindow* main_splitter = nullptr;
	wxAuiToolBar* action_toolbar = nullptr;
	wxAuiToolBar* output_toolbar = nullptr;
	wxDataViewListCtrl* script_list = nullptr;
	wxStaticText* selection_summary = nullptr;
	wxStyledTextCtrl* console_output = nullptr;
	wxIcon file_script_icon;
	wxIcon package_script_icon;
	std::string selected_script_id;
	bool refreshing_list = false;

	static LuaScriptsWindow* instance;
};

#endif // RME_LUA_SCRIPTS_WINDOW_H
