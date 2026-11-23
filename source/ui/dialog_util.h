//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_DIALOG_UTIL_H_
#define RME_UI_DIALOG_UTIL_H_

#include <wx/wx.h>

class DialogUtil {
public:
	static long PopupDialog(wxWindow* parent, wxString title, wxString text, long style, wxString configsavename = wxEmptyString, uint32_t configsavevalue = 0);
	static long PopupDialog(wxString title, wxString text, long style, wxString configsavename = wxEmptyString, uint32_t configsavevalue = 0);

	static void ListDialog(wxWindow* parent, wxString title, const std::vector<std::string>& vec);
	static void ListDialog(const wxString& title, const std::vector<std::string>& vec) {
		ListDialog(nullptr, title, vec);
	}

	// Warnings produced while loading client data or a map. Shown in a ListDialog, or, when
	// Config::SHOW_LOAD_WARNINGS_DIALOG is off, appended to load-warnings.log next to the
	// executable so the load is not blocked.
	static void LoadWarningsDialog(const wxString& title, const std::vector<std::string>& warnings);

	static void ShowTextBox(wxWindow* parent, wxString title, wxString contents);
	static void ShowTextBox(const wxString& title, const wxString& contents) {
		ShowTextBox(nullptr, title, contents);
	}

	static void ShowRichToolTip(const wxString& title, const wxString& message, wxWindow* win, const wxRect* rect = nullptr, int icon = wxICON_INFORMATION);
};

#endif
