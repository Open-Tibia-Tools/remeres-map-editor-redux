//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/position_paste.h"
#include "ui/numbertextctrl.h"
#include "util/common.h"

#include <utility>

void BindPositionPaste(NumberTextCtrl* x_field, NumberTextCtrl* y_field, NumberTextCtrl* z_field, std::function<PositionBounds()> bounds, std::function<void(const Position&)> on_pasted) {
	auto handler = [x_field, y_field, z_field, bounds = std::move(bounds), on_pasted = std::move(on_pasted)](wxClipboardTextEvent& event) {
		const PositionBounds limits = bounds ? bounds() : PositionBounds {};
		Position position;
		if (!posFromClipboard(position, limits.width, limits.height)) {
			event.Skip();
			return;
		}
		x_field->SetIntValue(position.x);
		y_field->SetIntValue(position.y);
		z_field->SetIntValue(position.z);
		if (on_pasted) {
			on_pasted(position);
		}
	};
	x_field->Bind(wxEVT_TEXT_PASTE, handler);
	y_field->Bind(wxEVT_TEXT_PASTE, handler);
	z_field->Bind(wxEVT_TEXT_PASTE, handler);
}
