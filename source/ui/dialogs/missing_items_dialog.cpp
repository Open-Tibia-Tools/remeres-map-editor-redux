//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "ui/dialogs/missing_items_dialog.h"
#include "ui/dialog_util.h"

#include <format>
#include <algorithm>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/stream.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

MissingItemsDialog::MissingItemsDialog(wxWindow* parent, const MissingItemReport& report, bool hasOtb) :
	wxDialog(parent, wxID_ANY, "Missing Item Definitions Report", wxDefaultPosition, parent ? parent->FromDIP(wxSize(700, 600)) : wxSize(700, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	report(report),
	hasOtb(hasOtb),
	listMissingInDat(nullptr),
	listMissingInOtb(nullptr),
	listXmlNoOtb(nullptr),
	listOtbNoXml(nullptr),
	countMissingInDat(nullptr),
	countMissingInOtb(nullptr),
	countXmlNoOtb(nullptr),
	countOtbNoXml(nullptr) {
	BuildUI();
	CenterOnParent();
}

void MissingItemsDialog::BuildUI() {
	auto* mainSizer = new wxBoxSizer(wxVERTICAL);

	// Sort entries ascending by Server ID, then Client ID
	auto sort_entries = [](const std::vector<MissingItemEntry>& vec) {
		auto sorted = vec;
		std::sort(sorted.begin(), sorted.end(), [](const MissingItemEntry& a, const MissingItemEntry& b) {
			if (a.server_id != b.server_id) return a.server_id < b.server_id;
			return a.client_id < b.client_id;
		});
		return sorted;
	};

	auto sorted_dat = sort_entries(report.missing_in_dat);
	auto sorted_otb = sort_entries(report.missing_in_otb);
	auto sorted_xml_no_otb = sort_entries(report.xml_no_otb);
	auto sorted_otb_no_xml = sort_entries(report.otb_no_xml);

	// Header text
	wxString headerTextContent;
	if (hasOtb) {
		headerTextContent = "The following items have mismatched definitions between items.otb, tibia.dat, and items.xml.\n"
		                   "Items without definitions will be marked as invalid on the map.";
	} else {
		headerTextContent = "The following items have mismatched definitions between tibia.dat and items.xml.\n"
		                   "Items without definitions will be marked as invalid on the map.";
	}
	auto* headerText = new wxStaticText(this, wxID_ANY, headerTextContent,
		wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
	mainSizer->Add(headerText, wxSizerFlags().Expand().Border(wxALL, FromDIP(10)));

	// Create notebook for sections
	auto* notebook = new wxNotebook(this, wxID_ANY);

	// Section 1: Missing in DAT
	auto* page1 = new wxPanel(notebook, wxID_ANY);
	auto* sizer1 = new wxBoxSizer(wxVERTICAL);
	countMissingInDat = new wxStaticText(page1, wxID_ANY, "");
	sizer1->Add(countMissingInDat, wxSizerFlags().Border(wxALL, FromDIP(5)));

	constexpr int colFlags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_REORDERABLE;
	constexpr long listStyle = wxDV_ROW_LINES | wxDV_HORIZ_RULES | wxDV_VERT_RULES;

	listMissingInDat = new wxDataViewListCtrl(page1, wxID_ANY, wxDefaultPosition, wxDefaultSize, listStyle);
	listMissingInDat->AppendTextColumn("Server ID", wxDATAVIEW_CELL_INERT, FromDIP(100), wxALIGN_LEFT, colFlags);
	listMissingInDat->AppendTextColumn("Client ID", wxDATAVIEW_CELL_INERT, FromDIP(100), wxALIGN_LEFT, colFlags);
	listMissingInDat->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, FromDIP(200), wxALIGN_LEFT, colFlags);
	listMissingInDat->AppendTextColumn("Description", wxDATAVIEW_CELL_INERT, FromDIP(250), wxALIGN_LEFT, colFlags);
	sizer1->Add(listMissingInDat, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(5)));

	for (const auto& entry : sorted_dat) {
		wxVector<wxVariant> row;
		row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.server_id))));
		row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.client_id))));
		row.push_back(wxVariant(wxString::FromUTF8(entry.name.empty() ? "unknown" : entry.name)));
		row.push_back(wxVariant(wxString::FromUTF8(entry.description)));
		listMissingInDat->AppendItem(row);
	}
	countMissingInDat->SetLabelText(wxString::FromUTF8(std::format("Items in {} missing from tibia.dat: {}",
		hasOtb ? "items.otb" : "items.xml", report.missing_in_dat.size())));
	page1->SetSizer(sizer1);
	notebook->AddPage(page1, wxString::FromUTF8(std::format("Missing in DAT ({})", report.missing_in_dat.size())));

	// Section 2: Missing in OTB (DAT not in primary source)
	auto* page2 = new wxPanel(notebook, wxID_ANY);
	auto* sizer2 = new wxBoxSizer(wxVERTICAL);
	countMissingInOtb = new wxStaticText(page2, wxID_ANY, "");
	sizer2->Add(countMissingInOtb, wxSizerFlags().Border(wxALL, FromDIP(5)));

	listMissingInOtb = new wxDataViewListCtrl(page2, wxID_ANY, wxDefaultPosition, wxDefaultSize, listStyle);
	listMissingInOtb->AppendTextColumn("Client ID", wxDATAVIEW_CELL_INERT, FromDIP(150), wxALIGN_LEFT, colFlags);
	listMissingInOtb->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, FromDIP(200), wxALIGN_LEFT, colFlags);
	sizer2->Add(listMissingInOtb, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(5)));

	for (const auto& entry : sorted_otb) {
		wxVector<wxVariant> row;
		row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.client_id))));
		row.push_back(wxVariant(wxString::FromUTF8(entry.name.empty() ? "(no name)" : entry.name)));
		listMissingInOtb->AppendItem(row);
	}
	countMissingInOtb->SetLabelText(wxString::FromUTF8(std::format("Items in tibia.dat not referenced by {}: {}",
		hasOtb ? "items.otb" : "items.xml", report.missing_in_otb.size())));
	page2->SetSizer(sizer2);
	notebook->AddPage(page2, wxString::FromUTF8(std::format("DAT not in {} ({})", hasOtb ? "OTB" : "XML", report.missing_in_otb.size())));

	// Section 3: XML no OTB (only shown if hasOtb is true)
	if (hasOtb) {
		auto* page3 = new wxPanel(notebook, wxID_ANY);
		auto* sizer3 = new wxBoxSizer(wxVERTICAL);
		countXmlNoOtb = new wxStaticText(page3, wxID_ANY, "");
		sizer3->Add(countXmlNoOtb, wxSizerFlags().Border(wxALL, FromDIP(5)));

		listXmlNoOtb = new wxDataViewListCtrl(page3, wxID_ANY, wxDefaultPosition, wxDefaultSize, listStyle);
		listXmlNoOtb->AppendTextColumn("Server ID", wxDATAVIEW_CELL_INERT, FromDIP(100), wxALIGN_LEFT, colFlags);
		listXmlNoOtb->AppendTextColumn("Client ID", wxDATAVIEW_CELL_INERT, FromDIP(100), wxALIGN_LEFT, colFlags);
		listXmlNoOtb->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, FromDIP(200), wxALIGN_LEFT, colFlags);
		listXmlNoOtb->AppendTextColumn("Description", wxDATAVIEW_CELL_INERT, FromDIP(250), wxALIGN_LEFT, colFlags);
		sizer3->Add(listXmlNoOtb, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(5)));

		for (const auto& entry : sorted_xml_no_otb) {
			wxVector<wxVariant> row;
			row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.server_id))));
			row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.client_id))));
			row.push_back(wxVariant(wxString::FromUTF8(entry.name.empty() ? "unknown" : entry.name)));
			row.push_back(wxVariant(wxString::FromUTF8(entry.description)));
			listXmlNoOtb->AppendItem(row);
		}
		countXmlNoOtb->SetLabelText(wxString::FromUTF8(std::format("Items in items.xml missing from items.otb: {}", report.xml_no_otb.size())));
		page3->SetSizer(sizer3);
		notebook->AddPage(page3, wxString::FromUTF8(std::format("XML no OTB ({})", report.xml_no_otb.size())));
	}

	// Section 4: OTB no XML (only shown if hasOtb is true)
	if (hasOtb) {
		auto* page4 = new wxPanel(notebook, wxID_ANY);
		auto* sizer4 = new wxBoxSizer(wxVERTICAL);
		countOtbNoXml = new wxStaticText(page4, wxID_ANY, "");
		sizer4->Add(countOtbNoXml, wxSizerFlags().Border(wxALL, FromDIP(5)));

		listOtbNoXml = new wxDataViewListCtrl(page4, wxID_ANY, wxDefaultPosition, wxDefaultSize, listStyle);
		listOtbNoXml->AppendTextColumn("Server ID", wxDATAVIEW_CELL_INERT, FromDIP(100), wxALIGN_LEFT, colFlags);
		listOtbNoXml->AppendTextColumn("Client ID", wxDATAVIEW_CELL_INERT, FromDIP(100), wxALIGN_LEFT, colFlags);
		listOtbNoXml->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, FromDIP(200), wxALIGN_LEFT, colFlags);
		listOtbNoXml->AppendTextColumn("Description", wxDATAVIEW_CELL_INERT, FromDIP(250));
		sizer4->Add(listOtbNoXml, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(5)));

		for (const auto& entry : sorted_otb_no_xml) {
			wxVector<wxVariant> row;
			row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.server_id))));
			row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.client_id))));
			row.push_back(wxVariant(wxString::FromUTF8(entry.name.empty() ? "unknown" : entry.name)));
			row.push_back(wxVariant(wxString::FromUTF8(entry.description)));
			listOtbNoXml->AppendItem(row);
		}
		countOtbNoXml->SetLabelText(wxString::FromUTF8(std::format("Items in items.otb missing from items.xml: {}", report.otb_no_xml.size())));
		page4->SetSizer(sizer4);
		notebook->AddPage(page4, wxString::FromUTF8(std::format("OTB no XML ({})", report.otb_no_xml.size())));
	}

	mainSizer->Add(notebook, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(5)));

	// Buttons
	auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

	auto* copyButton = new wxButton(this, wxID_ANY, "Copy to Clipboard");
	copyButton->Bind(wxEVT_BUTTON, &MissingItemsDialog::OnCopyToClipboard, this);
	buttonSizer->Add(copyButton, wxSizerFlags().Border(wxALL, FromDIP(5)));

	auto* saveButton = new wxButton(this, wxID_ANY, "Save Report...");
	saveButton->Bind(wxEVT_BUTTON, &MissingItemsDialog::OnSaveReport, this);
	buttonSizer->Add(saveButton, wxSizerFlags().Border(wxALL, FromDIP(5)));

	buttonSizer->AddStretchSpacer();

	auto* ignoreButton = new wxButton(this, wxID_ANY, "Close");
	ignoreButton->SetDefault();
	ignoreButton->Bind(wxEVT_BUTTON, &MissingItemsDialog::OnIgnore, this);
	buttonSizer->Add(ignoreButton, wxSizerFlags().Border(wxALL, FromDIP(5)));

	mainSizer->Add(buttonSizer, wxSizerFlags().Expand().Border(wxTOP, FromDIP(5)));

	SetSizer(mainSizer);
	mainSizer->Fit(this);
	SetMinSize(wxSize(FromDIP(600), FromDIP(400)));
}

