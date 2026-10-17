#include "palette/controls/palette_search_ctrl.h"
#include "ui/theme.h"

PaletteSearchCtrl::PaletteSearchCtrl(wxWindow* parent, wxWindowID id,
	const wxString& value,
	const wxPoint& pos,
	const wxSize& size,
	long style) :
	wxSearchCtrl(parent, id, value, pos, size, style) {

	ShowCancelButton(true);
	SetFont(Theme::GetFont(9, false));

	// Dismiss/clear on Escape
	Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
		if (event.GetKeyCode() == WXK_ESCAPE) {
			if (!GetValue().empty()) {
				ChangeValue(wxEmptyString);
				wxCommandEvent evt(wxEVT_TEXT, GetId());
				evt.SetEventObject(this);
				evt.SetString(wxEmptyString);
				ProcessWindowEvent(evt);
				return;
			}
		}
		event.Skip();
	});

	// Clear on cancel button click
	Bind(wxEVT_SEARCHCTRL_CANCEL_BTN, [this](wxCommandEvent& /*event*/) {
		ChangeValue(wxEmptyString);
		wxCommandEvent evt(wxEVT_TEXT, GetId());
		evt.SetEventObject(this);
		evt.SetString(wxEmptyString);
		ProcessWindowEvent(evt);
	});

	ApplyTheme();
}

bool PaletteSearchCtrl::IsTopNavigationDomain(NavigationKind kind) const {
	if (kind == Navigation_Accel) {
		return true;
	}
	return wxSearchCtrl::IsTopNavigationDomain(kind);
}

#ifdef __WXMSW__
bool PaletteSearchCtrl::MSWTranslateMessage(WXMSG* /*msg*/) {
	return false;
}

bool PaletteSearchCtrl::MSWShouldPreProcessMessage(WXMSG* /*msg*/) {
	return false;
}
#endif

void PaletteSearchCtrl::ApplyTheme() {
	const wxColour surface = Theme::Get(Theme::Role::Surface);
	const wxColour text = Theme::Get(Theme::Role::Text);

	SetBackgroundColour(surface);
	SetForegroundColour(text);

	for (wxWindow* child : GetChildren()) {
		if (child) {
			child->SetBackgroundColour(surface);
			child->SetForegroundColour(text);
		}
	}
	Refresh();
}
