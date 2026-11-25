#include "app/main.h"

#include "app/client_version.h"
#include "ui/controls/themed_button.h"
#include "ui/dialogs/client_mismatch_dialog.h"
#include "ui/dialogs/client_mismatch_presentation.h"
#include "ui/theme.h"
#include "ui/theme_contrast.h"
#include "util/common.h"
#include "util/image_manager.h"

#include <wx/button.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include <format>

ClientMismatchDialog::ClientMismatchDialog(
    wxWindow* parent, const ClientVersion& current, const ClientVersion& target, const MapVersion& map_ver, std::string_view map_filename,
    int real_open_map_count, SwitchPreconditionStatus switch_status
) :
    wxDialog(parent, wxID_ANY, "Map Version Mismatch", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    const wxColour surface_bg = Theme::Get(Theme::Role::Surface);
    SetBackgroundColour(surface_bg);

    const int outer = Theme::Grid(4);
    const int gap_sm = Theme::Grid(1);
    const int gap_md = Theme::Grid(2);
    const int gap_lg = Theme::Grid(3);

    const ClientMismatchPresentation pres = BuildClientMismatchPresentation(
        ClientMismatchVersionSummary {
            .name = current.getName(),
            .otbm_id = static_cast<int>(current.getPrefferedMapVersionID()),
            .otb_major = static_cast<uint32_t>(current.getOTBVersion().format_version),
            .otb_minor = current.getProtocolID(),
        },
        ClientMismatchVersionSummary {
            .name = target.getName(),
            .otbm_id = static_cast<int>(map_ver.otbm),
            .otb_major = map_ver.items_major,
            .otb_minor = map_ver.client,
        },
        map_filename, real_open_map_count, switch_status
    );

    wxBoxSizer* top_sizer = new wxBoxSizer(wxVERTICAL);

    const wxColour raised_bg = Theme::Get(Theme::Role::RaisedSurface);
    const wxColour text_fg = Theme::Get(Theme::Role::Text);
    const wxColour subtle_fg = Theme::Get(Theme::Role::TextSubtle);
    const wxSize card_min_size = FromDIP(wxSize(195, -1));

    auto build_card = [&](Theme::Role border_role, const std::string& header_label, Theme::Role header_role,
                          const std::string& version_name, const std::string& version_meta) {
        wxPanel* border_panel = new wxPanel(this, wxID_ANY);
        border_panel->SetBackgroundColour(Theme::Get(border_role));
        border_panel->SetMinSize(card_min_size);

        wxPanel* inner_panel = new wxPanel(border_panel, wxID_ANY);
        inner_panel->SetBackgroundColour(raised_bg);

        wxBoxSizer* card_sizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* header_text
            = new wxStaticText(inner_panel, wxID_ANY, wxstr(header_label), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
        header_text->SetFont(Theme::GetFont(8, true));
        header_text->SetForegroundColour(Theme::Get(header_role));
        header_text->SetBackgroundColour(raised_bg);
        card_sizer->Add(header_text, wxSizerFlags().Expand().Border(wxBOTTOM, gap_sm));

        wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText* name_text = new wxStaticText(inner_panel, wxID_ANY, wxstr(version_name));
        name_text->SetFont(Theme::GetFont(11, true));
        name_text->SetForegroundColour(text_fg);
        name_text->SetBackgroundColour(raised_bg);
        row_sizer->Add(name_text, wxSizerFlags().Align(wxALIGN_BOTTOM).Border(wxRIGHT, gap_md));

        row_sizer->AddStretchSpacer(1);

        wxStaticText* meta_text = new wxStaticText(inner_panel, wxID_ANY, wxstr(version_meta));
        meta_text->SetFont(Theme::GetFont(8, false));
        meta_text->SetForegroundColour(subtle_fg);
        meta_text->SetBackgroundColour(raised_bg);
        row_sizer->Add(meta_text, wxSizerFlags().Align(wxALIGN_BOTTOM));

        card_sizer->Add(row_sizer, wxSizerFlags().Expand());

        wxBoxSizer* inner_pad = new wxBoxSizer(wxVERTICAL);
        inner_pad->Add(card_sizer, wxSizerFlags(1).Expand().Border(wxALL, gap_lg));
        inner_panel->SetSizer(inner_pad);

        wxBoxSizer* border_sizer = new wxBoxSizer(wxVERTICAL);
        border_sizer->Add(inner_panel, wxSizerFlags(1).Expand().Border(wxALL, 1));
        border_panel->SetSizer(border_sizer);
        return border_panel;
    };

    // Comparison row: Loaded Client -> Map File
    wxFlexGridSizer* cards_grid = new wxFlexGridSizer(1, 3, 0, gap_md);
    cards_grid->AddGrowableCol(0, 1);
    cards_grid->AddGrowableCol(2, 1);

    wxPanel* loaded_card
        = build_card(Theme::Role::CardBorder, pres.loaded_header, Theme::Role::TextSubtle, pres.loaded_name, pres.loaded_meta);
    wxStaticBitmap* arrow_bmp = new wxStaticBitmap(this, wxID_ANY, IMAGE_MANAGER.GetBitmap(ICON_ARROW_RIGHT, FromDIP(wxSize(14, 14))));
    wxPanel* map_card = build_card(Theme::Role::Accent, pres.map_header, Theme::Role::Accent, pres.map_name, pres.map_meta);

    cards_grid->Add(loaded_card, wxSizerFlags(1).Expand());
    cards_grid->Add(arrow_bmp, wxSizerFlags().Center());
    cards_grid->Add(map_card, wxSizerFlags(1).Expand());
    top_sizer->Add(cards_grid, wxSizerFlags().Expand().Border(wxALL, outer));

    if (pres.show_tabs_warning || pres.show_switch_unavailable_note) {
        const bool is_warning = pres.show_tabs_warning;
        const wxColour notice_fg = is_warning
            ? [&]() {
                  const ThemeRgb warn_rgb = PickAccessibleForeground(
                      ToThemeRgb(surface_bg), ToThemeRgb(Theme::Get(Theme::Role::Warning)), ToThemeRgb(Theme::Get(Theme::Role::Error))
                  );
                  return wxColour(warn_rgb.r, warn_rgb.g, warn_rgb.b);
              }()
            : subtle_fg;

        wxBoxSizer* notice_sizer = new wxBoxSizer(wxHORIZONTAL);
        wxStaticBitmap* notice_icon = new wxStaticBitmap(
            this, wxID_ANY,
            IMAGE_MANAGER.GetBitmap(is_warning ? ICON_TRIANGLE_EXCLAMATION : ICON_INFO, FromDIP(wxSize(16, 16)))
        );
        notice_sizer->Add(notice_icon, wxSizerFlags().CenterVertical().Border(wxRIGHT, gap_sm));

        const std::string& notice_message = is_warning ? pres.tabs_warning_text : pres.switch_unavailable_text;
        wxStaticText* notice_text = new wxStaticText(this, wxID_ANY, wxstr(notice_message));
        notice_text->SetFont(Theme::GetFont(9, false));
        notice_text->SetForegroundColour(notice_fg);
        notice_text->SetBackgroundColour(surface_bg);
        notice_text->Wrap(FromDIP(410));
        notice_sizer->Add(notice_text, wxSizerFlags(1).CenterVertical());

        top_sizer->Add(notice_sizer, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, outer));
    }

    top_sizer->Add(
        new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL),
        wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, outer)
    );

    // Equal-width Style C footer: Switch Client | Force Load (always default) | Cancel
    wxButton* switch_btn = new wxButton(this, wxID_NO, "Switch Client", wxDefaultPosition, FromDIP(wxSize(-1, 30)));
    switch_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_ROTATE));
    switch_btn->SetFont(Theme::GetFont(9, false));
    switch_btn->SetToolTip(wxstr(pres.switch_tooltip));
    if (pres.switch_enabled) {
        ApplyThemeButtonStyle(switch_btn, Theme::Role::SecondaryButton, Theme::Role::Text);
    } else {
        switch_btn->Enable(false);
        ApplyThemeButtonStyle(switch_btn, Theme::Role::ButtonDisabled, Theme::Role::TextSubtle);
    }

    wxButton* force_btn = new wxButton(this, wxID_YES, "Force Load", wxDefaultPosition, FromDIP(wxSize(-1, 30)));
    force_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_BOLT));
    force_btn->SetFont(Theme::GetFont(9, true));
    force_btn->SetToolTip(wxstr(std::format("Open this map using the currently loaded {} client assets.", current.getName())));
    ApplyThemeButtonStyle(force_btn, Theme::Role::PrimaryButton, Theme::Role::TextOnAccent);
    force_btn->SetDefault();
    force_btn->SetFocus();

    wxButton* cancel_btn = new wxButton(this, wxID_CANCEL, "Cancel", wxDefaultPosition, FromDIP(wxSize(-1, 30)));
    cancel_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
    cancel_btn->SetFont(Theme::GetFont(9, false));
    cancel_btn->SetToolTip("Cancel opening this map.");
    ApplyThemeButtonStyle(cancel_btn, Theme::Role::SecondaryButton, Theme::Role::Text);

    wxGridSizer* footer_grid = new wxGridSizer(1, 3, 0, gap_md);
    footer_grid->Add(switch_btn, wxSizerFlags().Expand());
    footer_grid->Add(force_btn, wxSizerFlags().Expand());
    footer_grid->Add(cancel_btn, wxSizerFlags().Expand());
    top_sizer->Add(footer_grid, wxSizerFlags().Expand().Border(wxALL, outer));

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