wxString MissingItemsDialog::GenerateReportText() const {
	wxString text;
	text += "=== Missing Item Definitions Report ===\n\n";

	if (hasOtb) {
		text += wxString::FromUTF8(std::format("--- Items in items.otb missing from tibia.dat ({}) ---\n", report.missing_in_dat.size()));
	} else {
		text += wxString::FromUTF8(std::format("--- Items in items.xml missing from tibia.dat ({}) ---\n", report.missing_in_dat.size()));
	}
	for (const auto& entry : report.missing_in_dat) {
		text += wxString::FromUTF8(std::format("Server ID: {}, Client ID: {}, Name: {}, Description: {}\n",
			entry.server_id, entry.client_id, entry.name, entry.description));
	}
	text += "\n";

	if (hasOtb) {
		text += wxString::FromUTF8(std::format("--- Items in tibia.dat missing from items.otb ({}) ---\n", report.missing_in_otb.size()));
	} else {
		text += wxString::FromUTF8(std::format("--- Items in tibia.dat not referenced by items.xml ({}) ---\n", report.missing_in_otb.size()));
	}
	for (const auto& entry : report.missing_in_otb) {
		text += wxString::FromUTF8(std::format("Client ID: {}\n", entry.client_id));
	}
	text += "\n";

	if (hasOtb) {
		text += wxString::FromUTF8(std::format("--- Items in items.xml missing from items.otb ({}) ---\n", report.xml_no_otb.size()));
		for (const auto& entry : report.xml_no_otb) {
			text += wxString::FromUTF8(std::format("Server ID: {}, Client ID: {}, Name: {}, Description: {}\n",
				entry.server_id, entry.client_id, entry.name, entry.description));
		}
		text += "\n";

		text += wxString::FromUTF8(std::format("--- Items in items.otb missing from items.xml ({}) ---\n", report.otb_no_xml.size()));
		for (const auto& entry : report.otb_no_xml) {
			text += wxString::FromUTF8(std::format("Server ID: {}, Client ID: {}, Name: {}, Description: {}\n",
				entry.server_id, entry.client_id, entry.name, entry.description));
		}
		text += "\n";
	}

	return text;
}

