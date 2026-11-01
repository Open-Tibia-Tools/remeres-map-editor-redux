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
#include "lua_script_manager.h"
#include "lua_scripts_window.h"
#include <format>
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/theme.h"
#include "util/image_manager.h"

#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dir.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>

#include <filesystem>

namespace {
wxString BuildScriptLabel(const LuaScript& script) {
	wxString label = wxString::FromUTF8(script.getDisplayName());
	label += script.isPackage() ? " [pkg]" : " [file]";
	return label;
}

wxIcon MakeTintedIcon(std::string_view assetPath, wxWindow* window) {
	const wxBitmap bitmap = IMAGE_MANAGER.GetBitmap(
		assetPath,
		wxWindow::FromDIP(wxSize(16, 16), window),
		Theme::Get(Theme::Role::TextSubtle)
	);

	wxIcon icon;
	icon.CopyFromBitmap(bitmap);
	return icon;
}
}

LuaScriptsWindow* LuaScriptsWindow::instance = nullptr;

LuaScriptsWindow::LuaScriptsWindow(wxWindow* parent) :
	wxPanel(parent, wxID_ANY) {
	instance = this;
	BuildUI();
	ApplyTheme();
	RefreshScriptList();

	g_luaScripts.setOutputCallback([this](const std::string& msg, bool isError) {
		if (wxThread::IsMain()) {
			LogMessage(wxString::FromUTF8(msg), isError);
			return;
		}

		wxTheApp->CallAfter([this, msg, isError]() {
			if (instance == this) {
				LogMessage(wxString::FromUTF8(msg), isError);
			}
		});
	});
}

LuaScriptsWindow::~LuaScriptsWindow() {
	g_luaScripts.setOutputCallback(nullptr);
	if (instance == this) {
		instance = nullptr;
	}
}

