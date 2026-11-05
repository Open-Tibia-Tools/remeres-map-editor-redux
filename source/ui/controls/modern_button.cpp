#include "ui/controls/modern_button.h"

ModernButton::ModernButton(wxWindow* parent, wxWindowID id, const wxString& label, const wxPoint& pos, const wxSize& size, long style) :
	wxControl(parent, id, pos, size, style | wxBORDER_NONE),
	m_animTimer(this) {
	SetLabel(label);
	SetBackgroundStyle(wxBG_STYLE_PAINT);

	Bind(wxEVT_PAINT, &ModernButton::OnPaint, this);
	Bind(wxEVT_ENTER_WINDOW, &ModernButton::OnMouse, this);
	Bind(wxEVT_LEAVE_WINDOW, &ModernButton::OnMouse, this);
	Bind(wxEVT_LEFT_DOWN, &ModernButton::OnMouse, this);
	Bind(wxEVT_LEFT_UP, &ModernButton::OnMouse, this);
	Bind(wxEVT_ERASE_BACKGROUND, &ModernButton::OnEraseBackground, this);
	m_animTimer.Bind(wxEVT_TIMER, &ModernButton::OnTimer, this);
}

void ModernButton::SetBitmap(const wxBitmap& bitmap) {
	m_icon = bitmap;
	InvalidateBestSize();
	Refresh();
}

wxSize ModernButton::DoGetBestClientSize() const {
	wxClientDC dc(const_cast<ModernButton*>(this));
	dc.SetFont(GetFont());
	wxSize text = dc.GetTextExtent(GetLabel());

	int width = text.x + FromDIP(40);
	if (m_icon.IsOk()) {
		width += m_icon.GetWidth() + FromDIP(8);
	}

	int height = std::max(text.y, m_icon.IsOk() ? m_icon.GetHeight() : 0) + FromDIP(20);
	return wxSize(width, height);
}

void ModernButton::DoSetSizeHints(int minW, int minH, int maxW, int maxH, int incW, int incH) {
	wxControl::DoSetSizeHints(minW, minH, maxW, maxH, incW, incH);
}

void ModernButton::OnEraseBackground(wxEraseEvent&) {
	// No-op to prevent flicker
}

void ModernButton::OnPaint(wxPaintEvent&) {
	wxAutoBufferedPaintDC dc(this);
	wxSize clientSize = GetClientSize();

	// Get colors from system settings for theme consistency
	wxColour bg = GetParent()->GetBackgroundColour();
	wxColour base = bg; // Flat background by default
	wxColour hover = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT).ChangeLightness(110);
	wxColour textCol = GetForegroundColour();

	// Interpolate background color (Extremely subtle/fast for "Quiet" feel)
	wxColour currentBg;
	if (m_hoverAlpha <= 0.0f) {
		currentBg = base;
	} else if (m_hoverAlpha >= 1.0f) {
		currentBg = hover;
	} else {
		// Linear interpolation
		int r = base.Red() + (int)((hover.Red() - base.Red()) * m_hoverAlpha);
		int g = base.Green() + (int)((hover.Green() - base.Green()) * m_hoverAlpha);
		int b = base.Blue() + (int)((hover.Blue() - base.Blue()) * m_hoverAlpha);
		currentBg = wxColour(r, g, b);
	}

	dc.SetBrush(wxBrush(currentBg));
	dc.SetPen(*wxTRANSPARENT_PEN); // No border for flat look
	dc.DrawRectangle(GetClientSize());

	// Focus indication (quiet)
	if (HasFocus()) {
		dc.SetBrush(*wxTRANSPARENT_BRUSH);
		dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT), 1, wxPENSTYLE_STIPPLE));
		dc.DrawRectangle(GetClientSize());
	}

	dc.SetFont(GetFont());
	dc.SetTextForeground(textCol);
	wxSize textSize = dc.GetTextExtent(GetLabel());

	int x = FromDIP(10);
	int cy = clientSize.y / 2;

	if (m_icon.IsOk()) {
		int iy = cy - m_icon.GetHeight() / 2;
		dc.DrawBitmap(m_icon, x, iy, true);
		x += m_icon.GetWidth() + FromDIP(8);
	}

	dc.DrawText(GetLabel(), x, cy - textSize.y / 2); // Left-aligned nav style
}

void ModernButton::OnMouse(wxMouseEvent& evt) {
	if (evt.Entering()) {
		m_targetHover = 1.0f;
		if (!m_animTimer.IsRunning()) {
			m_animTimer.Start(16);
		}
	} else if (evt.Leaving()) {
		m_targetHover = 0.0f;
		if (!m_animTimer.IsRunning()) {
			m_animTimer.Start(16);
		}
	} else if (evt.LeftDown()) {
		CaptureMouse();
		Refresh();
	} else if (evt.LeftUp()) {
		if (HasCapture()) {
			ReleaseMouse();
		}
		if (GetClientRect().Contains(evt.GetPosition())) {
			wxCommandEvent event(wxEVT_BUTTON, GetId());
			event.SetEventObject(this);
			ProcessWindowEvent(event);
		}
	}
	evt.Skip();
}

void ModernButton::OnTimer(wxTimerEvent&) {
	const float SPEED = 0.35f; // Punchier transition
	m_hoverAlpha += (m_targetHover - m_hoverAlpha) * SPEED;
	if (std::abs(m_targetHover - m_hoverAlpha) < 0.005f) {
		m_hoverAlpha = m_targetHover;
		m_animTimer.Stop();
	}
	Refresh();
}