void MissingItemsDialog::OnCopyToClipboard(wxCommandEvent& WXUNUSED(evt)) {
	if (!wxTheClipboard->Open()) {
		DialogUtil::PopupDialog(this, "Clipboard Error",
		             "Failed to open clipboard. Another application may be using it.", wxOK | wxICON_ERROR);
		return;
	}

	wxTheClipboard->SetData(new wxTextDataObject(GenerateReportText()));
	wxTheClipboard->Close();
	DialogUtil::PopupDialog(this, "Clipboard", "Report copied to clipboard.", wxOK | wxICON_INFORMATION);
}

void MissingItemsDialog::OnSaveReport(wxCommandEvent& WXUNUSED(evt)) {
	wxFileDialog saveDlg(this, "Save Report As", "", "missing_items_report.txt",
		"Text files (*.txt)|*.txt", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

	if (saveDlg.ShowModal() == wxID_OK) {
		wxString path = saveDlg.GetPath();
		wxFileOutputStream stream(path);
		if (stream.IsOk()) {
			wxTextOutputStream txt(stream);
			txt << GenerateReportText();
			DialogUtil::PopupDialog(this, "Save Report", "Report saved successfully.", wxOK | wxICON_INFORMATION);
		} else {
			DialogUtil::PopupDialog(this, "Save Report Error", "Failed to create report file.", wxOK | wxICON_ERROR);
		}
	}
}

void MissingItemsDialog::OnIgnore(wxCommandEvent& WXUNUSED(evt)) {
	EndModal(wxID_OK);
}

void MissingItemsDialog::Show(wxWindow* parent, const MissingItemReport& report, bool hasOtb) {
	if (report.missing_in_dat.empty() && report.missing_in_otb.empty() && report.xml_no_otb.empty() && report.otb_no_xml.empty()) {
		return;
	}

	MissingItemsDialog dialog(parent, report, hasOtb);
	dialog.ShowModal();
}
