//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/dialog_util.h"
#include <wx/app.h>
#include "ui/gui.h"
#include "util/image_manager.h"

#include "app/application.h"

#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/stattext.h>

#include <memory>

namespace {
// The dialogs receive lines that may still contain embedded newlines; split
// them so every entry is one line the user can select and copy.
std::vector<wxString> SplitDialogItems(std::vector<std::string> items) {
	std::vector<wxString> lines;
	for (size_t i = 0; i < items.size();) {
		wxString str = items[i];
		if (const size_t pos = str.find('\n'); pos != wxString::npos) {
			wxString line = str.substr(0, pos);
			if (line.EndsWith("\r")) {
				// CRLF input would otherwise keep a stray carriage return that
				// the join re-adds, producing "\r\r\n" in copied/exported text.
				line.RemoveLast();
			}
			lines.push_back(line);
			items[i] = str.substr(pos + 1).ToStdString();
			continue;
		}
		lines.push_back(str);
		++i;
	}
	return lines;
}

wxString JoinDialogItems(const std::vector<wxString>& lines) {
	wxString joined;
	for (const wxString& line : lines) {
		joined += line;
		joined += "\r\n";
	}
	return joined;
}
} // namespace

long DialogUtil::PopupDialog(wxWindow* parent, wxString title, wxString text, long style, wxString configsavename, uint32_t configsavevalue) {
	if (text.empty()) {
		return wxID_ANY;
	}

	wxMessageDialog dlg(parent, text, title, style);
	return dlg.ShowModal();
}

long DialogUtil::PopupDialog(wxString title, wxString text, long style, wxString configsavename, uint32_t configsavevalue) {
	// Need access to main frame for parent.
	return PopupDialog(static_cast<wxApp*>(wxTheApp)->GetTopWindow(), title, text, style, configsavename, configsavevalue);
}

void DialogUtil::ListDialog(wxWindow* parent, wxString title, const std::vector<std::string>& param_items) {
	if (param_items.empty()) {
		return;
	}

	auto lines = std::make_shared<std::vector<wxString>>(SplitDialogItems(param_items));
	if (lines->empty()) {
		return;
	}

	// Create the window
	wxDialog* dlg = newd wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxCAPTION | wxCLOSE_BOX);

	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);
	wxListBox* item_list = newd wxListBox(dlg, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_EXTENDED);
	item_list->SetMinSize(dlg->FromDIP(wxSize(500, 300)));

	for (const wxString& line : *lines) {
		item_list->Append(line);
	}
	sizer->Add(item_list, 1, wxEXPAND);

	wxStaticText* feedback = newd wxStaticText(dlg, wxID_ANY, "Ctrl+C copies the selection, or every line when nothing is selected.");
	sizer->Add(feedback, 0, wxLEFT | wxRIGHT | wxTOP, dlg->FromDIP(4));

	// Copy helpers: Selected copies only what is selected (that is the button
	// name), Ctrl+C falls back to every line when the selection is empty.
	auto selected_lines = [item_list]() {
		std::vector<wxString> picked;
		for (unsigned int i = 0; i < item_list->GetCount(); ++i) {
			if (item_list->IsSelected(i)) {
				picked.push_back(item_list->GetString(i));
			}
		}
		return picked;
	};

	auto copy_lines = [feedback](const std::vector<wxString>& picked) {
		if (picked.empty()) {
			feedback->SetLabel("Nothing to copy.");
			return;
		}
		if (!wxTheClipboard->Open()) {
			feedback->SetLabel("Could not open the clipboard.");
			return;
		}
		const bool stored = wxTheClipboard->SetData(new wxTextDataObject(JoinDialogItems(picked)));
		wxTheClipboard->Close();
		if (!stored) {
			feedback->SetLabel("Could not put the text in the clipboard.");
			return;
		}
		feedback->SetLabel(wxString::Format("Copied %zu line(s) to the clipboard.", picked.size()));
	};

	wxSizer* action_sizer = newd wxBoxSizer(wxHORIZONTAL);

	wxButton* copy_selected = newd wxButton(dlg, wxID_ANY, "Copy selected");
	copy_selected->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_COPY));
	copy_selected->Bind(wxEVT_BUTTON, [copy_lines, selected_lines](wxCommandEvent&) {
		copy_lines(selected_lines());
	});
	action_sizer->Add(copy_selected, wxSizerFlags(1).Expand().Border(wxRIGHT, 4));

	wxButton* copy_all = newd wxButton(dlg, wxID_ANY, "Copy all");
	copy_all->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_COPY));
	copy_all->Bind(wxEVT_BUTTON, [copy_lines, lines](wxCommandEvent&) {
		copy_lines(*lines);
	});
	action_sizer->Add(copy_all, wxSizerFlags(1).Expand().Border(wxRIGHT, 4));

	wxButton* export_button = newd wxButton(dlg, wxID_ANY, "Export...");
	export_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_SAVE));
	export_button->Bind(wxEVT_BUTTON, [dlg, lines, title, feedback](wxCommandEvent&) {
		wxFileDialog save_dlg(dlg, "Export list", wxEmptyString, title + ".txt", "Text file (*.txt)|*.txt|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (save_dlg.ShowModal() != wxID_OK) {
			return;
		}

		// wxTempFile replaces the destination only on Commit, so a failed write
		// cannot truncate the file the user chose to overwrite.
		wxTempFile file(save_dlg.GetPath());
		if (!file.IsOpened()) {
			feedback->SetLabel("Could not open the file for writing.");
			return;
		}

		const wxScopedCharBuffer utf8 = JoinDialogItems(*lines).utf8_str();
		if (file.Write(utf8.data(), utf8.length()) != utf8.length() || !file.Commit()) {
			feedback->SetLabel("The file could not be written; the previous file is untouched.");
			return;
		}
		feedback->SetLabel(wxString::Format("Exported %zu line(s).", lines->size()));
	});
	action_sizer->Add(export_button, wxSizerFlags(1).Expand().Border(wxRIGHT, 4));

	wxButton* okBtn = newd wxButton(dlg, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	action_sizer->Add(okBtn, wxSizerFlags(1).Expand());

	sizer->Add(action_sizer, 0, wxEXPAND | wxALL, dlg->FromDIP(4));

	dlg->Bind(wxEVT_CHAR_HOOK, [copy_lines, selected_lines, lines](wxKeyEvent& event) {
		if (event.GetKeyCode() == 'C' && event.ControlDown()) {
			std::vector<wxString> picked = selected_lines();
			if (picked.empty()) {
				picked = *lines;
			}
			copy_lines(picked);
			return;
		}
		event.Skip();
	});

	dlg->SetSizerAndFit(sizer);

	// Show the window
	dlg->ShowModal();
	dlg->Destroy();
}

void DialogUtil::ShowTextBox(wxWindow* parent, wxString title, wxString content) {
	wxDialog* dlg = newd wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxCAPTION | wxCLOSE_BOX);
	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);
	wxTextCtrl* text_field = newd wxTextCtrl(dlg, wxID_ANY, content, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
	text_field->SetMinSize(dlg->FromDIP(wxSize(400, 550)));
	topsizer->Add(text_field, wxSizerFlags(5).Expand());

	wxSizer* choicesizer = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* okBtn = newd wxButton(dlg, wxID_CANCEL, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	choicesizer->Add(okBtn, wxSizerFlags(1).Center());
	topsizer->Add(choicesizer, wxSizerFlags(0).Center());
	dlg->SetSizerAndFit(topsizer);

	dlg->ShowModal();
	dlg->Destroy();
}
