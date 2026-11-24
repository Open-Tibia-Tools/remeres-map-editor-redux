#include "app/main.h"

#include "app/client_version.h"
#include "ui/controls/themed_button.h"
#include "ui/dialogs/client_mismatch_dialog.h"
#include "ui/theme.h"
#include "util/common.h"
#include "util/image_manager.h"

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include <format>

ClientMismatchDialog::ClientMismatchDialog(
    wxWindow* parent, const ClientVersion& current, const ClientVersion& target, uint32_t otb_major, int real_open_map_count,
    bool target_has_valid_paths
) :
    wxDialog(parent, wxID_ANY, "Map Version Mismatch", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    SetBackgroundColour(Theme::Get(Theme::Role::Surface));

    const int outer = Theme::Grid(4);
    const int gap_sm = Theme::Grid(1);
    const int gap_md = Theme::Grid(2);
    const int gap_lg = Theme::Grid(3);
    const int wrap_width = Theme::Grid(98);
    const bool can_switch = current.getID() != target.getID();

    wxBoxSizer* top_sizer = new wxBoxSizer(wxVERTICAL);

    // Header row with warning icon and summary
    wxBoxSizer* header_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticBitmap* icon = new wxStaticBitmap(this, wxID_ANY, IMAGE_MANAGER.GetBitmap(ICON_TRIANGLE_EXCLAMATION, FromDIP(wxSize(32, 32))));
    header_sizer->Add(icon, wxSizerFlags().CenterVertical().Border(wxRIGHT, gap_lg));

    wxBoxSizer* title_sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* title_text = new wxStaticText(this, wxID_ANY, "The map was created for a different client version.");
    title_text->SetFont(Theme::GetFont(10, true));
    title_text->SetForegroundColour(Theme::Get(Theme::Role::Text));
    title_text->Wrap(wrap_width);
    title_sizer->Add(title_text, wxSizerFlags().Border(wxBOTTOM, gap_sm));

    wxStaticText* subtitle_text
        = new wxStaticText(this, wxID_ANY, "Choose whether to switch the editor client or load the map using active client assets.");
    subtitle_text->SetFont(Theme::GetFont(9, false));
    subtitle_text->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
    subtitle_text->Wrap(wrap_width);
    title_sizer->Add(subtitle_text, wxSizerFlags());

    header_sizer->Add(title_sizer, wxSizerFlags(1).CenterVertical());
    top_sizer->Add(header_sizer, wxSizerFlags().Expand().Border(wxALL, outer));

    // Version details and impact summary
    wxStaticBoxSizer* details_box = new wxStaticBoxSizer(wxVERTICAL, this, "Version Details");
    wxWindow* box = details_box->GetStaticBox();

    wxFlexGridSizer* grid = new wxFlexGridSizer(0, 2, gap_sm, gap_lg);
    grid->AddGrowableCol(1, 1);

    auto add_detail_row = [&](const wxString& label, const wxString& value, Theme::Role value_role, bool bold_value) {
        wxStaticText* key_text = new wxStaticText(box, wxID_ANY, label);
        key_text->SetFont(Theme::GetFont(9, false));
        key_text->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
        grid->Add(key_text, wxSizerFlags().Align(wxALIGN_TOP));

        wxStaticText* val_text = new wxStaticText(box, wxID_ANY, value);
        val_text->SetFont(Theme::GetFont(9, bold_value));
        val_text->SetForegroundColour(Theme::Get(value_role));
        grid->Add(val_text, wxSizerFlags().Expand());
    };

    add_detail_row("Map client:", wxstr(std::format("{} (OTB v{})", target.getName(), otb_major)), Theme::Role::Text, true);
    add_detail_row("Active client:", wxstr(current.getName()), Theme::Role::Text, true);
    if (can_switch && !target_has_valid_paths) {
        add_detail_row("Target assets:", "Not configured (folder selection required on switch)", Theme::Role::TextSubtle, false);
    }

    details_box->Add(grid, wxSizerFlags().Expand().Border(wxALL, gap_md));

    wxStaticText* force_warn = new wxStaticText(
        box, wxID_ANY, "Force-loading preserves open tabs, but mismatched client assets may omit or substitute items and sprites."
    );
    force_warn->SetFont(Theme::GetFont(8, false));
    force_warn->SetForegroundColour(Theme::Get(Theme::Role::Warning));
    force_warn->Wrap(wrap_width - outer);
    details_box->Add(force_warn, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, gap_md));

    if (can_switch && real_open_map_count > 0) {
        const std::string switch_warn_text = (real_open_map_count == 1)
            ? "Switching client version will close 1 open map tab."
            : std::format("Switching client version will close {} open map tabs.", real_open_map_count);
        wxStaticText* switch_warn = new wxStaticText(box, wxID_ANY, wxstr(switch_warn_text));
        switch_warn->SetFont(Theme::GetFont(8, false));
        switch_warn->SetForegroundColour(Theme::Get(Theme::Role::Warning));
        switch_warn->Wrap(wrap_width - outer);
        details_box->Add(switch_warn, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, gap_md));
    }

    top_sizer->Add(details_box, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, outer));

    // Separated single-row color-coded action bar
    top_sizer->Add(
        new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL),
        wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, outer)
    );

    const std::string force_label = std::format("Force Load ({})", current.getName());
    wxButton* force_btn = new wxButton(this, wxID_YES, wxstr(force_label), wxDefaultPosition, FromDIP(wxSize(-1, 30)));
    force_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_BOLT));
    force_btn->SetFont(Theme::GetFont(9, true));
    force_btn->SetToolTip("Opens the map in a new tab using active client assets. Existing tabs stay open.");
    ApplyThemeButtonStyle(force_btn, Theme::Role::Warning);

    const std::string switch_label = std::format("Switch to {}{}", target.getName(), (can_switch && !target_has_valid_paths) ? "..." : "");
    wxButton* switch_btn = new wxButton(this, wxID_NO, wxstr(switch_label), wxDefaultPosition, FromDIP(wxSize(-1, 30)));
    switch_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_ROTATE));
    switch_btn->SetFont(Theme::GetFont(9, true));
    if (can_switch) {
        switch_btn->SetToolTip("Switches RME client version to match the map. Closes currently open map tabs.");
        ApplyThemeButtonStyle(switch_btn, Theme::Role::PrimaryButton, Theme::Role::TextOnAccent);
    } else {
        switch_btn->Enable(false);
        switch_btn->SetToolTip("The resolved target client is already active with incompatible item definitions.");
        ApplyThemeButtonStyle(switch_btn, Theme::Role::ButtonDisabled, Theme::Role::TextSubtle);
    }

    wxButton* cancel_btn = new wxButton(this, wxID_CANCEL, "Cancel", wxDefaultPosition, FromDIP(wxSize(-1, 30)));
    cancel_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
    cancel_btn->SetFont(Theme::GetFont(9, false));
    cancel_btn->SetToolTip("Cancel opening this map.");
    ApplyThemeButtonStyle(cancel_btn, Theme::Role::SecondaryButton, Theme::Role::Text);

    const bool switch_is_default = can_switch && real_open_map_count == 0;
    if (switch_is_default) {
        switch_btn->SetDefault();
        switch_btn->SetFocus();
    } else {
        force_btn->SetDefault();
        force_btn->SetFocus();
    }

    wxBoxSizer* footer_sizer = new wxBoxSizer(wxHORIZONTAL);
    footer_sizer->Add(force_btn, wxSizerFlags().CenterVertical().Border(wxRIGHT, gap_md));
    footer_sizer->Add(switch_btn, wxSizerFlags().CenterVertical());
    footer_sizer->AddStretchSpacer(1);
    footer_sizer->Add(cancel_btn, wxSizerFlags().CenterVertical().Border(wxLEFT, gap_lg));
    top_sizer->Add(footer_sizer, wxSizerFlags().Expand().Border(wxALL, outer));

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
