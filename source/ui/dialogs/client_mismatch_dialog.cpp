#include "app/main.h"
#include "ui/dialogs/client_mismatch_dialog.h"
#include "app/client_version.h"
#include "ui/theme.h"
#include "util/image_manager.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/statbmp.h>
#include <wx/statbox.h>
#include <format>

ClientMismatchDialog::ClientMismatchDialog(
	wxWindow* parent,
	const ClientVersion& current,
	const ClientVersion& target,
	uint32_t otb_major,
	int open_map_count
) :
	wxDialog(parent, wxID_ANY, "Map Version Mismatch", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	wxBoxSizer* top_sizer = new wxBoxSizer(wxVERTICAL);

	// Header row with Warning Icon and primary message
	wxBoxSizer* header_sizer = new wxBoxSizer(wxHORIZONTAL);
	wxStaticBitmap* icon = new wxStaticBitmap(this, wxID_ANY, IMAGE_MANAGER.GetBitmap(ICON_TRIANGLE_EXCLAMATION, FromDIP(wxSize(32, 32))));
	header_sizer->Add(icon, wxSizerFlags().CenterVertical().Border(wxRIGHT, 12));

	wxBoxSizer* title_sizer = new wxBoxSizer(wxVERTICAL);
	wxStaticText* title_text = new wxStaticText(this, wxID_ANY, "The map was created for a different client version.");
	wxFont title_font = title_text->GetFont();
	title_font.MakeBold();
	title_font.SetPointSize(title_font.GetPointSize() + 1);
	title_text->SetFont(title_font);
	title_text->SetForegroundColour(Theme::Get(Theme::Role::Text));
	title_sizer->Add(title_text, wxSizerFlags().Border(wxBOTTOM, 4));

	wxStaticText* subtitle_text = new wxStaticText(
		this,
		wxID_ANY,
		"You can either force-load the map using the currently active client assets, or switch the editor client version."
	);
	subtitle_text->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	title_sizer->Add(subtitle_text, wxSizerFlags());

	header_sizer->Add(title_sizer, wxSizerFlags(1).CenterVertical());
	top_sizer->Add(header_sizer, wxSizerFlags().Expand().Border(wxALL, 16));

	// Detail Box
	wxStaticBoxSizer* details_box = new wxStaticBoxSizer(wxVERTICAL, this, "Version Details");

	std::string map_info = std::format("Map Client: {} (OTB v{})", target.getName(), otb_major);
	wxStaticText* map_label = new wxStaticText(details_box->GetStaticBox(), wxID_ANY, wxstr(map_info));
	map_label->SetForegroundColour(Theme::Get(Theme::Role::Text));
	details_box->Add(map_label, wxSizerFlags().Border(wxALL, 6));

	std::string active_info = std::format("Active Editor Client: {}", current.getName());
	wxStaticText* active_label = new wxStaticText(details_box->GetStaticBox(), wxID_ANY, wxstr(active_info));
	active_label->SetForegroundColour(Theme::Get(Theme::Role::Text));
	details_box->Add(active_label, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 6));

	if (open_map_count > 0) {
		std::string warn_info = std::format(
			"Note: You currently have {} open map tab(s). Switching client version will close them.",
			open_map_count
		);
		wxStaticText* warn_label = new wxStaticText(details_box->GetStaticBox(), wxID_ANY, wxstr(warn_info));
		warn_label->SetForegroundColour(Theme::Get(Theme::Role::Warning));
		details_box->Add(warn_label, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, 6));
	}

	top_sizer->Add(details_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 16));

	// Action Buttons
	wxBoxSizer* button_sizer = new wxBoxSizer(wxVERTICAL);

	std::string force_btn_label = std::format("Force Load with Active Client ({})", current.getName());
	wxButton* force_btn = new wxButton(this, wxID_YES, wxstr(force_btn_label));
	force_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_BOLT));
	force_btn->SetToolTip("Opens the map in a new tab using current client assets. Does not close existing tabs.");
	force_btn->SetDefault();
	button_sizer->Add(force_btn, wxSizerFlags().Expand().Border(wxBOTTOM, 8));

	std::string switch_btn_label = std::format("Switch Client Version to {}...", target.getName());
	wxButton* switch_btn = new wxButton(this, wxID_NO, wxstr(switch_btn_label));
	switch_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_ROTATE));
	switch_btn->SetToolTip("Changes RME client version to match the map. Closes all currently open map tabs.");
	button_sizer->Add(switch_btn, wxSizerFlags().Expand().Border(wxBOTTOM, 8));

	wxButton* cancel_btn = new wxButton(this, wxID_CANCEL, "Cancel");
	cancel_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	cancel_btn->SetToolTip("Cancel opening this map.");
	button_sizer->Add(cancel_btn, wxSizerFlags().Expand());

	top_sizer->Add(button_sizer, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 16));

	SetSizerAndFit(top_sizer);
	Centre(wxBOTH);

	force_btn->Bind(wxEVT_BUTTON, &ClientMismatchDialog::OnForceLoad, this);
	switch_btn->Bind(wxEVT_BUTTON, &ClientMismatchDialog::OnSwitchVersion, this);
	cancel_btn->Bind(wxEVT_BUTTON, &ClientMismatchDialog::OnCancel, this);

	wxIcon dlg_icon;
	dlg_icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_TRIANGLE_EXCLAMATION, FromDIP(wxSize(32, 32))));
	SetIcon(dlg_icon);
}

void ClientMismatchDialog::OnForceLoad(wxCommandEvent&) {
	choice = ClientMismatchChoice::ForceLoadActive;
	EndModal(wxID_YES);
}

void ClientMismatchDialog::OnSwitchVersion(wxCommandEvent&) {
	choice = ClientMismatchChoice::SwitchVersion;
	EndModal(wxID_NO);
}

void ClientMismatchDialog::OnCancel(wxCommandEvent&) {
	choice = ClientMismatchChoice::Cancel;
	EndModal(wxID_CANCEL);
}
