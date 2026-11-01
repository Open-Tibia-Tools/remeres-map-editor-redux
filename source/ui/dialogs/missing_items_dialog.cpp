//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "ui/dialogs/missing_items_dialog.h"

#include <format>
#include <algorithm>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/stream.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

MissingItemsDialog::MissingItemsDialog(wxWindow* parent, const MissingItemReport& report, bool hasOtb) :
	wxDialog(parent, wxID_ANY, "Missing Item Definitions Report", wxDefaultPosition, wxSize(700, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
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
	auto* mainSizer = newd wxBoxSizer(wxVERTICAL);

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
	auto* headerText = newd wxStaticText(this, wxID_ANY, headerTextContent,
		wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
	mainSizer->Add(headerText, 0, wxALL | wxEXPAND, FromDIP(10));

	// Create notebook for sections
	auto* notebook = newd wxNotebook(this, wxID_ANY);

	// Section 1: Missing in DAT
	auto* page1 = newd wxPanel(notebook, wxID_ANY);
	auto* sizer1 = newd wxBoxSizer(wxVERTICAL);
	countMissingInDat = newd wxStaticText(page1, wxID_ANY, "");
	sizer1->Add(countMissingInDat, 0, wxALL, FromDIP(5));

	listMissingInDat = newd wxDataViewListCtrl(page1, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_HORIZ_RULES);
	listMissingInDat->AppendTextColumn("Server ID", wxDATAVIEW_CELL_INERT, FromDIP(100));
	listMissingInDat->AppendTextColumn("Client ID", wxDATAVIEW_CELL_INERT, FromDIP(100));
	listMissingInDat->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, FromDIP(200));
	listMissingInDat->AppendTextColumn("Description", wxDATAVIEW_CELL_INERT, FromDIP(250));
	sizer1->Add(listMissingInDat, 1, wxALL | wxEXPAND, FromDIP(5));

	for (const auto& entry : sorted_dat) {
		wxVector<wxVariant> row;
		row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.server_id))));
		row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.client_id))));
		row.push_back(wxVariant(wxString::FromUTF8(entry.name.empty() ? "unknown" : entry.name)));
		row.push_back(wxVariant(wxString::FromUTF8(entry.description)));
		listMissingInDat->AppendItem(row);
	}
	countMissingInDat->SetLabel(wxString::FromUTF8(std::format("Items in {} missing from tibia.dat: {}",
		hasOtb ? "items.otb" : "items.xml", report.missing_in_dat.size())));
	page1->SetSizer(sizer1);
	notebook->AddPage(page1, wxString::FromUTF8(std::format("Missing in DAT ({})", report.missing_in_dat.size())));

	// Section 2: Missing in OTB (DAT not in primary source)
	auto* page2 = newd wxPanel(notebook, wxID_ANY);
	auto* sizer2 = newd wxBoxSizer(wxVERTICAL);
	countMissingInOtb = newd wxStaticText(page2, wxID_ANY, "");
	sizer2->Add(countMissingInOtb, 0, wxALL, FromDIP(5));

	listMissingInOtb = newd wxDataViewListCtrl(page2, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_HORIZ_RULES);
	listMissingInOtb->AppendTextColumn("Client ID", wxDATAVIEW_CELL_INERT, FromDIP(150));
	listMissingInOtb->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, FromDIP(200));
	sizer2->Add(listMissingInOtb, 1, wxALL | wxEXPAND, FromDIP(5));

	for (const auto& entry : sorted_otb) {
		wxVector<wxVariant> row;
		row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.client_id))));
		row.push_back(wxVariant(wxString::FromUTF8(entry.name.empty() ? "(no name)" : entry.name)));
		listMissingInOtb->AppendItem(row);
	}
	countMissingInOtb->SetLabel(wxString::FromUTF8(std::format("Items in tibia.dat not referenced by {}: {}",
		hasOtb ? "items.otb" : "items.xml", report.missing_in_otb.size())));
	page2->SetSizer(sizer2);
	notebook->AddPage(page2, wxString::FromUTF8(std::format("DAT not in {} ({})", hasOtb ? "OTB" : "XML", report.missing_in_otb.size())));

	// Section 3: XML no OTB (only shown if hasOtb is true)
	if (hasOtb) {
		auto* page3 = newd wxPanel(notebook, wxID_ANY);
		auto* sizer3 = newd wxBoxSizer(wxVERTICAL);
		countXmlNoOtb = newd wxStaticText(page3, wxID_ANY, "");
		sizer3->Add(countXmlNoOtb, 0, wxALL, FromDIP(5));

		listXmlNoOtb = newd wxDataViewListCtrl(page3, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_HORIZ_RULES);
		listXmlNoOtb->AppendTextColumn("Server ID", wxDATAVIEW_CELL_INERT, FromDIP(100));
		listXmlNoOtb->AppendTextColumn("Client ID", wxDATAVIEW_CELL_INERT, FromDIP(100));
		listXmlNoOtb->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, FromDIP(200));
		listXmlNoOtb->AppendTextColumn("Description", wxDATAVIEW_CELL_INERT, FromDIP(250));
		sizer3->Add(listXmlNoOtb, 1, wxALL | wxEXPAND, FromDIP(5));

		for (const auto& entry : sorted_xml_no_otb) {
			wxVector<wxVariant> row;
			row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.server_id))));
			row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.client_id))));
			row.push_back(wxVariant(wxString::FromUTF8(entry.name.empty() ? "unknown" : entry.name)));
			row.push_back(wxVariant(wxString::FromUTF8(entry.description)));
			listXmlNoOtb->AppendItem(row);
		}
		countXmlNoOtb->SetLabel(wxString::FromUTF8(std::format("Items in items.xml missing from items.otb: {}", report.xml_no_otb.size())));
		page3->SetSizer(sizer3);
		notebook->AddPage(page3, wxString::FromUTF8(std::format("XML no OTB ({})", report.xml_no_otb.size())));
	}

	// Section 4: OTB no XML (only shown if hasOtb is true)
	if (hasOtb) {
		auto* page4 = newd wxPanel(notebook, wxID_ANY);
		auto* sizer4 = newd wxBoxSizer(wxVERTICAL);
		countOtbNoXml = newd wxStaticText(page4, wxID_ANY, "");
		sizer4->Add(countOtbNoXml, 0, wxALL, FromDIP(5));

		listOtbNoXml = newd wxDataViewListCtrl(page4, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_HORIZ_RULES);
		listOtbNoXml->AppendTextColumn("Server ID", wxDATAVIEW_CELL_INERT, FromDIP(100));
		listOtbNoXml->AppendTextColumn("Client ID", wxDATAVIEW_CELL_INERT, FromDIP(100));
		listOtbNoXml->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, FromDIP(200));
		listOtbNoXml->AppendTextColumn("Description", wxDATAVIEW_CELL_INERT, FromDIP(250));
		sizer4->Add(listOtbNoXml, 1, wxALL | wxEXPAND, FromDIP(5));

		for (const auto& entry : sorted_otb_no_xml) {
			wxVector<wxVariant> row;
			row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.server_id))));
			row.push_back(wxVariant(wxString::FromUTF8(std::format("{}", entry.client_id))));
			row.push_back(wxVariant(wxString::FromUTF8(entry.name.empty() ? "unknown" : entry.name)));
			row.push_back(wxVariant(wxString::FromUTF8(entry.description)));
			listOtbNoXml->AppendItem(row);
		}
		countOtbNoXml->SetLabel(wxString::FromUTF8(std::format("Items in items.otb missing from items.xml: {}", report.otb_no_xml.size())));
		page4->SetSizer(sizer4);
		notebook->AddPage(page4, wxString::FromUTF8(std::format("OTB no XML ({})", report.otb_no_xml.size())));
	}

	mainSizer->Add(notebook, 1, wxALL | wxEXPAND, FromDIP(5));

	// Buttons
	auto* buttonSizer = newd wxBoxSizer(wxHORIZONTAL);

	auto* copyButton = newd wxButton(this, wxID_ANY, "Copy to Clipboard");
	copyButton->Bind(wxEVT_BUTTON, &MissingItemsDialog::OnCopyToClipboard, this);
	buttonSizer->Add(copyButton, 0, wxALL, FromDIP(5));

	auto* saveButton = newd wxButton(this, wxID_ANY, "Save Report...");
	saveButton->Bind(wxEVT_BUTTON, &MissingItemsDialog::OnSaveReport, this);
	buttonSizer->Add(saveButton, 0, wxALL, FromDIP(5));

	buttonSizer->AddStretchSpacer();

	auto* ignoreButton = newd wxButton(this, wxID_ANY, "Close");
	ignoreButton->SetDefault();
	ignoreButton->Bind(wxEVT_BUTTON, &MissingItemsDialog::OnIgnore, this);
	buttonSizer->Add(ignoreButton, 0, wxALL, FromDIP(5));

	mainSizer->Add(buttonSizer, 0, wxEXPAND | wxTOP, FromDIP(5));

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
		wxMessageBox("Failed to open clipboard. Another application may be using it.",
		             "Clipboard Error", wxOK | wxICON_ERROR, this);
		return;
	}

	wxTheClipboard->SetData(newd wxTextDataObject(GenerateReportText()));
	wxTheClipboard->Close();
	wxMessageBox("Report copied to clipboard.", "Clipboard", wxOK | wxICON_INFORMATION, this);
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
			wxMessageBox("Report saved successfully.", "Save Report", wxOK | wxICON_INFORMATION, this);
		} else {
			wxMessageBox("Failed to create report file.", "Save Report Error", wxOK | wxICON_ERROR, this);
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
