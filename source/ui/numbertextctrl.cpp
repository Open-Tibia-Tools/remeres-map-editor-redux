//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/numbertextctrl.h"
#include "util/number_input.h"
#include <format>

NumberTextCtrl::NumberTextCtrl(
    wxWindow* parent, wxWindowID id, long value, long minvalue, long maxvalue, const wxPoint& pos, const wxSize& sz, long style,
    const wxString& name
) :
    NumberTextCtrl(parent, id, value, minvalue, maxvalue, style, name, pos, sz)
{
}

NumberTextCtrl::NumberTextCtrl(wxWindow* parent, wxWindowID id, long value, long minvalue, long maxvalue, long style, const wxString& name, const wxPoint& pos, const wxSize& sz) :
	wxTextCtrl(parent, id, wxstr(std::format("{}", value)), pos, sz, style, wxTextValidator(wxFILTER_NUMERIC), name),
	minval(minvalue), maxval(maxvalue), lastval(value) {
	Bind(wxEVT_KILL_FOCUS, &NumberTextCtrl::OnKillFocus, this);
    if (HasFlag(wxTE_PROCESS_ENTER)) {
        Bind(wxEVT_TEXT_ENTER, &NumberTextCtrl::OnTextEnter, this);
    }
}

NumberTextCtrl::~NumberTextCtrl() {
	////
}

void NumberTextCtrl::OnKillFocus(wxFocusEvent& evt) {
	CheckRange();
	evt.Skip();
}

void NumberTextCtrl::OnTextEnter(wxCommandEvent& evt) {
	CheckRange();
}

bool NumberTextCtrl::TryBefore(wxEvent& event)
{
    // Bypass wxTextValidator::OnPaste so BindPositionPaste receives the unmodified clipboard
    // text via wxEVT_TEXT_PASTE before the validator strips non-numeric chars and consumes it.
    if (event.GetEventType() == wxEVT_TEXT_PASTE) {
        return wxEvtHandler::TryBefore(event);
    }
    return wxTextCtrl::TryBefore(event);
}

void NumberTextCtrl::SetIntValue(long value) {
	lastval = value;
	wxString sv;
	sv << value;
	// Will generate events
	SetValue(sv);
}

long NumberTextCtrl::GetIntValue() {
	long l;
	if (GetValue().ToLong(&l)) {
		return l;
	}
	return 0;
}

void NumberTextCtrl::SetMinValue(long value) {
	if (value == minval) {
		return;
	}
	minval = value;
	CheckRange();
}

void NumberTextCtrl::SetMaxValue(long value) {
	if (value == maxval) {
		return;
	}
	maxval = value;
	CheckRange();
}

void NumberTextCtrl::CheckRange() {
	const wxString text = GetValue();

	// Text that is not one whole number (for example a pasted position that was not
	// accepted as one) restores the last value instead of gluing its digits together.
	const long v = static_cast<long>(ResolveNumberInput(text.utf8_string(), minval, maxval, lastval));
	lastval = v;

	wxString ntext;
	ntext << v;

	// Check if there was any change
	if (ntext != text) {
		// ChangeValue doesn't generate events
		ChangeValue(ntext);
	}
}