void LuaScriptsWindow::BuildUI() {
	const int padding = Theme::Grid(2);
	const wxSize iconSize = wxWindow::FromDIP(wxSize(16, 16), this);
	const long toolbarStyle = (wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_OVERFLOW | wxAUI_TB_PLAIN_BACKGROUND) & ~wxAUI_TB_GRIPPER;

	file_script_icon = MakeTintedIcon(ICON_FILE_PEN, this);
	package_script_icon = MakeTintedIcon(ICON_FOLDER_OPEN, this);

	auto* mainSizer = new wxBoxSizer(wxVERTICAL);

	action_toolbar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, toolbarStyle);
	action_toolbar->SetToolBitmapSize(iconSize);

	const auto addTool = [this, iconSize](int id, std::string_view iconPath, const wxString& shortHelp, const wxString& longHelp) {
		action_toolbar->AddTool(
			id,
			wxEmptyString,
			IMAGE_MANAGER.GetBitmap(iconPath, iconSize, Theme::Get(Theme::Role::Text)),
			wxNullBitmap,
			wxITEM_NORMAL,
			shortHelp,
			longHelp,
			nullptr
		);
	};

	addTool(SCRIPT_MANAGER_RELOAD, ICON_ROTATE, "Reload scripts", "Reload all discovered scripts");
	addTool(SCRIPT_MANAGER_OPEN_FOLDER, ICON_FOLDER_OPEN, "Open scripts folder", "Open the scripts folder");
	addTool(SCRIPT_MANAGER_RUN_SCRIPT, ICON_PLAY, "Run selected script", "Run the selected enabled script");
	addTool(SCRIPT_MANAGER_DISABLE, ICON_MINUS, "Disable selected script", "Disable the selected script");
	addTool(SCRIPT_MANAGER_EDIT_OPEN, ICON_PEN_TO_SQUARE, "Edit or open selected script", "Open the selected script in the default editor");
	addTool(SCRIPT_MANAGER_REVEAL, ICON_SQUARE_ARROW_UP_RIGHT, "Reveal selected script", "Open the selected script folder");
	addTool(SCRIPT_MANAGER_REMOVE, ICON_TRASH_CAN, "Remove selected script", "Delete the selected script from disk");
	action_toolbar->Realize();
	mainSizer->Add(action_toolbar, wxSizerFlags().Expand().Border(wxALL, padding));

	main_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_BORDER);
	main_splitter->SetMinimumPaneSize(Theme::Grid(20));

	auto* listPanel = new wxPanel(main_splitter);
	auto* listSizer = new wxBoxSizer(wxVERTICAL);
	script_list = new wxDataViewListCtrl(listPanel, SCRIPT_MANAGER_LIST, wxDefaultPosition, wxDefaultSize, wxDV_SINGLE);
	script_list->AppendToggleColumn("On", wxDATAVIEW_CELL_ACTIVATABLE, Theme::Grid(10), wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE);
	script_list->AppendIconTextColumn("Script", wxDATAVIEW_CELL_INERT, Theme::Grid(64), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
	listSizer->Add(script_list, wxSizerFlags(1).Expand());
	listPanel->SetSizer(listSizer);

	auto* outputPanel = new wxPanel(main_splitter);
	auto* outputSizer = new wxStaticBoxSizer(wxVERTICAL, outputPanel, "Session Output");
	auto* outputHeaderSizer = new wxBoxSizer(wxHORIZONTAL);

	selection_summary = new wxStaticText(outputSizer->GetStaticBox(), wxID_ANY, "No script selected");
	selection_summary->SetFont(Theme::GetFont(9, true));
	outputHeaderSizer->Add(selection_summary, wxSizerFlags(1).CenterVertical().Border(wxRIGHT, padding));

	output_toolbar = new wxAuiToolBar(outputSizer->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, toolbarStyle);
	output_toolbar->SetToolBitmapSize(iconSize);
	output_toolbar->AddTool(
		SCRIPT_MANAGER_COPY_OUTPUT,
		wxEmptyString,
		IMAGE_MANAGER.GetBitmap(ICON_COPY, iconSize, Theme::Get(Theme::Role::Text)),
		wxNullBitmap,
		wxITEM_NORMAL,
		"Copy output",
		"Copy the session output to the clipboard",
		nullptr
	);
	output_toolbar->AddTool(
		SCRIPT_MANAGER_CLEAR_CONSOLE,
		wxEmptyString,
		IMAGE_MANAGER.GetBitmap(ICON_TRASH_CAN, iconSize, Theme::Get(Theme::Role::Text)),
		wxNullBitmap,
		wxITEM_NORMAL,
		"Clear output",
		"Clear the session output",
		nullptr
	);
	output_toolbar->Realize();
	outputHeaderSizer->Add(output_toolbar, wxSizerFlags().CenterVertical());
	outputSizer->Add(outputHeaderSizer, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, padding));

	console_output = new wxTextCtrl(
		outputSizer->GetStaticBox(),
		wxID_ANY,
		wxEmptyString,
		wxDefaultPosition,
		wxDefaultSize,
		wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxHSCROLL
	);
	console_output->SetFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE));
	console_output->SetMinSize(wxSize(-1, Theme::Grid(28)));
	outputSizer->Add(console_output, wxSizerFlags(1).Expand().Border(wxALL, padding));

	outputPanel->SetSizer(outputSizer);

	main_splitter->SplitHorizontally(listPanel, outputPanel, -Theme::Grid(36));
	main_splitter->SetSashGravity(1.0);
	mainSizer->Add(main_splitter, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, padding));

	SetSizer(mainSizer);

	script_list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &LuaScriptsWindow::OnSelectionChanged, this);
	script_list->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, &LuaScriptsWindow::OnItemValueChanged, this);
	script_list->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &LuaScriptsWindow::OnItemContextMenu, this);

	Bind(wxEVT_MENU, &LuaScriptsWindow::OnReloadScripts, this, SCRIPT_MANAGER_RELOAD);
	Bind(wxEVT_MENU, &LuaScriptsWindow::OnOpenFolder, this, SCRIPT_MANAGER_OPEN_FOLDER);
	Bind(wxEVT_MENU, &LuaScriptsWindow::OnRunScript, this, SCRIPT_MANAGER_RUN_SCRIPT);
	Bind(wxEVT_MENU, &LuaScriptsWindow::OnDisableScript, this, SCRIPT_MANAGER_DISABLE);
	Bind(wxEVT_MENU, &LuaScriptsWindow::OnEditOpen, this, SCRIPT_MANAGER_EDIT_OPEN);
	Bind(wxEVT_MENU, &LuaScriptsWindow::OnReveal, this, SCRIPT_MANAGER_REVEAL);
	Bind(wxEVT_MENU, &LuaScriptsWindow::OnRemoveScript, this, SCRIPT_MANAGER_REMOVE);
	Bind(wxEVT_MENU, &LuaScriptsWindow::OnCopyConsole, this, SCRIPT_MANAGER_COPY_OUTPUT);
	Bind(wxEVT_MENU, &LuaScriptsWindow::OnClearConsole, this, SCRIPT_MANAGER_CLEAR_CONSOLE);

	UpdateActionState();
}

