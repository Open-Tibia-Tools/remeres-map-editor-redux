#include "ui/welcome/startup_button.h"

#include <algorithm>

#include "ui/theme.h"

namespace {
struct ButtonPalette {
	wxColour background;
	wxColour border;
	wxColour text;
};

ButtonPalette resolveButtonPalette(StartupButtonVariant variant, bool hovered, bool pressed, bool enabled) {
	if (!enabled) {
		return {
			Theme::Get(Theme::Role::ButtonDisabled),
			Theme::Get(Theme::Role::Border),
			Theme::Get(Theme::Role::TextSubtle),
		};
	}

	if (variant == StartupButtonVariant::Primary) {
		if (pressed) {
			return {
				Theme::Get(Theme::Role::PrimaryButtonHover),
				Theme::Get(Theme::Role::PrimaryButtonHover),
				Theme::Get(Theme::Role::TextOnAccent),
			};
		}
		if (hovered) {
			return {
				Theme::Get(Theme::Role::PrimaryButtonHover),
				Theme::Get(Theme::Role::PrimaryButtonHover),
				Theme::Get(Theme::Role::TextOnAccent),
			};
		}
		return {
			Theme::Get(Theme::Role::PrimaryButton),
			Theme::Get(Theme::Role::PrimaryButton),
			Theme::Get(Theme::Role::TextOnAccent),
		};
	}

	if (pressed) {
		return {
			Theme::Get(Theme::Role::SecondaryButtonHover),
			Theme::Get(Theme::Role::Border),
			Theme::Get(Theme::Role::Text),
		};
	}
	if (hovered) {
		return {
			Theme::Get(Theme::Role::SecondaryButtonHover),
			Theme::Get(Theme::Role::Border),
			Theme::Get(Theme::Role::Text),
		};
	}
	return {
		Theme::Get(Theme::Role::SecondaryButton),
		Theme::Get(Theme::Role::Border),
		Theme::Get(Theme::Role::Text),
	};
}
}

StartupButton::StartupButton(wxWindow* parent, wxWindowID id, const wxString& label, StartupButtonVariant variant, const wxPoint& pos, const wxSize& size) :
	wxControl(parent, id, pos, size, wxBORDER_NONE),
	m_variant(variant) {
	SetLabel(label);
	SetFont(Theme::GetFont(9, true));
	SetForegroundColour(Theme::Get(Theme::Role::Text));
	SetBackgroundStyle(wxBG_STYLE_PAINT);

	Bind(wxEVT_PAINT, &StartupButton::OnPaint, this);
	Bind(wxEVT_ENTER_WINDOW, &StartupButton::OnMouse, this);
	Bind(wxEVT_LEAVE_WINDOW, &StartupButton::OnMouse, this);
	Bind(wxEVT_LEFT_DOWN, &StartupButton::OnMouse, this);
	Bind(wxEVT_LEFT_UP, &StartupButton::OnMouse, this);
	Bind(wxEVT_MOUSE_CAPTURE_LOST, &StartupButton::OnMouseCaptureLost, this);
}

void StartupButton::SetBitmap(const wxBitmapBundle& bundle) {
	m_bundle = bundle;
	m_icon = bundle.IsOk() ? bundle.GetBitmapFor(this) : wxBitmap();
	InvalidateBestSize();
	Refresh();
}

void StartupButton::SetBitmap(const wxBitmap& bitmap) {
	m_bundle = wxBitmapBundle::FromBitmap(bitmap);
	m_icon = bitmap;
	InvalidateBestSize();
	Refresh();
}

void StartupButton::SetVariant(StartupButtonVariant variant) {
	m_variant = variant;
	Refresh();
}

wxSize StartupButton::DoGetBestClientSize() const {
	wxClientDC dc(const_cast<StartupButton*>(this));
	dc.SetFont(GetFont());

	const wxSize text_size = dc.GetTextExtent(GetLabel());
	const wxBitmap icon = m_bundle.IsOk() ? m_bundle.GetBitmapFor(this) : m_icon;
	const int icon_width = icon.IsOk() ? icon.GetWidth() + FromDIP(8) : 0;
	const int width = text_size.x + icon_width + FromDIP(30);
	const int height = std::max(text_size.y, icon.IsOk() ? icon.GetHeight() : 0) + FromDIP(16);
	return { std::max(width, FromDIP(120)), std::max(height, FromDIP(42)) };
}

void StartupButton::OnPaint(wxPaintEvent& WXUNUSED(event)) {
	wxAutoBufferedPaintDC dc(this);
	dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
	dc.Clear();

	const ButtonPalette palette = resolveButtonPalette(m_variant, m_hovered, m_pressed, IsEnabled());
	const wxRect rect = GetClientRect().Deflate(1);

	dc.SetPen(wxPen(palette.border));
	dc.SetBrush(wxBrush(palette.background));
	dc.DrawRoundedRectangle(rect, FromDIP(8));

	dc.SetFont(GetFont());
	dc.SetTextForeground(palette.text);

	const wxSize text_size = dc.GetTextExtent(GetLabel());
	const wxBitmap icon = m_bundle.IsOk() ? m_bundle.GetBitmapFor(this) : m_icon;
	const int content_width = text_size.x + (icon.IsOk() ? icon.GetWidth() + FromDIP(8) : 0);
	int x = rect.x + (rect.width - content_width) / 2;
	const int y = rect.y + (rect.height - text_size.y) / 2;

	if (icon.IsOk()) {
		const int icon_y = rect.y + (rect.height - icon.GetHeight()) / 2;
		dc.DrawBitmap(icon, x, icon_y, true);
		x += icon.GetWidth() + FromDIP(8);
	}

	dc.DrawText(GetLabel(), x, y);
}

void StartupButton::OnMouse(wxMouseEvent& event) {
	if (!IsEnabled()) {
		return;
	}

	if (event.Entering()) {
		m_hovered = true;
	} else if (event.Leaving()) {
		m_hovered = false;
		m_pressed = false;
	} else if (event.LeftDown()) {
		m_pressed = true;
		if (!HasCapture()) {
			CaptureMouse();
		}
	} else if (event.LeftUp()) {
		const bool was_pressed = m_pressed;
		m_pressed = false;
		if (HasCapture()) {
			ReleaseMouse();
		}
		if (was_pressed && GetClientRect().Contains(event.GetPosition())) {
			wxCommandEvent click_event(wxEVT_BUTTON, GetId());
			click_event.SetEventObject(this);
			ProcessWindowEvent(click_event);
		}
	}

	Refresh();
}

void StartupButton::OnMouseCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(event)) {
	m_pressed = false;
	Refresh();
}
