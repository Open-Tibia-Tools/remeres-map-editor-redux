//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_POSITION_PASTE_H_
#define RME_UI_POSITION_PASTE_H_

#include "map/position_format.h"

#include <functional>

class NumberTextCtrl;

// Fills X, Y and Z at once when a whole position, in any Copy Position format, is pasted
// into one of the three fields. wxEVT_TEXT_PASTE is sent for Ctrl+V, Shift+Insert and the
// context menu Paste of a plain text control on wxMSW (WM_PASTE), wxGTK and wxOSX. Any
// other text is pasted into the focused field as usual. bounds is asked on every paste,
// so it can follow the current map; on_pasted runs after the three fields were filled.
void BindPositionPaste(NumberTextCtrl* x_field, NumberTextCtrl* y_field, NumberTextCtrl* z_field, std::function<PositionBounds()> bounds, std::function<void(const Position&)> on_pasted = {});

#endif
