#include "ui/welcome/startup_card.h"

#include "ui/theme.h"

StartupCardPanel::StartupCardPanel(wxWindow* parent, const wxString& title, bool compact) :
	wxPanel(parent, wxID_ANY),
	m_title(title),
	m_compact(compact) {
	SetBackgroundStyle(wxBG_STYLE_PAINT);
	SetBackgroundColour(Theme::Get(Theme::Role::RaisedSurface));
	SetForegroundColour(Theme::Get(Theme::Role::Text));

	auto* root_sizer = new wxBoxSizer(wxVERTICAL);
	const int body_padding = FromDIP(m_compact ? 10 : 14);

	if (!m_title.empty()) {
		root_sizer->AddSpacer(FromDIP(m_compact ? 10 : 12));

		m_title_label = new wxStaticText(this, wxID_ANY, m_title);
		m_title_label->SetFont(Theme::GetFont(9, true));
		m_title_label->SetForegroundColour(Theme::Get(Theme::Role::Text));
		m_title_label->SetBackgroundColour(GetBackgroundColour());
		root_sizer->Add(m_title_label, wxSizerFlags().Border(wxLEFT | wxRIGHT, body_padding));

		auto* separator = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(1)));
		separator->SetBackgroundColour(Theme::Get(Theme::Role::Border));
		root_sizer->Add(separator, wxSizerFlags().Expand().Border(wxALL, body_padding));
	}

	m_body_sizer = new wxBoxSizer(wxVERTICAL);
	root_sizer->Add(m_body_sizer, wxSizerFlags(1).Expand().Border(m_title.empty() ? wxALL : (wxLEFT | wxRIGHT | wxBOTTOM), body_padding));

	SetSizer(root_sizer);
	Bind(wxEVT_PAINT, &StartupCardPanel::OnPaint, this);
}

void StartupCardPanel::OnPaint(wxPaintEvent& WXUNUSED(event)) {
	wxAutoBufferedPaintDC dc(this);
	dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
	dc.Clear();

	const wxRect rect = GetClientRect().Deflate(1);
	dc.SetPen(wxPen(Theme::Get(Theme::Role::Border)));
	dc.SetBrush(wxBrush(Theme::Get(Theme::Role::RaisedSurface)));
	dc.DrawRoundedRectangle(rect, FromDIP(10));
}