void LuaScriptsWindow::ApplyTheme() {
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	SetForegroundColour(Theme::Get(Theme::Role::Text));

	if (action_toolbar) {
		action_toolbar->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
		action_toolbar->SetForegroundColour(Theme::Get(Theme::Role::Text));
	}
	if (output_toolbar) {
		output_toolbar->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
		output_toolbar->SetForegroundColour(Theme::Get(Theme::Role::Text));
	}
	if (script_list) {
		script_list->SetBackgroundColour(Theme::Get(Theme::Role::Background));
		script_list->SetForegroundColour(Theme::Get(Theme::Role::Text));
		script_list->SetAlternateRowColour(Theme::Get(Theme::Role::Background));
	}
	if (selection_summary) {
		selection_summary->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	}
	if (console_output) {
		console_output->SetBackgroundColour(Theme::Get(Theme::Role::Background));
		console_output->SetForegroundColour(Theme::Get(Theme::Role::Text));
	}
}

void LuaScriptsWindow::RefreshScriptList() {
	if (!script_list) {
		return;
	}

	refreshing_list = true;
	script_list->Freeze();
	script_list->DeleteAllItems();

	const auto& scripts = g_luaScripts.getScripts();
	for (size_t index = 0; index < scripts.size(); ++index) {
		const auto& script = scripts[index];
		wxVector<wxVariant> row;
		const wxIcon& icon = script->isPackage() ? package_script_icon : file_script_icon;

		row.push_back(wxVariant(script->isEnabled()));
		row.push_back(wxVariant(wxDataViewIconText(BuildScriptLabel(*script), icon)));
		script_list->AppendItem(row, static_cast<wxUIntPtr>(index));
	}

	script_list->Thaw();
	refreshing_list = false;

	ApplyTheme();
	SelectScriptById(selected_script_id);
	RefreshSelectionSummary();
	UpdateActionState();
}

void LuaScriptsWindow::SelectScriptById(const std::string& uniqueId) {
	if (!script_list || uniqueId.empty()) {
		return;
	}

	const auto& scripts = g_luaScripts.getScripts();
	const unsigned itemCount = script_list->GetItemCount();
	for (unsigned row = 0; row < itemCount; ++row) {
		const wxDataViewItem item = script_list->RowToItem(row);
		if (!item.IsOk()) {
			continue;
		}

		const size_t scriptIndex = static_cast<size_t>(script_list->GetItemData(item));
		if (scriptIndex < scripts.size() && scripts[scriptIndex]->getUniqueId() == uniqueId) {
			script_list->Select(item);
			return;
		}
	}
}

std::optional<size_t> LuaScriptsWindow::GetSelectedScriptIndex() const {
	if (!script_list) {
		return std::nullopt;
	}

	const wxDataViewItem item = script_list->GetSelection();
	if (!item.IsOk()) {
		return std::nullopt;
	}

	const size_t scriptIndex = static_cast<size_t>(script_list->GetItemData(item));
	const auto& scripts = g_luaScripts.getScripts();
	if (scriptIndex >= scripts.size()) {
		return std::nullopt;
	}

	return scriptIndex;
}

void LuaScriptsWindow::RefreshSelectionSummary() {
	if (!selection_summary) {
		return;
	}

	const auto selected = GetSelectedScriptIndex();
	const auto& scripts = g_luaScripts.getScripts();
	if (!selected || *selected >= scripts.size()) {
		selected_script_id.clear();
		selection_summary->SetLabelText("No script selected");
		return;
	}

	const auto& script = scripts[*selected];
	selected_script_id = script->getUniqueId();
	selection_summary->SetLabelText(BuildScriptLabel(*script));
}

void LuaScriptsWindow::UpdateActionState() {
	const auto selected = GetSelectedScriptIndex();
	const auto& scripts = g_luaScripts.getScripts();
	const bool hasSelection = selected && *selected < scripts.size();
	const bool canRun = hasSelection && scripts[*selected]->isEnabled();
	const bool canDisable = hasSelection && scripts[*selected]->isEnabled();
	const bool hasOutput = console_output && console_output->GetLastPosition() > 0;

	if (action_toolbar) {
		action_toolbar->EnableTool(SCRIPT_MANAGER_RUN_SCRIPT, canRun);
		action_toolbar->EnableTool(SCRIPT_MANAGER_DISABLE, canDisable);
		action_toolbar->EnableTool(SCRIPT_MANAGER_EDIT_OPEN, hasSelection);
		action_toolbar->EnableTool(SCRIPT_MANAGER_REVEAL, hasSelection);
		action_toolbar->EnableTool(SCRIPT_MANAGER_REMOVE, hasSelection);
		action_toolbar->Refresh();
	}
	if (output_toolbar) {
		output_toolbar->EnableTool(SCRIPT_MANAGER_COPY_OUTPUT, hasOutput);
		output_toolbar->EnableTool(SCRIPT_MANAGER_CLEAR_CONSOLE, hasOutput);
		output_toolbar->Refresh();
	}
}

