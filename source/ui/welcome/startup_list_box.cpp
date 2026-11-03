#include "ui/welcome/startup_list_box.h"

#include "util/image_manager.h"
#include "ui/theme.h"

StartupListBox::StartupListBox(wxWindow* parent, wxWindowID id) :
	wxVListBox(parent, id, wxDefaultPosition, wxDefaultSize, wxLB_SINGLE | wxBORDER_NONE) {
	SetBackgroundColour(Theme::Get(Theme::Role::PanelBackground));
	SetForegroundColour(Theme::Get(Theme::Role::Text));
}

void StartupListBox::SetItems(std::vector<StartupListItem> items) {
	m_items = std::move(items);
	SetItemCount(m_items.size());
	RefreshAll();
}

const StartupListItem* StartupListBox::GetItem(size_t index) const {
	if (index >= m_items.size()) {
		return nullptr;
	}
	return &m_items[index];
}

void StartupListBox::OnDrawBackground(wxDC& dc, const wxRect& rect, size_t index) const {
	const bool selected = IsSelected(index);
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(wxBrush(selected ? Theme::Get(Theme::Role::SelectionFill) : Theme::Get(Theme::Role::PanelBackground)));
	dc.DrawRoundedRectangle(rect.Deflate(FromDIP(2), FromDIP(2)), FromDIP(8));

	if (selected) {
		wxRect accent_rect = rect.Deflate(FromDIP(4), FromDIP(6));
		accent_rect.SetWidth(FromDIP(4));
		dc.SetBrush(wxBrush(Theme::Get(Theme::Role::PrimaryButton)));
		dc.DrawRoundedRectangle(accent_rect, FromDIP(2));
	}
}

void StartupListBox::OnDrawItem(wxDC& dc, const wxRect& rect, size_t index) const {
	if (index >= m_items.size()) {
		return;
	}

	const auto& item = m_items[index];
	const bool selected = IsSelected(index);
	const int icon_size = FromDIP(16);
	const int left_padding = FromDIP(14);
	const int top_padding = FromDIP(10);
	const int text_x = rect.x + left_padding + icon_size + FromDIP(10);
	const int available_width = rect.width - (text_x - rect.x) - FromDIP(12);

	const wxBitmap icon = GetIconBitmap(item.icon_art_id);
	if (icon.IsOk()) {
		dc.DrawBitmap(icon, rect.x + left_padding, rect.y + top_padding, true);
	}

	dc.SetFont(Theme::GetFont(9, true));
	const bool isDark = (Theme::Get(Theme::Role::Surface).Red() <= 128);
	const wxColour primary_colour = selected ? (isDark ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Text)) : (item.accent_colour.IsOk() ? item.accent_colour : Theme::Get(Theme::Role::Text));
	dc.SetTextForeground(primary_colour);
	const wxString primary_text = wxControl::Ellipsize(item.primary_text, dc, wxELLIPSIZE_MIDDLE, available_width);
	dc.DrawText(primary_text, text_x, rect.y + FromDIP(6));

	dc.SetFont(Theme::GetFont(8, false));
	dc.SetTextForeground(selected ? (isDark ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle)) : Theme::Get(Theme::Role::TextSubtle));
	const wxString secondary_text = wxControl::Ellipsize(item.secondary_text, dc, wxELLIPSIZE_END, available_width);
	dc.DrawText(secondary_text, text_x, rect.y + FromDIP(26));
}

wxCoord StartupListBox::OnMeasureItem(size_t WXUNUSED(index)) const {
	return FromDIP(50);
}

wxBitmap StartupListBox::GetIconBitmap(const std::string& art_id) const {
	if (const auto cached = m_icon_cache.find(art_id); cached != m_icon_cache.end()) {
		return cached->second;
	}

	wxBitmap bitmap = IMAGE_MANAGER.GetBitmap(art_id, FromDIP(wxSize(16, 16)));
	m_icon_cache.emplace(art_id, bitmap);
	return bitmap;
}
