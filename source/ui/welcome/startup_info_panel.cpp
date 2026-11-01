#include "ui/welcome/startup_info_panel.h"

#include <algorithm>

#include "util/image_manager.h"
#include "ui/theme.h"

StartupInfoPanel::StartupInfoPanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY) {
	SetBackgroundColour(parent->GetBackgroundColour());
	SetForegroundColour(Theme::Get(Theme::Role::Text));

	m_content_sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(m_content_sizer);

	Bind(wxEVT_SIZE, &StartupInfoPanel::OnSize, this);
}

void StartupInfoPanel::SetFields(const std::vector<StartupInfoField>& fields) {
	Freeze();
	m_content_sizer->Clear(true);
	m_value_labels.clear();

	for (const auto& field : fields) {
		auto* field_sizer = new wxBoxSizer(wxVERTICAL);
		auto* label_sizer = new wxBoxSizer(wxHORIZONTAL);

		auto* icon = new wxStaticBitmap(this, wxID_ANY, IMAGE_MANAGER.GetBitmap(field.icon_art_id, wxSize(14, 14)));
		label_sizer->Add(icon, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(4)));

		auto* label = new wxStaticText(this, wxID_ANY, field.label);
		label->SetFont(Theme::GetFont(8, false));
		label->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
		label->SetBackgroundColour(GetBackgroundColour());
		label_sizer->Add(label, wxSizerFlags(1).CenterVertical());
		field_sizer->Add(label_sizer, wxSizerFlags().Border(wxBOTTOM, FromDIP(1)));

		const wxString display_value = field.value.empty() ? wxString("-") : field.value;
		auto* value = new wxStaticText(this, wxID_ANY, display_value);
		value->SetFont(Theme::GetFont(9, true));
		value->SetForegroundColour(field.value_colour.IsOk() ? field.value_colour : Theme::Get(Theme::Role::Text));
		value->SetBackgroundColour(GetBackgroundColour());
		field_sizer->Add(value, wxSizerFlags().Border(wxLEFT | wxBOTTOM, FromDIP(18)));
		m_value_labels.push_back(value);

		m_content_sizer->Add(field_sizer, wxSizerFlags().Expand());

		if (field.separator_after) {
			auto* separator = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(1)));
			separator->SetBackgroundColour(Theme::Get(Theme::Role::Border));
			m_content_sizer->Add(separator, wxSizerFlags().Expand().Border(wxBOTTOM, FromDIP(6)));
		}
	}

	Thaw();
	Layout();
	UpdateWrapping();
}

void StartupInfoPanel::OnSize(wxSizeEvent& event) {
	UpdateWrapping();
	event.Skip();
}

void StartupInfoPanel::UpdateWrapping() {
	const int wrap_width = std::max(GetClientSize().GetWidth() - FromDIP(20), FromDIP(140));
	for (auto* label : m_value_labels) {
		label->Wrap(wrap_width);
	}
	Layout();
}