void LuaScriptsWindow::LogMessage(const wxString& message, bool isError) {
	if (!console_output) {
		return;
	}

	wxTextAttr attr;
	attr.SetTextColour(isError ? Theme::Get(Theme::Role::Error) : Theme::Get(Theme::Role::Text));
	console_output->SetDefaultStyle(attr);

	const wxString timestamp = wxDateTime::Now().Format("[%H:%M:%S] ");
	console_output->AppendText(timestamp + message);
	if (!message.EndsWith("\n")) {
		console_output->AppendText("\n");
	}
	console_output->ShowPosition(console_output->GetLastPosition());
	UpdateActionState();
}

void LuaScriptsWindow::ClearConsole() {
	if (console_output) {
		console_output->Clear();
	}
	UpdateActionState();
}

void LuaScriptsWindow::RunSelectedScript() {
	const auto selected = GetSelectedScriptIndex();
	const auto& scripts = g_luaScripts.getScripts();
	if (!selected || *selected >= scripts.size()) {
		return;
	}

	const auto& script = scripts[*selected];
	if (!script->isEnabled()) {
		LogMessage("Script is disabled: " + wxString::FromUTF8(script->getDisplayName()), true);
		return;
	}

	LogMessage("Running: " + wxString::FromUTF8(script->getDisplayName()));
	std::string error;
	if (!g_luaScripts.executeScript(*selected, error)) {
		LogMessage("Error: " + wxString::FromUTF8(error), true);
	}
}

void LuaScriptsWindow::DisableSelectedScript() {
	const auto selected = GetSelectedScriptIndex();
	const auto& scripts = g_luaScripts.getScripts();
	if (!selected || *selected >= scripts.size()) {
		return;
	}

	if (!scripts[*selected]->isEnabled()) {
		return;
	}

	g_luaScripts.setScriptEnabled(*selected, false);
	g_gui.UpdateMenubar();
	RefreshScriptList();
}

void LuaScriptsWindow::RemoveSelectedScript() {
	const auto selected = GetSelectedScriptIndex();
	const auto& scripts = g_luaScripts.getScripts();
	if (!selected || *selected >= scripts.size()) {
		return;
	}

	const auto& script = scripts[*selected];
	const wxString scriptName = wxString::FromUTF8(script->getDisplayName());
	const wxString targetKind = script->isPackage() ? "script package" : "script file";
	const wxString message = script->isPackage()
		? wxstr(std::format("Delete \"{}\" and its entire script folder?", script->getDisplayName()))
		: wxstr(std::format("Delete \"{}\" from the scripts folder?", script->getDisplayName()));

	wxMessageDialog confirm(this, message, "Remove Script", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
	if (confirm.ShowModal() != wxID_YES) {
		return;
	}

	const wxString targetPathString = script->isPackage()
		? wxString::FromUTF8(script->getDirectory())
		: wxString::FromUTF8(script->getFilePath());
	const std::filesystem::path targetPath(targetPathString.ToStdWstring());

	std::error_code errorCode;
	bool removed = false;
	if (script->isPackage()) {
		removed = std::filesystem::remove_all(targetPath, errorCode) > 0;
	} else {
		removed = std::filesystem::remove(targetPath, errorCode);
	}

	if (errorCode || !removed) {
		LogMessage(
			wxstr(std::format(
				"Failed to remove {}: {}",
				targetKind.ToStdString(),
				errorCode ? errorCode.message() : "no filesystem changes were made"
			)),
			true
		);
		return;
	}

	selected_script_id.clear();
	LogMessage(wxstr(std::format("Removed {}: {}", targetKind.ToStdString(), script->getDisplayName())));
	g_luaScripts.reloadScripts();
	g_gui.UpdateMenubar();
	RefreshScriptList();
}

void LuaScriptsWindow::OpenSelectedScript() {
	const auto selected = GetSelectedScriptIndex();
	const auto& scripts = g_luaScripts.getScripts();
	if (!selected || *selected >= scripts.size()) {
		return;
	}

	wxLaunchDefaultApplication(wxString::FromUTF8(scripts[*selected]->getFilePath()));
}

void LuaScriptsWindow::RevealSelectedScript() {
	const auto selected = GetSelectedScriptIndex();
	const auto& scripts = g_luaScripts.getScripts();
	if (!selected || *selected >= scripts.size()) {
		return;
	}

	wxLaunchDefaultApplication(wxString::FromUTF8(scripts[*selected]->getDirectory()));
}

void LuaScriptsWindow::OnSelectionChanged(wxDataViewEvent&) {
	RefreshSelectionSummary();
	UpdateActionState();
}

void LuaScriptsWindow::OnItemActivated(wxDataViewEvent&) {
}

void LuaScriptsWindow::OnItemValueChanged(wxDataViewEvent& event) {
	if (refreshing_list || event.GetColumn() != 0 || !event.GetItem().IsOk()) {
		return;
	}

	const size_t scriptIndex = static_cast<size_t>(script_list->GetItemData(event.GetItem()));
	const auto& scripts = g_luaScripts.getScripts();
	if (scriptIndex >= scripts.size()) {
		return;
	}

	const int row = script_list->ItemToRow(event.GetItem());
	if (row == wxNOT_FOUND) {
		return;
	}

	wxVariant value;
	script_list->GetValue(value, static_cast<unsigned int>(row), 0);
	g_luaScripts.setScriptEnabled(scriptIndex, value.GetBool());
	g_gui.UpdateMenubar();
	RefreshScriptList();
}

void LuaScriptsWindow::OnItemContextMenu(wxDataViewEvent& event) {
	if (!event.GetItem().IsOk()) {
		return;
	}

	script_list->Select(event.GetItem());
	RefreshSelectionSummary();
	UpdateActionState();

	const auto selected = GetSelectedScriptIndex();
	const auto& scripts = g_luaScripts.getScripts();
	const bool hasSelection = selected && *selected < scripts.size();
	const bool canRun = hasSelection && scripts[*selected]->isEnabled();
	const bool canDisable = hasSelection && scripts[*selected]->isEnabled();

	wxMenu menu;
	wxMenuItem* runItem = menu.Append(SCRIPT_MANAGER_RUN_SCRIPT, "Run");
	wxMenuItem* disableItem = menu.Append(SCRIPT_MANAGER_DISABLE, "Disable");
	wxMenuItem* editItem = menu.Append(SCRIPT_MANAGER_EDIT_OPEN, "Edit/Open");
	wxMenuItem* revealItem = menu.Append(SCRIPT_MANAGER_REVEAL, "Reveal");
	menu.AppendSeparator();
	wxMenuItem* removeItem = menu.Append(SCRIPT_MANAGER_REMOVE, "Remove");

	if (runItem) {
		runItem->Enable(canRun);
	}
	if (disableItem) {
		disableItem->Enable(canDisable);
	}
	if (editItem) {
		editItem->Enable(hasSelection);
	}
	if (revealItem) {
		revealItem->Enable(hasSelection);
	}
	if (removeItem) {
		removeItem->Enable(hasSelection);
	}

	PopupMenu(&menu);
}

void LuaScriptsWindow::OnReloadScripts(wxCommandEvent&) {
	LogMessage("Reloading scripts...");
	g_luaScripts.reloadScripts();
	g_gui.UpdateMenubar();
	RefreshScriptList();
	LogMessage(wxstr(std::format("Scripts reloaded. Found {} scripts.", g_luaScripts.getScripts().size())));
}

void LuaScriptsWindow::OnOpenFolder(wxCommandEvent&) {
	wxString scriptsPath = g_luaScripts.getScriptsDirectory();
	if (!wxDirExists(scriptsPath)) {
		wxMkdir(scriptsPath);
	}

	wxLaunchDefaultApplication(scriptsPath);
}

void LuaScriptsWindow::OnClearConsole(wxCommandEvent&) {
	ClearConsole();
}

void LuaScriptsWindow::OnCopyConsole(wxCommandEvent&) {
	if (!console_output || console_output->IsEmpty()) {
		return;
	}

	wxClipboardLocker locker(wxTheClipboard);
	if (!locker) {
		LogMessage("Could not access the clipboard.", true);
		return;
	}

	wxTheClipboard->SetData(new wxTextDataObject(console_output->GetValue()));
}

void LuaScriptsWindow::OnRunScript(wxCommandEvent&) {
	RunSelectedScript();
}

void LuaScriptsWindow::OnDisableScript(wxCommandEvent&) {
	DisableSelectedScript();
}

void LuaScriptsWindow::OnEditOpen(wxCommandEvent&) {
	OpenSelectedScript();
}

void LuaScriptsWindow::OnReveal(wxCommandEvent&) {
	RevealSelectedScript();
}

void LuaScriptsWindow::OnRemoveScript(wxCommandEvent&) {
	RemoveSelectedScript();
}
